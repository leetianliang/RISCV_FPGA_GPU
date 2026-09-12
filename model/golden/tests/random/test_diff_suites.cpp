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

// Independent nearest-scale oracle for RGB565 COPY 1:1-like small scale.
void run_scale_diff(u32 seed) {
    Rng rng(seed);
    const u32 sw = 4, sh = 4, dw = 8, dh = 8;
    const u32 sstride = 8, dstride = 16;
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, dstride, dw, dh, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, sstride, sw, sh, PixelFormat::RGB565}, "s");
    std::vector<u8> tex(sstride * sh);
    for (size_t i = 0; i < tex.size(); i += 2) {
        const u16 px = static_cast<u16>((i * 17 + seed) & 0xFFFF);
        tex[i] = static_cast<u8>(px & 0xFF);
        tex[i + 1] = static_cast<u8>(px >> 8);
    }
    gpu.memory().write_block(0x20000, tex.data(), tex.size());
    std::vector<u8> dinit(dstride * dh, 0);
    gpu.memory().write_block(0x10000, dinit.data(), dinit.size());

    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = sstride;
    d.dst_stride = dstride;
    d.blit_ext = true;
    d.ext_ptr = 0x30000;
    d.w = dw;
    d.h = dh;
    compute_axis_aligned_uv(0, sw, dw, d.u0, d.du_dx);
    compute_axis_aligned_uv_v(0, sh, dh, d.v0, d.dv_dy);
    auto ext = make_draw2d_ext_v1(d);
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());
    auto cmd = make_blit_ext_cmd(d);
    cmd[10] = pack_wh(sw, sh);
    cmd[11] = pack_wh(dw, dh);
    if (!gpu.execute_command(cmd).ok) {
        std::printf("scale exec fail seed=%u\n", seed);
        ++g_failures;
        return;
    }

    // Independent nearest: for each dest, nearest_index(q) with same UV
    std::vector<u8> expect(dstride * dh, 0);
    for (u32 y = 0; y < dh; ++y) {
        for (u32 x = 0; x < dw; ++x) {
            const i64 u = d.u0 + static_cast<i64>(x) * d.du_dx;
            const i64 v = d.v0 + static_cast<i64>(y) * d.dv_dy;
            const i32 nx = nearest_index_q16(static_cast<i32>(u));
            const i32 ny = nearest_index_q16(static_cast<i32>(v));
            // clamp to source rect [0,sw)
            const i32 cx = nx < 0 ? 0 : (nx >= static_cast<i32>(sw) ? static_cast<i32>(sw) - 1 : nx);
            const i32 cy = ny < 0 ? 0 : (ny >= static_cast<i32>(sh) ? static_cast<i32>(sh) - 1 : ny);
            const size_t so = static_cast<size_t>(cy) * sstride + static_cast<size_t>(cx) * 2;
            const size_t dof = static_cast<size_t>(y) * dstride + static_cast<size_t>(x) * 2;
            expect[dof] = tex[so];
            expect[dof + 1] = tex[so + 1];
        }
    }
    std::vector<u8> got;
    gpu.memory().read_block(0x10000, dstride * dh, got);
    if (got != expect) {
        std::printf("scale diff seed=%u first got %02x%02x exp %02x%02x u0=%d du=%d\n",
                    seed, got[0], got[1], expect[0], expect[1], d.u0, d.du_dx);
        ++g_failures;
    }
}

void run_core_alpha_key(u32 seed) {
    Rng rng(seed);
    const u32 w = 8, h = 8, stride = 32;  // ARGB8888 needs stride >= w*4
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, stride, w, h, PixelFormat::ARGB8888}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 8, 2, 2, PixelFormat::ARGB8888}, "s");
    std::vector<u8> init(stride * h, 0);
    for (size_t i = 0; i < init.size(); i += 4) {
        init[i] = 40;
        init[i + 1] = 80;
        init[i + 2] = 120;
        init[i + 3] = 200;
    }
    gpu.memory().write_block(0x10000, init.data(), init.size());
    std::vector<u8> tex = {0, 0, 200, 255,  // red opaque
                           0, 200, 0, 128,  // green half
                           200, 0, 0, 64,   // blue
                           10, 10, 10, 255};
    gpu.memory().write_block(0x20000, tex.data(), tex.size());

    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 8;
    d.dst_stride = stride;
    d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
    d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
    d.w = 2;
    d.h = 2;
    d.dst_x = static_cast<i32>(rng.range(6));
    d.dst_y = static_cast<i32>(rng.range(6));
    const u32 pick = rng.range(3);
    if (pick == 0) {
        d.blend = static_cast<u32>(BlendMode::COPY);
    } else if (pick == 1) {
        d.blend = static_cast<u32>(BlendMode::STRAIGHT_ALPHA);
        d.global_alpha_en = true;
        d.global_alpha = static_cast<u8>(rng.range(256));
        d.pixel_alpha_en = true;
    } else {
        d.color_key_en = true;
        d.color_key_rgb = 0x00C80000; // won't match; still exercise path
    }
    if (!gpu.execute_command(make_blit_cmd(d)).ok) {
        ++g_failures;
    }
}

}  // namespace

int main() {
    for (u32 s = 1; s <= 8; ++s) {
        run_scale_diff(s * 19);
    }
    for (u32 s = 1; s <= 12; ++s) {
        run_core_alpha_key(s * 7 + 3);
    }
    if (g_failures) {
        std::printf("golden_test_diff_suites FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_diff_suites PASS\n");
    return 0;
}
