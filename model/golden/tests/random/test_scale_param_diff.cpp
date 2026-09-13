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

void run_scale(u32 seed, u32 sw, u32 sh, u32 dw, u32 dh, u32 src_x, u32 src_y,
               u32 /*sstride_hint*/, u32 dstride) {
    Rng rng(seed);
    const u32 base_d = 0x10000;
    const u32 base_s = 0x20000;
    const u32 tex_w = src_x + sw + 4;
    const u32 tex_h = src_y + sh + 4;
    const u32 sstride = tex_w * 2;
    GoldenGPU gpu;
    gpu.register_surface(
        SurfaceDesc{base_d, dstride, dw, dh, PixelFormat::RGB565}, "d");
    const auto rs = gpu.register_surface(
        SurfaceDesc{base_s, sstride, tex_w, tex_h, PixelFormat::RGB565}, "s");
    if (!rs.ok) {
        std::printf("scale register src fail seed=%u tex=%ux%u ss=%u\n", seed, tex_w,
                    tex_h, sstride);
        ++g_failures;
        return;
    }

    std::vector<u8> tex(static_cast<size_t>(sstride) * tex_h, 0);
    for (size_t i = 0; i + 1 < tex.size(); i += 2) {
        const u16 px = static_cast<u16>((i * 13u + seed) & 0xFFFF);
        tex[i] = static_cast<u8>(px & 0xFF);
        tex[i + 1] = static_cast<u8>(px >> 8);
    }
    gpu.memory().write_block(base_s, tex.data(), tex.size());
    std::vector<u8> dinit(static_cast<size_t>(dstride) * dh, 0);
    gpu.memory().write_block(base_d, dinit.data(), dinit.size());

    BlitCmdDesc d;
    d.src_base = base_s;
    d.dst_base = base_d;
    d.src_stride = sstride;
    d.dst_stride = dstride;
    d.src_x = src_x;
    d.src_y = src_y;
    d.blit_ext = true;
    d.ext_ptr = 0x30000;
    d.w = dw;
    d.h = dh;
    compute_axis_aligned_uv(src_x, sw, dw, d.u0, d.du_dx);
    compute_axis_aligned_uv_v(src_y, sh, dh, d.v0, d.dv_dy);
    auto ext = make_draw2d_ext_v1(d);
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());
    auto cmd = make_blit_ext_cmd(d);
    cmd[10] = pack_wh(sw, sh);
    cmd[11] = pack_wh(dw, dh);
    const auto st = gpu.execute_command(cmd);
    if (!st.ok) {
        std::printf("scale exec fail seed=%u %u->%u fault=%u detail=%u tex=%ux%u ss=%u\n",
                    seed, sw, dw, static_cast<u32>(st.fault), st.fault_detail, tex_w,
                    tex_h, sstride);
        ++g_failures;
        return;
    }

    std::vector<u8> exp(static_cast<size_t>(dstride) * dh, 0);
    for (u32 y = 0; y < dh; ++y) {
        for (u32 x = 0; x < dw; ++x) {
            const i64 u = d.u0 + static_cast<i64>(x) * d.du_dx;
            const i64 v = d.v0 + static_cast<i64>(y) * d.dv_dy;
            i32 nx = nearest_index_q16(static_cast<i32>(u));
            i32 ny = nearest_index_q16(static_cast<i32>(v));
            // address domain: source rect [src_x, src_x+sw)
            auto map1 = [](i32 abs, i32 origin, i32 sz) -> i32 {
                i32 rel = abs - origin;
                if (rel < 0) rel = 0;
                if (rel >= sz) rel = sz - 1;
                return origin + rel;
            };
            const i32 ax = map1(nx, static_cast<i32>(src_x), static_cast<i32>(sw));
            const i32 ay = map1(ny, static_cast<i32>(src_y), static_cast<i32>(sh));
            const size_t so =
                static_cast<size_t>(ay) * sstride + static_cast<size_t>(ax) * 2;
            const size_t dof =
                static_cast<size_t>(y) * dstride + static_cast<size_t>(x) * 2;
            exp[dof] = tex[so];
            exp[dof + 1] = tex[so + 1];
        }
    }
    std::vector<u8> got;
    gpu.memory().read_block(base_d, static_cast<size_t>(dstride) * dh, got);
    if (got != exp) {
        std::printf("scale pixel mismatch seed=%u sw=%u dw=%u src=%u,%u\n", seed, sw,
                    dw, src_x, src_y);
        ++g_failures;
    }
}

}  // namespace

int main() {
    u32 n = 0;
    // strides cover full registered texture sheet
    run_scale(++n, 1, 1, 4, 4, 0, 0, 16, 16);
    run_scale(++n, 8, 1, 1, 1, 0, 0, 32, 8);
    run_scale(++n, 2, 2, 5, 3, 0, 0, 16, 16);
    run_scale(++n, 7, 5, 3, 2, 0, 0, 32, 16);
    run_scale(++n, 3, 3, 7, 7, 1, 1, 20, 24);
    run_scale(++n, 4, 4, 4, 4, 2, 0, 24, 24);
    run_scale(++n, 5, 5, 2, 9, 0, 1, 20, 20);
    for (u32 i = 0; i < 8; ++i) {
        const u32 sw = 1 + (i % 5);
        const u32 sh = 1 + ((i + 2) % 4);
        const u32 dw = 1 + ((i * 3) % 7);
        const u32 dh = 1 + ((i + 1) % 6);
        const u32 src_x = i % 3;
        const u32 src_y = i % 2;
        const u32 tex_w = src_x + sw + 2;
        const u32 sstride = tex_w * 2;
        run_scale(100 + i * 17, sw, sh, dw, dh, src_x, src_y, sstride,
                  16 + 4 * (i % 4));
    }
    if (g_failures) {
        std::printf("golden_test_scale_param_diff FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_scale_param_diff PASS\n");
    return 0;
}
