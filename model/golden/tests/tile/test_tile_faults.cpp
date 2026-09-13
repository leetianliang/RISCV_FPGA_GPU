#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/tile_binner.hpp"

#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;
using namespace golden;

#define EXPECT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d\n", __FILE__, __LINE__);                  \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

GoldenGPU make_gpu(u32 w, u32 h, u32 stride, PixelFormat fmt) {
    GoldenGPU g;
    g.register_surface(SurfaceDesc{0x10000, stride, w, h, fmt}, "d");
    std::vector<u8> init(static_cast<size_t>(stride) * h, 0);
    g.memory().write_block(0x10000, init.data(), init.size());
    return g;
}

void setup_tile(GoldenGPU& g, const std::vector<GpuCmd64>& draws, u32 w, u32 h,
                u32 tile, u32 stride, u32 desc_b, u32 hdr_b, u32 work_b) {
    const auto bin = bin_draws(draws, g.memory(), w, h, tile);
    g.register_resource(RegisteredResource{
        desc_b, static_cast<u32>(draws.size() * 64 + 64), 1, 1, "d"});
    g.register_resource(RegisteredResource{hdr_b, static_cast<u32>(
                                                bin.headers.size() * 16 + 64),
                                            1, 1, "h"});
    g.register_resource(RegisteredResource{
        work_b, static_cast<u32>(bin.workrefs.size() * 4 + 16), 1, 1, "w"});
    for (size_t i = 0; i < draws.size(); ++i) {
        const auto b = serialize_cmd_le(draws[i]);
        g.memory().write_block(desc_b + static_cast<u32>(i) * 64, b.data(), 64);
    }
    for (size_t i = 0; i < bin.headers.size(); ++i) {
        const auto hb = serialize_tile_header(bin.headers[i]);
        g.memory().write_block(hdr_b + static_cast<u32>(i) * 16, hb.data(), 16);
    }
    const auto wr = serialize_workrefs(bin.workrefs);
    if (!wr.empty()) {
        g.memory().write_block(work_b, wr.data(), wr.size());
    }
}

TileFrameCmd base_tf(u32 w, u32 h, u32 tile, u32 stride, PixelFormat fmt, u32 desc_b,
                     u32 hdr_b, u32 work_b) {
    TileFrameCmd tf;
    tf.draw_desc_base = desc_b;
    tf.tile_header_base = hdr_b;
    tf.work_list_base = work_b;
    tf.dst_base = 0x10000;
    tf.dst_stride = stride;
    tf.surface_w = w;
    tf.surface_h = h;
    tf.grid_w = (w + tile - 1) / tile;
    tf.grid_h = (h + tile - 1) / tile;
    tf.tile_w = tile;
    tf.tile_h = tile;
    tf.rt_state = tile_rt_state_store(fmt);
    return tf;
}

void test_reconfig_tile_size() {
    GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
    auto fill = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                   Rgba8888::pack(255, 255, 0, 0));
    setup_tile(g, {fill}, 32, 32, 16, 64, 0x30000, 0x31000, 0x32000);
    auto tf16 = base_tf(32, 32, 16, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
    EXPECT_TRUE(execute_tile_frame(g, make_tile_frame_cmd(tf16)).ok);
    setup_tile(g, {fill}, 32, 32, 32, 64, 0x40000, 0x41000, 0x42000);
    auto tf32 = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x40000, 0x41000, 0x42000);
    EXPECT_TRUE(execute_tile_frame(g, make_tile_frame_cmd(tf32)).ok);
}

void test_grid_mismatch() {
    GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
    auto fill = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                   Rgba8888::pack(255, 1, 2, 3));
    setup_tile(g, {fill}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
    auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
    tf.grid_w = 2;
    const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::BAD_TILE_CONFIG);
}

void test_depth_flag_rejected() {
    GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
    auto fill = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                   Rgba8888::pack(255, 1, 2, 3));
    setup_tile(g, {fill}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
    TileHeader th = {};
    th.work_offset = 0;
    th.work_count = 1;
    th.flags = kTileLoadDepth;
    const auto hb = serialize_tile_header(th);
    g.memory().write_block(0x31000, hb.data(), 16);
    auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
    const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::UNSUPPORTED_FEATURE);
}

void test_dont_load_requires_default() {
    GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
    auto fill = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                   Rgba8888::pack(255, 1, 2, 3));
    setup_tile(g, {fill}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
    TileHeader th = {};
    th.work_offset = 0;
    th.work_count = 1;
    th.flags = kTileDontLoadColor;
    const auto hb = serialize_tile_header(th);
    g.memory().write_block(0x31000, hb.data(), 16);
    auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
    const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::UNSUPPORTED_FEATURE);
    tf.rt_state |= kRtLoadColorDefault;
    EXPECT_TRUE(execute_tile_frame(g, make_tile_frame_cmd(tf)).ok);
}

void test_strict_reserved_pair() {
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        auto fill = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                       Rgba8888::pack(255, 1, 2, 3));
        setup_tile(g, {fill}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        tf.rt_state |= (1u << 9);
        EXPECT_TRUE(execute_tile_frame(g, make_tile_frame_cmd(tf)).ok);
        GpuCmd64 c = make_tile_frame_cmd(tf);
        c[0] |= kHStrict;
        const auto st = execute_tile_frame(g, c);
        EXPECT_TRUE(!st.ok);
        EXPECT_TRUE(st.fault == FaultCode::RESERVED_NONZERO);
    }
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        auto fill = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                       Rgba8888::pack(255, 1, 2, 3));
        setup_tile(g, {fill}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
        u8 w3[4] = {1, 0, 0, 0};
        g.memory().write_block(0x31000 + 12, w3, 4);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        GpuCmd64 c = make_tile_frame_cmd(tf);
        c[0] |= kHStrict;
        const auto st = execute_tile_frame(g, c);
        EXPECT_TRUE(!st.ok);
        EXPECT_TRUE(st.fault == FaultCode::RESERVED_NONZERO);
    }
}

// N6: table-driven exact fault matrix
void test_fault_matrix() {
    auto fill = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                   Rgba8888::pack(255, 1, 2, 3));
    // unmapped desc base
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::MEMORY_ERROR);
    }
    // unmapped header base
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        g.register_resource(RegisteredResource{0x30000, 64, 1, 1, "d"});
        auto b = serialize_cmd_le(fill);
        g.memory().write_block(0x30000, b.data(), 64);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::MEMORY_ERROR);
    }
    // unmapped worklist
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        g.register_resource(RegisteredResource{0x30000, 64, 1, 1, "d"});
        auto b = serialize_cmd_le(fill);
        g.memory().write_block(0x30000, b.data(), 64);
        g.register_resource(RegisteredResource{0x31000, 64, 1, 1, "h"});
        TileHeader th = {};
        th.work_offset = 0;
        th.work_count = 1;
        const auto hb = serialize_tile_header(th);
        g.memory().write_block(0x31000, hb.data(), 16);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::WORKLIST_BOUNDS);
    }
    // workref out of descriptor array
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        g.register_resource(RegisteredResource{0x30000, 64, 1, 1, "d"});
        auto b = serialize_cmd_le(fill);
        g.memory().write_block(0x30000, b.data(), 64);
        g.register_resource(RegisteredResource{0x31000, 64, 1, 1, "h"});
        TileHeader th = {};
        th.work_offset = 0;
        th.work_count = 1;
        const auto hb = serialize_tile_header(th);
        g.memory().write_block(0x31000, hb.data(), 16);
        g.register_resource(RegisteredResource{0x32000, 16, 1, 1, "w"});
        u8 wr[4] = {5, 0, 0, 0};  // index 5, only 1 desc
        g.memory().write_block(0x32000, wr, 4);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::DESCRIPTOR_BOUNDS);
    }
    // misaligned desc base — all other memory otherwise valid → BAD_ALIGNMENT
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        // register at unaligned address so lookup would succeed if we skipped align check
        g.register_resource(RegisteredResource{0x30001, 64, 1, 1, "d"});
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30001, 0x31000, 0x32000);
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::BAD_ALIGNMENT);
    }
    // misaligned header base
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        g.register_resource(RegisteredResource{0x30000, 64, 1, 1, "d"});
        g.register_resource(RegisteredResource{0x31008, 64, 1, 1, "h"});
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31008, 0x32000);
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::BAD_ALIGNMENT);
    }
    // misaligned worklist base
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        g.register_resource(RegisteredResource{0x30000, 64, 1, 1, "d"});
        g.register_resource(RegisteredResource{0x31000, 64, 1, 1, "h"});
        g.register_resource(RegisteredResource{0x32002, 16, 1, 1, "w"});
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32002);
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::BAD_ALIGNMENT);
    }
    // dest allocation too small
    {
        GoldenGPU g;
        g.register_surface(SurfaceDesc{0x10000, 16, 4, 4, PixelFormat::RGB565}, "d");
        std::vector<u8> init(64, 0);
        g.memory().write_block(0x10000, init.data(), init.size());
        auto fill = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                       Rgba8888::pack(255, 1, 2, 3));
        setup_tile(g, {fill}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::BAD_RECT);
    }
    // strict target format mismatch — exact TILE_TARGET_MISMATCH
    {
        GoldenGPU g = make_gpu(32, 32, 128, PixelFormat::ARGB8888);
        auto f = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                    Rgba8888::pack(255, 1, 2, 3));
        setup_tile(g, {f}, 32, 32, 32, 128, 0x30000, 0x31000, 0x32000);
        auto tf = base_tf(32, 32, 32, 128, PixelFormat::ARGB8888, 0x30000, 0x31000, 0x32000);
        tf.rt_state |= kRtStrictTargetMatch;
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::TILE_TARGET_MISMATCH);
    }
    // strict target base mismatch
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        auto f = make_fill_rect_cmd(0x20000, 64, 0, 0, 4, 4,
                                    Rgba8888::pack(255, 1, 2, 3));  // different dest base
        setup_tile(g, {f}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        tf.rt_state |= kRtStrictTargetMatch;
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::TILE_TARGET_MISMATCH);
    }
    // strict target stride mismatch
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        auto f = make_fill_rect_cmd(0x10000, 32, 0, 0, 4, 4,
                                    Rgba8888::pack(255, 1, 2, 3));  // different stride
        setup_tile(g, {f}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        tf.rt_state |= kRtStrictTargetMatch;
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::TILE_TARGET_MISMATCH);
    }
    // Header W3 non-Strict acceptance
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        auto f = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                    Rgba8888::pack(255, 1, 2, 3));
        setup_tile(g, {f}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
        u8 w3[4] = {1, 0, 0, 0};
        g.memory().write_block(0x31000 + 12, w3, 4);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        EXPECT_TRUE(execute_tile_frame(g, make_tile_frame_cmd(tf)).ok);
    }
    // TILE_FLAGS Reserved non-Strict acceptance
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        auto f = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                    Rgba8888::pack(255, 1, 2, 3));
        setup_tile(g, {f}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
        TileHeader th = {};
        th.work_offset = 0;
        th.work_count = 1;
        th.flags = (1u << 4);
        const auto hb = serialize_tile_header(th);
        g.memory().write_block(0x31000, hb.data(), 16);
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        EXPECT_TRUE(execute_tile_frame(g, make_tile_frame_cmd(tf)).ok);
    }
    // WorkList offset/count extending beyond mapped worklist
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        g.register_resource(RegisteredResource{0x30000, 64, 1, 1, "d"});
        auto b = serialize_cmd_le(fill);
        g.memory().write_block(0x30000, b.data(), 64);
        g.register_resource(RegisteredResource{0x31000, 64, 1, 1, "h"});
        TileHeader th = {};
        th.work_offset = 100;  // beyond mapped worklist
        th.work_count = 1;
        const auto hb = serialize_tile_header(th);
        g.memory().write_block(0x31000, hb.data(), 16);
        g.register_resource(RegisteredResource{0x32000, 16, 1, 1, "w"});
        auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(st.fault == FaultCode::WORKLIST_BOUNDS);
    }
    // partial Tile Header array OOB: grid needs 4 headers, only 2 mapped
    {
        GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
        g.register_resource(RegisteredResource{0x30000, 64, 1, 1, "d"});
        auto b = serialize_cmd_le(fill);
        g.memory().write_block(0x30000, b.data(), 64);
        // only 2 of 4 headers (16B each) are mapped
        g.register_resource(RegisteredResource{0x31000, 32, 1, 1, "h"});
        for (int i = 0; i < 2; ++i) {
            TileHeader th = {};
            th.work_offset = 0;
            th.work_count = 1;
            const auto hb = serialize_tile_header(th);
            g.memory().write_block(0x31000 + static_cast<u32>(i) * 16, hb.data(), 16);
        }
        g.register_resource(RegisteredResource{0x32000, 16, 1, 1, "w"});
        u8 wr[4] = {0, 0, 0, 0};
        g.memory().write_block(0x32000, wr, 4);
        auto tf = base_tf(32, 32, 16, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
        const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
        EXPECT_TRUE(!st.ok);
        EXPECT_TRUE(st.fault == FaultCode::MEMORY_ERROR);
    }
}

}  // namespace

int main() {
    test_reconfig_tile_size();
    test_grid_mismatch();
    test_depth_flag_rejected();
    test_dont_load_requires_default();
    test_strict_reserved_pair();
    test_fault_matrix();
    if (g_failures) {
        std::printf("golden_test_tile_faults FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_tile_faults PASS\n");
    return 0;
}
