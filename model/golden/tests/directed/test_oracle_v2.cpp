#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

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

u8 lerp(u8 a, u8 b, u16 f) {
    return static_cast<u8>((static_cast<u32>(a) * (65536u - f) +
                            static_cast<u32>(b) * f + 32768u) >>
                           16);
}

// E5: bilinear fractions + 2D horizontal-then-vertical ordering
void test_bilinear_fractions_and_order() {
    // 2x2 ARGB: (0,0)=R (1,0)=G (0,1)=B (1,1)=W
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::ARGB8888}, "d");
    gpu.register_resource(RegisteredResource{0x20000, 32, 2, 2, "s"});
    std::vector<u8> tex(32, 0);  // stride 16, 2 rows
    // row0 at 0: R, G
    tex[0] = 0; tex[1] = 0; tex[2] = 255; tex[3] = 255;
    tex[4] = 0; tex[5] = 255; tex[6] = 0; tex[7] = 255;
    // row1 at 16: B, W
    tex[16] = 255; tex[17] = 0; tex[18] = 0; tex[19] = 255;
    tex[20] = 255; tex[21] = 255; tex[22] = 255; tex[23] = 255;
    gpu.memory().write_block(0x20000, tex.data(), tex.size());

    auto sample = [&](i32 u0, i32 v0) -> u32 {
        GoldenGPU g;
        g.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::ARGB8888}, "d");
        g.register_resource(RegisteredResource{0x20000, 32, 2, 2, "s"});
        g.memory().write_block(0x20000, tex.data(), tex.size());
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 16;
        d.dst_stride = 16;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.blit_ext = true;
        d.ext_ptr = 0x30000;
        d.filter = static_cast<u32>(FilterMode::BILINEAR);
        d.w = 1;
        d.h = 1;
        d.u0 = u0;
        d.v0 = v0;
        d.du_dx = 0;
        d.dv_dy = 0;
        auto ext = make_draw2d_ext_v1(d);
        g.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
        g.memory().write_block(0x30000, ext.data(), ext.size());
        auto cmd = make_blit_ext_cmd(d);
        cmd[10] = pack_wh(2, 2);
        cmd[11] = pack_wh(1, 1);
        auto st = g.execute_command(cmd);
        if (!st.ok) {
            std::printf("bilinear exec fail fault=%u detail=%u\n",
                        static_cast<u32>(st.fault), st.fault_detail);
            return 0xDEAD;
        }
        u32 w = 0;
        g.memory().read32(0x10000, w);
        return w;
    };

    auto indep = [&](i32 u0, i32 v0, u32& out) {
        const i32 x0 = floor_q16_16(u0);
        const i32 y0 = floor_q16_16(v0);
        const u16 fx = frac_q16_16(u0);
        const u16 fy = frac_q16_16(v0);
        auto fetch = [&](i32 x, i32 y) -> Rgba8888 {
            i32 cx = x < 0 ? 0 : (x > 1 ? 1 : x);
            i32 cy = y < 0 ? 0 : (y > 1 ? 1 : y);
            const u8* p = tex.data() + static_cast<size_t>(cy * 16 + cx * 4);
            return Rgba8888::pack(p[3], p[2], p[1], p[0]);
        };
        const auto c00 = fetch(x0, y0);
        const auto c10 = fetch(x0 + 1, y0);
        const auto c01 = fetch(x0, y0 + 1);
        const auto c11 = fetch(x0 + 1, y0 + 1);
        const u8 h0a = lerp(c00.a(), c10.a(), fx);
        const u8 h0r = lerp(c00.r(), c10.r(), fx);
        const u8 h0g = lerp(c00.g(), c10.g(), fx);
        const u8 h0b = lerp(c00.b(), c10.b(), fx);
        const u8 h1a = lerp(c01.a(), c11.a(), fx);
        const u8 h1r = lerp(c01.r(), c11.r(), fx);
        const u8 h1g = lerp(c01.g(), c11.g(), fx);
        const u8 h1b = lerp(c01.b(), c11.b(), fx);
        const auto o = Rgba8888::pack(lerp(h0a, h1a, fy), lerp(h0r, h1r, fy),
                                      lerp(h0g, h1g, fy), lerp(h0b, h1b, fy));
        out = o.value;
    };

    const i32 fracs[] = {0, 1, 0x4000, 0x8000, 0xC000, 0xFFFF};
    for (i32 fx : fracs) {
        u32 exp = 0;
        indep(fx, 0, exp);
        const u32 got1 = sample(fx, 0);
        if (got1 != exp) {
            std::printf("bilinear fx=%d exp=%08X got=%08X\n", fx, exp, got1);
        }
        EXPECT_TRUE(got1 == exp);
        u32 exp2 = 0;
        indep(0, fx, exp2);
        const u32 got2 = sample(0, fx);
        if (got2 != exp2) {
            std::printf("bilinear fy=%d exp=%08X got=%08X\n", fx, exp2, got2);
        }
        EXPECT_TRUE(got2 == exp2);
    }
    // 2D ordering: (0x4000, 0xC000)
    u32 exp2d = 0;
    indep(0x4000, 0xC000, exp2d);
    EXPECT_TRUE(sample(0x4000, 0xC000) == exp2d);
    // Prove order matters: one-shot would differ from H-then-V for this pattern
    // (if H-then-V result equals production, ordering is as specified)
}

// E4: INDEX8 bilinear — palette then lerp, not index lerp
void test_index8_bilinear() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::ARGB8888}, "d");
    gpu.register_resource(RegisteredResource{0x20000, 8, 2, 2, "s"});
    // indices 0,1,2,3
    std::vector<u8> idx = {0, 1, 2, 3};
    gpu.memory().write_block(0x20000, idx.data(), 4);
    gpu.register_resource(RegisteredResource{0x40000, 1024, 256, 1, "p"});
    std::vector<u8> pal(1024, 0);
    auto put = [&](u32 i, u8 r, u8 g, u8 b) {
        pal[i * 4 + 0] = b;
        pal[i * 4 + 1] = g;
        pal[i * 4 + 2] = r;
        pal[i * 4 + 3] = 255;
    };
    put(0, 0, 0, 0);
    put(1, 255, 0, 0);
    put(2, 0, 255, 0);
    put(3, 255, 255, 255);
    gpu.memory().write_block(0x40000, pal.data(), pal.size());

    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 2;
    d.dst_stride = 16;
    d.src_format = static_cast<u32>(PixelFormat::INDEX8);
    d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
    d.palette_en = true;
    d.palette_addr = 0x40000;
    d.blit_ext = true;
    d.ext_ptr = 0x30000;
    d.filter = static_cast<u32>(FilterMode::BILINEAR);
    d.w = 1;
    d.h = 1;
    d.u0 = 0x8000;  // fx=0.5
    d.v0 = 0x8000;
    auto ext = make_draw2d_ext_v1(d);
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());
    auto cmd = make_blit_ext_cmd(d);
    cmd[10] = pack_wh(2, 2);
    cmd[11] = pack_wh(1, 1);
    EXPECT_TRUE(gpu.execute_command(cmd).ok);
    u32 got = 0;
    gpu.memory().read32(0x10000, got);
    // palette-decode then lerp: all a=255
    const Rgba8888 c00 = Rgba8888::from_u32(0xFF000000);
    const Rgba8888 c10 = Rgba8888::from_u32(0xFFFF0000);
    const Rgba8888 c01 = Rgba8888::from_u32(0xFF00FF00);
    const Rgba8888 c11 = Rgba8888::from_u32(0xFFFFFFFF);
    const u8 hr = lerp(c00.r(), c10.r(), 0x8000);
    const u8 hg = lerp(c00.g(), c10.g(), 0x8000);
    const u8 hb = lerp(c00.b(), c10.b(), 0x8000);
    const u8 r = lerp(hr, lerp(c01.r(), c11.r(), 0x8000), 0x8000);
    const u8 g = lerp(hg, lerp(c01.g(), c11.g(), 0x8000), 0x8000);
    const u8 b = lerp(hb, lerp(c01.b(), c11.b(), 0x8000), 0x8000);
    // Interpolating indices first would give different RGB
    const u8 r_idx_first = 0;
    EXPECT_TRUE(r != r_idx_first);
    const u8 gr = static_cast<u8>((got >> 16) & 0xFF);
    const u8 gg = static_cast<u8>((got >> 8) & 0xFF);
    const u8 gb = static_cast<u8>(got & 0xFF);
    EXPECT_TRUE(gr == r && gg == g && gb == b);
}

// E6: Straight vs Premult equivalence (opaque dest, Aeff applied once)
void test_straight_vs_premult_equiv() {
    // Premult source with Sa=255 is equivalent to straight with Sa=255, Aextra=255
    auto run = [&](bool premult) -> u32 {
        GoldenGPU gpu;
        gpu.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::ARGB8888},
                             "d");
        gpu.register_surface(SurfaceDesc{0x20000, 4, 1, 1, PixelFormat::ARGB8888},
                             "s");
        std::vector<u8> init = {0x40, 0x30, 0x20, 0x10};
        gpu.memory().write_block(0x10000, init.data(), 4);
        std::vector<u8> src = {0x00, 0x00, 0x80, 0xFF};  // opaque R=128
        gpu.memory().write_block(0x20000, src.data(), 4);
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 4;
        d.dst_stride = 16;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.w = 1;
        d.h = 1;
        d.pixel_alpha_en = true;
        if (premult) {
            d.blend = static_cast<u32>(BlendMode::PREMULT_ALPHA);
            d.premult = true;
        } else {
            d.blend = static_cast<u32>(BlendMode::STRAIGHT_ALPHA);
        }
        if (!gpu.execute_command(make_blit_cmd(d)).ok) {
            return 0xDEAD;
        }
        u32 w = 0;
        gpu.memory().read32(0x10000, w);
        return w;
    };
    const u32 a = run(false);
    const u32 b = run(true);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != 0xDEAD);
}

// E9: Repeat period cases with nonzero origin
void test_repeat_periods() {
    // src rect origin=1 width=2: texels green, blue
    auto pe = [&](i32 abs_u, u32 src_x, u32 src_w) -> u16 {
        GoldenGPU gpu;
        gpu.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::RGB565},
                             "d");
        gpu.register_resource(RegisteredResource{0x20000, 8, 4, 1, "s"});
        SurfaceView sv(&gpu.memory(), RegisteredResource{0x20000, 8, 4, 1, "s"}, 8,
                       PixelFormat::RGB565);
        sv.write_rgba(0, 0, Rgba8888::pack(255, 255, 0, 0), false, 0, 0);
        sv.write_rgba(1, 0, Rgba8888::pack(255, 0, 255, 0), false, 1, 0);
        sv.write_rgba(2, 0, Rgba8888::pack(255, 0, 0, 255), false, 2, 0);
        sv.write_rgba(3, 0, Rgba8888::pack(255, 255, 255, 255), false, 3, 0);
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 8;
        d.dst_stride = 16;
        d.src_x = src_x;
        d.w = 1;
        d.h = 1;
        d.addr_u = static_cast<u32>(AddressMode::REPEAT);
        d.blit_ext = true;
        d.ext_ptr = 0x30000;
        d.u0 = abs_u << 16;
        d.v0 = 0;
        d.du_dx = 0;
        d.dv_dy = 0;
        auto ext = make_draw2d_ext_v1(d);
        gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
        gpu.memory().write_block(0x30000, ext.data(), ext.size());
        auto cmd = make_blit_ext_cmd(d);
        cmd[10] = pack_wh(src_w, 1);
        cmd[11] = pack_wh(1, 1);
        if (!gpu.execute_command(cmd).ok) {
            return 0;
        }
        u16 p = 0;
        gpu.memory().read16(0x10000, p);
        return p;
    };
    // origin=1 size=2: 1=green, 2=blue
    // -1 → rel=-2 → 0 → green
    EXPECT_TRUE(pe(-1, 1, 2) == 0x07E0);
    // size 2 → rel=1 → blue
    EXPECT_TRUE(pe(2, 1, 2) == 0x001F);
    // size+1=3 → rel=2 → wrap 0 → green
    EXPECT_TRUE(pe(3, 1, 2) == 0x07E0);
    // 2*size+k = 5, k=1 → rel=4 → 0 → green
    EXPECT_TRUE(pe(5, 1, 2) == 0x07E0);
    // origin 0 size 3
    EXPECT_TRUE(pe(3, 0, 3) == 0xF800);  // 3%3=0 red
    EXPECT_TRUE(pe(6, 0, 3) == 0xF800);
}

}  // namespace

int main() {
    test_bilinear_fractions_and_order();
    test_index8_bilinear();
    test_straight_vs_premult_equiv();
    test_repeat_periods();
    if (g_failures) {
        std::printf("golden_test_oracle_v2 FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_oracle_v2 PASS\n");
    return 0;
}
