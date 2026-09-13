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

ExecResult run_tile(GoldenGPU& gpu, const std::vector<GpuCmd64>& draws, u32 tw,
                    u32 th, u32 tile, u32 rt_state) {
    const auto bin = bin_draws(draws, gpu.memory(), tw, th, tile);
    const u32 desc_base = 0x30000;
    const u32 hdr_base = 0x34000;
    const u32 work_base = 0x36000;
    gpu.register_resource(RegisteredResource{
        desc_base, static_cast<u32>(draws.size() * 64 + 64), 1, 1, "d"});
    gpu.register_resource(RegisteredResource{
        hdr_base, static_cast<u32>(bin.headers.size() * 16 + 64), 1, 1, "h"});
    gpu.register_resource(RegisteredResource{
        work_base, static_cast<u32>(bin.workrefs.size() * 4 + 16), 1, 1, "w"});
    for (size_t i = 0; i < draws.size(); ++i) {
        const auto b = serialize_cmd_le(draws[i]);
        gpu.memory().write_block(desc_base + static_cast<u32>(i) * 64, b.data(), 64);
    }
    for (size_t i = 0; i < bin.headers.size(); ++i) {
        const auto hb = serialize_tile_header(bin.headers[i]);
        gpu.memory().write_block(hdr_base + static_cast<u32>(i) * 16, hb.data(), 16);
    }
    const auto wr = serialize_workrefs(bin.workrefs);
    if (!wr.empty()) {
        gpu.memory().write_block(work_base, wr.data(), wr.size());
    }
    TileFrameCmd tf;
    tf.draw_desc_base = desc_base;
    tf.tile_header_base = hdr_base;
    tf.work_list_base = work_base;
    tf.dst_base = 0x10000;
    tf.dst_stride = tw * 2;
    tf.surface_w = tw;
    tf.surface_h = th;
    tf.grid_w = (tw + tile - 1) / tile;
    tf.grid_h = (th + tile - 1) / tile;
    tf.tile_w = tile;
    tf.tile_h = tile;
    tf.rt_state = rt_state;
    return execute_tile_frame(gpu, make_tile_frame_cmd(tf));
}

void test_workref_order_via_tile() {
    const u32 stride = 64;
    auto run_imm = [&](const std::vector<GpuCmd64>& draws, std::vector<u8>& fb) {
        GoldenGPU g;
        g.register_surface(SurfaceDesc{0x10000, stride, 32, 32, PixelFormat::RGB565},
                           "d");
        std::vector<u8> init(stride * 32, 0);
        g.memory().write_block(0x10000, init.data(), init.size());
        for (const auto& c : draws) {
            EXPECT_TRUE(g.execute_command(c).ok);
        }
        g.memory().read_block(0x10000, init.size(), fb);
    };
    auto run_tile_fb = [&](const std::vector<GpuCmd64>& draws, bool reverse_wr,
                           std::vector<u8>& fb) {
        GoldenGPU g;
        g.register_surface(SurfaceDesc{0x10000, stride, 32, 32, PixelFormat::RGB565},
                           "d");
        std::vector<u8> init(stride * 32, 0);
        g.memory().write_block(0x10000, init.data(), init.size());
        auto bin = bin_draws(draws, g.memory(), 32, 32, 32);
        if (reverse_wr) {
            // reverse each tile's workrefs
            for (auto& h : bin.headers) {
                u32 off = h.work_offset;
                u32 n = h.work_count;
                for (u32 i = 0; i < n / 2; ++i) {
                    std::swap(bin.workrefs[off + i], bin.workrefs[off + n - 1 - i]);
                }
            }
        }
        const u32 desc_base = 0x30000, hdr_base = 0x34000, work_base = 0x36000;
        g.register_resource(RegisteredResource{
            desc_base, static_cast<u32>(draws.size() * 64), 1, 1, "d"});
        g.register_resource(RegisteredResource{
            hdr_base, static_cast<u32>(bin.headers.size() * 16), 1, 1, "h"});
        g.register_resource(RegisteredResource{
            work_base, static_cast<u32>(bin.workrefs.size() * 4), 1, 1, "w"});
        for (size_t i = 0; i < draws.size(); ++i) {
            const auto b = serialize_cmd_le(draws[i]);
            g.memory().write_block(desc_base + static_cast<u32>(i) * 64, b.data(), 64);
        }
        for (size_t i = 0; i < bin.headers.size(); ++i) {
            const auto hb = serialize_tile_header(bin.headers[i]);
            g.memory().write_block(hdr_base + static_cast<u32>(i) * 16, hb.data(), 16);
        }
        const auto wr = serialize_workrefs(bin.workrefs);
        g.memory().write_block(work_base, wr.data(), wr.size());
        TileFrameCmd tf;
        tf.draw_desc_base = desc_base;
        tf.tile_header_base = hdr_base;
        tf.work_list_base = work_base;
        tf.dst_base = 0x10000;
        tf.dst_stride = stride;
        tf.surface_w = 32;
        tf.surface_h = 32;
        tf.grid_w = 1;
        tf.grid_h = 1;
        tf.tile_w = 32;
        tf.tile_h = 32;
        tf.rt_state = static_cast<u32>(PixelFormat::RGB565);
        EXPECT_TRUE(execute_tile_frame(g, make_tile_frame_cmd(tf)).ok);
        g.memory().read_block(0x10000, init.size(), fb);
    };

    auto white = make_fill_rect_cmd(0x10000, stride, 0, 0, 32, 32,
                                    Rgba8888::pack(255, 255, 255, 255));
    auto halfred = make_fill_rect_cmd(0x10000, stride, 0, 0, 32, 32,
                                      Rgba8888::pack(128, 255, 0, 0));
    u32 ds = halfred[12];
    ds = (ds & ~(0xFu << 8)) | (static_cast<u32>(BlendMode::STRAIGHT_ALPHA) << 8);
    ds |= (1u << 20);
    halfred[12] = ds;
    halfred[14] = (128u << 24);
    std::vector<GpuCmd64> draws = {white, halfred};
    std::vector<u8> imm, t_ok, t_rev;
    run_imm(draws, imm);
    run_tile_fb(draws, false, t_ok);
    run_tile_fb(draws, true, t_rev);
    EXPECT_TRUE(imm == t_ok);
    u32 mism = 0;
    for (size_t i = 0; i < t_ok.size(); ++i) {
        if (t_ok[i] != t_rev[i]) ++mism;
    }
    EXPECT_TRUE(mism > 0);
}

void test_strict_target_match() {
    GoldenGPU g;
    const u32 stride = 64;
    g.register_surface(SurfaceDesc{0x10000, stride, 32, 32, PixelFormat::RGB565},
                       "d");
    std::vector<u8> init(stride * 32, 0);
    g.memory().write_block(0x10000, init.data(), init.size());
    auto fill = make_fill_rect_cmd(0x10000, stride, 0, 0, 4, 4,
                                   Rgba8888::pack(255, 1, 2, 3));
    std::vector<GpuCmd64> draws = {fill};
    const auto bin = bin_draws(draws, g.memory(), 32, 32, 32);
    const u32 desc_base = 0x30000, hdr_base = 0x34000, work_base = 0x36000;
    g.register_resource(RegisteredResource{desc_base, 64, 1, 1, "d"});
    g.register_resource(RegisteredResource{hdr_base, 64, 1, 1, "h"});
    g.register_resource(RegisteredResource{work_base, 16, 1, 1, "w"});
    const auto b = serialize_cmd_le(fill);
    g.memory().write_block(desc_base, b.data(), 64);
    for (size_t i = 0; i < bin.headers.size(); ++i) {
        const auto hb = serialize_tile_header(bin.headers[i]);
        g.memory().write_block(hdr_base + static_cast<u32>(i) * 16, hb.data(), 16);
    }
    const auto wr = serialize_workrefs(bin.workrefs);
    if (!wr.empty()) g.memory().write_block(work_base, wr.data(), wr.size());
    TileFrameCmd tf;
    tf.draw_desc_base = desc_base;
    tf.tile_header_base = hdr_base;
    tf.work_list_base = work_base;
    tf.dst_base = 0x10000;
    tf.dst_stride = stride;
    tf.surface_w = 32;
    tf.surface_h = 32;
    tf.grid_w = 1;
    tf.grid_h = 1;
    tf.tile_w = 32;
    tf.tile_h = 32;
    // mismatch: TILE format ARGB, desc is RGB565, strict
    tf.rt_state = static_cast<u32>(PixelFormat::ARGB8888) | kRtStrictTargetMatch |
                  kRtStoreColor;
    // dest resource is RGB565; strict mismatch on format
    const auto st = execute_tile_frame(g, make_tile_frame_cmd(tf));
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::TILE_TARGET_MISMATCH);
}

}  // namespace

int main() {
    test_workref_order_via_tile();
    test_strict_target_match();
    if (g_failures) {
        std::printf("golden_test_tile_ext FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_tile_ext PASS\n");
    return 0;
}
