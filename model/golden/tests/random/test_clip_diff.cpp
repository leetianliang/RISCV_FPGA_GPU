#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

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

// Independent clip: raster = dest ∩ clip ∩ target; UV from full dest origin.
void run_clip(u32 seed) {
    Rng rng(seed);
    const u32 tw = 12, th = 10, tstride = 24;
    const u32 sw = 6, sh = 5, sstride = 12;
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, tstride, tw, th, PixelFormat::RGB565},
                         "d");
    gpu.register_surface(SurfaceDesc{0x20000, sstride, sw, sh, PixelFormat::RGB565},
                         "s");
    std::vector<u8> tex(sstride * sh, 0);
    for (size_t i = 0; i + 1 < tex.size(); i += 2) {
        const u16 px = static_cast<u16>((i * 7 + seed) & 0xFFFF);
        tex[i] = static_cast<u8>(px & 0xFF);
        tex[i + 1] = static_cast<u8>(px >> 8);
    }
    gpu.memory().write_block(0x20000, tex.data(), tex.size());
    std::vector<u8> dinit(tstride * th, 0x10);
    gpu.memory().write_block(0x10000, dinit.data(), dinit.size());

    const i32 dst_x = static_cast<i32>(rng.range(16)) - 4;  // may be negative
    const i32 dst_y = static_cast<i32>(rng.range(14)) - 4;
    const u32 dw = 4 + rng.range(8);
    const u32 dh = 3 + rng.range(6);
    // clip may be empty / partial / full
    const i32 cx0 = static_cast<i32>(rng.range(12)) - 2;
    const i32 cy0 = static_cast<i32>(rng.range(10)) - 2;
    const i32 cx1 = cx0 + static_cast<i32>(rng.range(10));
    const i32 cy1 = cy0 + static_cast<i32>(rng.range(8));

    const bool scaled = (rng.range(2) != 0) && (dw != sw || dh != sh);
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = sstride;
    d.dst_stride = tstride;
    d.src_x = 0;
    d.src_y = 0;
    d.dst_x = dst_x;
    d.dst_y = dst_y;
    d.w = dw;
    d.h = dh;
    d.blit_ext = true;
    d.ext_ptr = 0x30000;
    d.clip_en = true;
    d.clip_xmin = cx0;
    d.clip_ymin = cy0;
    d.clip_xmax = cx1;
    d.clip_ymax = cy1;
    if (scaled) {
        compute_axis_aligned_uv(0, sw, dw, d.u0, d.du_dx);
        compute_axis_aligned_uv_v(0, sh, dh, d.v0, d.dv_dy);
        auto cmd = make_blit_ext_cmd(d);
        cmd[10] = pack_wh(sw, sh);
        cmd[11] = pack_wh(dw, dh);
        auto ext = make_draw2d_ext_v1(d);
        gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
        gpu.memory().write_block(0x30000, ext.data(), ext.size());
        if (!gpu.execute_command(cmd).ok) {
            std::printf("clip exec fail seed=%u\n", seed);
            ++g_failures;
            return;
        }
    } else {
        d.w = sw;
        d.h = sh;
        d.u0 = 0;
        d.du_dx = 65536;
        d.dv_dy = 65536;
        auto cmd = make_blit_ext_cmd(d);
        auto ext = make_draw2d_ext_v1(d);
        gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
        gpu.memory().write_block(0x30000, ext.data(), ext.size());
        if (!gpu.execute_command(cmd).ok) {
            std::printf("clip exec fail seed=%u\n", seed);
            ++g_failures;
            return;
        }
    }

    const i32 dest_w = scaled ? static_cast<i32>(dw) : static_cast<i32>(sw);
    const i32 dest_h = scaled ? static_cast<i32>(dh) : static_cast<i32>(sh);

    auto raster_max = [&](i32 a, i32 b) { return a > b ? a : b; };
    auto raster_min = [&](i32 a, i32 b) { return a < b ? a : b; };
    i32 rx0 = raster_max(dst_x, cx0);
    i32 ry0 = raster_max(dst_y, cy0);
    i32 rx1 = raster_min(dst_x + dest_w, cx1);
    i32 ry1 = raster_min(dst_y + dest_h, cy1);
    rx0 = raster_max(rx0, 0);
    ry0 = raster_max(ry0, 0);
    rx1 = raster_min(rx1, static_cast<i32>(tw));
    ry1 = raster_min(ry1, static_cast<i32>(th));

    std::vector<u8> exp = dinit;
    for (i32 y = ry0; y < ry1; ++y) {
        for (i32 x = rx0; x < rx1; ++x) {
            const i64 lx = x - dst_x;
            const i64 ly = y - dst_y;
            const i64 u = d.u0 + lx * d.du_dx;
            const i64 v = d.v0 + ly * d.dv_dy;
            i32 nx = nearest_index_q16(static_cast<i32>(u));
            i32 ny = nearest_index_q16(static_cast<i32>(v));
            if (nx < 0) nx = 0;
            if (nx >= static_cast<i32>(sw)) nx = static_cast<i32>(sw) - 1;
            if (ny < 0) ny = 0;
            if (ny >= static_cast<i32>(sh)) ny = static_cast<i32>(sh) - 1;
            const size_t so =
                static_cast<size_t>(ny) * sstride + static_cast<size_t>(nx) * 2;
            const size_t dof =
                static_cast<size_t>(y) * tstride + static_cast<size_t>(x) * 2;
            exp[dof] = tex[so];
            exp[dof + 1] = tex[so + 1];
        }
    }
    std::vector<u8> got;
    gpu.memory().read_block(0x10000, tstride * th, got);
    if (got != exp) {
        std::printf("clip mismatch seed=%u dst=%d,%d scaled=%d dest=%dx%d clip=%d,%d %d,%d\n",
                    seed, dst_x, dst_y, scaled ? 1 : 0, dest_w, dest_h, cx0, cy0, cx1,
                    cy1);
        ++g_failures;
    }
}

}  // namespace

int main() {
    for (u32 i = 0; i < 24; ++i) {
        run_clip(500u + i * 29u);
    }
    if (g_failures) {
        std::printf("golden_test_clip_diff FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_clip_diff PASS\n");
    return 0;
}
