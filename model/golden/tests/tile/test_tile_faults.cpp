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
    // second frame tile 32 (resize scratch)
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
    tf.grid_w = 1;
    tf.grid_h = 1;  // correct for 32/32
    EXPECT_TRUE(execute_tile_frame(g, make_tile_frame_cmd(tf)).ok);
    tf.grid_w = 2;  // too large
    const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::BAD_TILE_CONFIG);
}

void test_depth_flag_rejected() {
    GoldenGPU g = make_gpu(32, 32, 64, PixelFormat::RGB565);
    auto fill = make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                   Rgba8888::pack(255, 1, 2, 3));
    setup_tile(g, {fill}, 32, 32, 32, 64, 0x30000, 0x31000, 0x32000);
    // set TILE_LOAD_DEPTH in header flags
    {
        TileHeader th = {};
        th.work_offset = 0;
        th.work_count = 1;
        th.flags = kTileLoadDepth;
        const auto hb = serialize_tile_header(th);
        g.memory().write_block(0x31000, hb.data(), 16);
    }
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
    {
        TileHeader th = {};
        th.work_offset = 0;
        th.work_count = 1;
        th.flags = kTileDontLoadColor;  // no CLEAR, no default unless RT bit set
        const auto hb = serialize_tile_header(th);
        g.memory().write_block(0x31000, hb.data(), 16);
    }
    auto tf = base_tf(32, 32, 32, 64, PixelFormat::RGB565, 0x30000, 0x31000, 0x32000);
    // without LOAD_COLOR_DEFAULT → UNSUPPORTED
    const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::UNSUPPORTED_FEATURE);
    // with LOAD_COLOR_DEFAULT → zero-init and succeed
    tf.rt_state |= kRtLoadColorDefault;
    EXPECT_TRUE(execute_tile_frame(g, make_tile_frame_cmd(tf)).ok);
}

void test_strict_reserved_pair() {
    // RT_STATE reserved: non-strict ignore, strict fault
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
    // Header W3 reserved + strict
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

}  // namespace

int main() {
    test_reconfig_tile_size();
    test_grid_mismatch();
    test_depth_flag_rejected();
    test_dont_load_requires_default();
    test_strict_reserved_pair();
    if (g_failures) {
        std::printf("golden_test_tile_faults FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_tile_faults PASS\n");
    return 0;
}
