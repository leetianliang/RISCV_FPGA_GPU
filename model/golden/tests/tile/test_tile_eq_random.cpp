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
    const u32 desc_base = 0x38000;
    const u32 hdr_base = 0x3A000;
    const u32 work_base = 0x3C000;
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
    tf.rt_state = tile_rt_state_store(PixelFormat::RGB565);
    return execute_tile_frame(gpu, make_tile_frame_cmd(tf));
}

std::vector<GpuCmd64> random_draws(u32 seed, u32 tw, u32 th) {
    Rng rng(seed);
    std::vector<GpuCmd64> draws;
    const int n = 4 + static_cast<int>(rng.range(8));
    for (int i = 0; i < n; ++i) {
        const u32 kind = rng.range(6);  // FILL / FILL-α / FILL-add / BLIT_EXT nearest / BLIT_EXT bilinear / BLIT+key
        if (kind < 3) {
            const auto color = Rgba8888::pack(
                rng.range(2) ? 255 : static_cast<u32>(1 + rng.range(254)),
                static_cast<u8>(rng.range(256)), static_cast<u8>(rng.range(256)),
                static_cast<u8>(rng.range(256)));
            // also clamp fill dest
            const u32 x = rng.range(tw);
            const u32 y = rng.range(th);
            u32 w = 1 + rng.range(24);
            u32 h = 1 + rng.range(24);
            if (x + w > tw) w = tw - x;
            if (y + h > th) h = th - y;
            if (w == 0) w = 1;
            if (h == 0) h = 1;
            auto cmd = make_fill_rect_cmd(0x10000, tw * 2, static_cast<i32>(x),
                                          static_cast<i32>(y), w, h, color);
            if (kind == 1) {
                u32 ds = cmd[12];
                ds = (ds & ~(0xFu << 8)) |
                     (static_cast<u32>(BlendMode::STRAIGHT_ALPHA) << 8);
                ds |= (1u << 20);
                cmd[12] = ds;
                cmd[14] = (static_cast<u32>(rng.range(256)) << 24);
            } else if (kind == 2) {
                u32 ds = cmd[12];
                ds = (ds & ~(0xFu << 8)) |
                     (static_cast<u32>(BlendMode::ADD_SAT) << 8);
                cmd[12] = ds;
            }
            draws.push_back(cmd);
        } else if (kind == 3) {
            // BLIT_EXT nearest scale 4→8
            BlitCmdDesc d;
            d.src_base = 0x20000;
            d.dst_base = 0x10000;
            d.src_stride = 16;
            d.dst_stride = tw * 2;
            d.blit_ext = true;
            d.ext_ptr = 0x30000;
            d.w = 8;
            d.h = 8;
            d.dst_x = static_cast<i32>(rng.range(tw - 8 + 1));
            d.dst_y = static_cast<i32>(rng.range(th - 8 + 1));
            compute_axis_aligned_uv(0, 4, 8, d.u0, d.du_dx);
            compute_axis_aligned_uv_v(0, 4, 8, d.v0, d.dv_dy);
            auto cmd = make_blit_ext_cmd(d);
            cmd[10] = pack_wh(4, 4);
            cmd[11] = pack_wh(8, 8);
            draws.push_back(cmd);
        } else if (kind == 4) {
            // BLIT_EXT bilinear
            BlitCmdDesc d;
            d.src_base = 0x20000;
            d.dst_base = 0x10000;
            d.src_stride = 16;
            d.dst_stride = tw * 2;
            d.blit_ext = true;
            d.ext_ptr = 0x30000;
            d.filter = static_cast<u32>(FilterMode::BILINEAR);
            d.w = 6;
            d.h = 6;
            d.dst_x = static_cast<i32>(rng.range(tw - 6 + 1));
            d.dst_y = static_cast<i32>(rng.range(th - 6 + 1));
            compute_axis_aligned_uv(0, 4, 6, d.u0, d.du_dx);
            compute_axis_aligned_uv_v(0, 4, 6, d.v0, d.dv_dy);
            auto cmd = make_blit_ext_cmd(d);
            cmd[10] = pack_wh(4, 4);
            cmd[11] = pack_wh(6, 6);
            draws.push_back(cmd);
        } else {
            // small RGB565 blit from a 8x8 sheet at 0x20000
            BlitCmdDesc d;
            d.src_base = 0x20000;
            d.dst_base = 0x10000;
            d.src_stride = 16;
            d.dst_stride = tw * 2;
            d.src_x = rng.range(4);
            d.src_y = rng.range(4);
            d.w = 1 + rng.range(8);
            d.h = 1 + rng.range(8);
            if (d.src_x + d.w > 8) d.w = 8 - d.src_x;
            if (d.src_y + d.h > 8) d.h = 8 - d.src_y;
            if (d.w == 0) d.w = 1;
            if (d.h == 0) d.h = 1;
            d.dst_x = static_cast<i32>(rng.range(tw > d.w ? tw - d.w + 1 : 1));
            d.dst_y = static_cast<i32>(rng.range(th > d.h ? th - d.h + 1 : 1));
            if (rng.range(2)) {
                d.color_key_en = true;
                d.color_key_rgb = 0x001C00;
            }
            draws.push_back(make_blit_cmd(d));
        }
    }
    return draws;
}

void run_frame(u32 seed, u32 tile) {
    const u32 tw = 48, th = 40;
    const u32 stride = tw * 2;
    GoldenGPU imm;
    imm.register_surface(SurfaceDesc{0x10000, stride, tw, th, PixelFormat::RGB565},
                         "d");
    imm.register_surface(SurfaceDesc{0x20000, 16, 8, 8, PixelFormat::RGB565}, "s");
    std::vector<u8> init(static_cast<size_t>(stride) * th, 0);
    std::vector<u8> tex(16 * 8);
    Rng tr(seed ^ 0xA5A5u);
    for (size_t i = 0; i + 1 < tex.size(); i += 2) {
        const u16 px = static_cast<u16>(tr.next() & 0xFFFF);
        tex[i] = static_cast<u8>(px & 0xFF);
        tex[i + 1] = static_cast<u8>(px >> 8);
    }
    imm.memory().write_block(0x10000, init.data(), init.size());
    imm.memory().write_block(0x20000, tex.data(), tex.size());
    imm.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    {
        BlitCmdDesc ed;
        auto ext = make_draw2d_ext_v1(ed);
        imm.memory().write_block(0x30000, ext.data(), 64);
    }
    const auto draws = random_draws(seed, tw, th);
    for (const auto& c : draws) {
        const auto st = imm.execute_command(c);
        if (!st.ok) {
            std::printf("imm fail seed=%u fault=%u\n", seed, static_cast<u32>(st.fault));
            ++g_failures;
            return;
        }
    }
    std::vector<u8> fb_imm;
    imm.memory().read_block(0x10000, init.size(), fb_imm);

    GoldenGPU tileg;
    tileg.register_surface(SurfaceDesc{0x10000, stride, tw, th, PixelFormat::RGB565},
                           "d");
    tileg.register_surface(SurfaceDesc{0x20000, 16, 8, 8, PixelFormat::RGB565}, "s");
    tileg.memory().write_block(0x10000, init.data(), init.size());
    tileg.memory().write_block(0x20000, tex.data(), tex.size());
    tileg.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    {
        BlitCmdDesc ed;
        auto ext = make_draw2d_ext_v1(ed);
        tileg.memory().write_block(0x30000, ext.data(), 64);
    }
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
