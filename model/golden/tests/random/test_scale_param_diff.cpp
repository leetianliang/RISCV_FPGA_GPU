#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

#include <cstdio>
#include <limits>
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

// Test-local formulas from Pixel Arithmetic V0.1 (do not call production helpers).
i64 t_round_div(i64 num, i64 den) {
    const bool neg = num < 0;
    u64 unneg;
    if (num == std::numeric_limits<i64>::min()) {
        unneg = static_cast<u64>(std::numeric_limits<i64>::max()) + 1ull;
    } else {
        unneg = static_cast<u64>(neg ? -num : num);
    }
    const u64 uden = static_cast<u64>(den);
    u64 q = unneg / uden;
    const u64 r = unneg % uden;
    if (r * 2ull >= uden) {
        q += 1ull;
    }
    if (!neg) {
        return static_cast<i64>(q);
    }
    if (q > static_cast<u64>(std::numeric_limits<i64>::max())) {
        return std::numeric_limits<i64>::min();
    }
    return -static_cast<i64>(q);
}

void t_uv(u32 src, u32 src_size, u32 dst_size, i32& u0, i32& du) {
    du = static_cast<i32>(t_round_div(static_cast<i64>(src_size) * 65536, dst_size));
    u0 = static_cast<i32>((static_cast<i64>(src) << 16) +
                          t_round_div(static_cast<i64>(src_size - dst_size) * 32768,
                                      dst_size));
}

i32 t_nearest(i32 q) {
    const i64 t = static_cast<i64>(q) + 32768;
    if (t >= 0) {
        return static_cast<i32>(t / 65536);
    }
    return -static_cast<i32>(((-t) + 65535) / 65536);
}

void run_scale(u32 seed, u32 sw, u32 sh, u32 dw, u32 dh, u32 src_x, u32 src_y,
               u32 sstride_pad, u32 dstride) {
    const u32 base_d = 0x10000;
    const u32 base_s = 0x20000;
    const u32 tex_w = src_x + sw + 4;
    const u32 tex_h = src_y + sh + 4;
    const u32 sstride = tex_w * 2 + sstride_pad;  // may pad
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

    i32 u0 = 0, du = 0, v0 = 0, dv = 0;
    t_uv(src_x, sw, dw, u0, du);
    t_uv(src_y, sh, dh, v0, dv);

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
    d.u0 = u0;
    d.du_dx = du;
    d.v0 = v0;
    d.dv_dy = dv;
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
            const i64 u = u0 + static_cast<i64>(x) * du;
            const i64 v = v0 + static_cast<i64>(y) * dv;
            i32 nx = t_nearest(static_cast<i32>(u));
            i32 ny = t_nearest(static_cast<i32>(v));
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
        std::printf("scale pixel mismatch seed=%u sw=%u dw=%u src=%u,%u pad=%u\n", seed,
                    sw, dw, src_x, src_y, sstride_pad);
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
        const u32 pad = (i % 2) ? 4 : 0;  // padded source stride
        run_scale(100 + i * 17, sw, sh, dw, dh, src_x, src_y, pad,
                  16 + 4 * (i % 4));
    }
    if (g_failures) {
        std::printf("golden_test_scale_param_diff FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_scale_param_diff PASS\n");
    return 0;
}
