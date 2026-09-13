#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/tile_binner.hpp"

#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;
using namespace golden;

struct Rng {
    u32 s;
    explicit Rng(u32 seed) : s(seed) {}
    u32 next() {
        s = s * 1664525u + 1013904223u;
        return s;
    }
    u32 range(u32 n) { return n ? next() % n : 0; }
};

ExecResult run_tile_path(GoldenGPU& gpu, const std::vector<GpuCmd64>& draws,
                         u32 tw, u32 th, u32 tile) {
    const auto bin = bin_draws(draws, gpu.memory(), tw, th, tile);
    const u32 desc_base = 0x30000;
    const u32 hdr_base = 0x34000;
    const u32 work_base = 0x36000;
    gpu.register_resource(RegisteredResource{
        desc_base, static_cast<u32>(draws.size() * 64 + 64), 1, 1, "d"});
    gpu.register_resource(
        RegisteredResource{hdr_base, static_cast<u32>(bin.headers.size() * 16 + 64), 1,
                           1, "h"});
    const u32 wsize = static_cast<u32>(bin.workrefs.size() * 4 + 16);
    gpu.register_resource(RegisteredResource{work_base, wsize, 1, 1, "w"});
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
    tf.rt_state = static_cast<u32>(PixelFormat::RGB565);
    return execute_tile_frame(gpu, make_tile_frame_cmd(tf),
                              static_cast<u32>(draws.size()));
}

std::vector<GpuCmd64> random_draws(u32 seed, u32 tw, u32 th) {
    Rng rng(seed);
    std::vector<GpuCmd64> draws;
    const int n = 4 + static_cast<int>(rng.range(8));
    for (int i = 0; i < n; ++i) {
        const auto color = Rgba8888::pack(
            rng.range(2) ? 255 : static_cast<u32>(1 + rng.range(254)),
            static_cast<u8>(rng.range(256)), static_cast<u8>(rng.range(256)),
            static_cast<u8>(rng.range(256)));
        const u32 w = 1 + rng.range(24);
        const u32 h = 1 + rng.range(24);
        const u32 x = rng.range(tw);
        const u32 y = rng.range(th);
        auto cmd = make_fill_rect_cmd(0x10000, tw * 2, static_cast<i32>(x),
                                      static_cast<i32>(y), w, h, color);
        if (rng.range(3) == 0) {
            u32 ds = cmd[12];
            ds = (ds & ~(0xFu << 8)) |
                 (static_cast<u32>(BlendMode::STRAIGHT_ALPHA) << 8);
            cmd[12] = ds;
            cmd[14] = (static_cast<u32>(rng.range(256)) << 24);
        }
        draws.push_back(cmd);
    }
    return draws;
}

void run_frame(u32 seed, u32 tile) {
    const u32 tw = 48, th = 40;
    GoldenGPU imm;
    imm.register_surface(SurfaceDesc{0x10000, tw * 2, tw, th, PixelFormat::RGB565},
                         "d");
    std::vector<u8> init(static_cast<size_t>(tw * 2) * th, 0);
    imm.memory().write_block(0x10000, init.data(), init.size());
    const auto draws = random_draws(seed, tw, th);
    for (const auto& c : draws) {
        const auto st = imm.execute_command(c);
        if (!st.ok) {
            std::printf("imm fail seed=%u\n", seed);
            ++g_failures;
            return;
        }
    }
    std::vector<u8> fb_imm;
    imm.memory().read_block(0x10000, init.size(), fb_imm);

    GoldenGPU tileg;
    tileg.register_surface(SurfaceDesc{0x10000, tw * 2, tw, th, PixelFormat::RGB565},
                           "d");
    tileg.memory().write_block(0x10000, init.data(), init.size());
    const auto st = run_tile_path(tileg, draws, tw, th, tile);
    if (!st.ok) {
        std::printf("tile fail seed=%u tile=%u fault=%u\n", seed, tile,
                    static_cast<u32>(st.fault));
        ++g_failures;
        return;
    }
    std::vector<u8> fb_tile;
    tileg.memory().read_block(0x10000, init.size(), fb_tile);
    if (fb_imm != fb_tile) {
        std::printf("eq mismatch seed=%u tile=%u\n", seed, tile);
        ++g_failures;
    }
}

}  // namespace

int main() {
    // ≥100 frames at each tile size (small frames for speed)
    for (u32 i = 0; i < 100; ++i) {
        run_frame(1000 + i * 13, 16);
    }
    for (u32 i = 0; i < 100; ++i) {
        run_frame(5000 + i * 13, 32);
    }
    for (u32 i = 0; i < 100; ++i) {
        run_frame(9000 + i * 13, 64);
    }
    if (g_failures) {
        std::printf("golden_test_tile_eq_random FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_tile_eq_random PASS (300 frames)\n");
    return 0;
}
