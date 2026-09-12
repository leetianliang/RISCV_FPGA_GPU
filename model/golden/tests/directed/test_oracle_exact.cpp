#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;
#define EXPECT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d\n", __FILE__, __LINE__);                  \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

using namespace golden;

void test_bilinear_fractions() {
    // 2x1 texture: left=black, right=white; sample at fx via UV
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::ARGB8888}, "d");
    gpu.register_resource(RegisteredResource{0x20000, 8, 2, 1, "s"});
    // pixel0 black, pixel1 white
    std::vector<u8> tex = {0, 0, 0, 255, 255, 255, 255, 255};
    gpu.memory().write_block(0x20000, tex.data(), tex.size());

    auto run_at = [&](i32 u0) -> u8 {
        gpu.reset();
        gpu.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::ARGB8888}, "d");
        gpu.register_resource(RegisteredResource{0x20000, 8, 2, 1, "s"});
        gpu.memory().write_block(0x20000, tex.data(), tex.size());
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 8;
        d.dst_stride = 16;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.blit_ext = true;
        d.ext_ptr = 0x30000;
        d.filter = static_cast<u32>(FilterMode::BILINEAR);
        d.w = 1;
        d.h = 1;
        d.u0 = u0;
        d.v0 = 0;
        d.du_dx = 0;
        d.dv_dy = 0;
        auto ext = make_draw2d_ext_v1(d);
        gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
        gpu.memory().write_block(0x30000, ext.data(), ext.size());
        auto cmd = make_blit_ext_cmd(d);
        cmd[10] = pack_wh(2, 1);
        cmd[11] = pack_wh(1, 1);
        if (!gpu.execute_command(cmd).ok) {
            return 0xFF;
        }
        u32 w = 0;
        gpu.memory().read32(0x10000, w);
        return static_cast<u8>((w >> 16) & 0xFF);  // R
    };

    // u=0 → nearest texel0 black
    EXPECT_TRUE(run_at(0) == 0);
    // u=0.5=0x8000: floor=0, fx=0x8000 → LERP(0,255,0x8000)=128
    EXPECT_TRUE(run_at(0x8000) == 128);
    // u=1.0 → texel1
    EXPECT_TRUE(run_at(65536) == 255);
    // u=0xFFFF ≈ almost 1.0
    EXPECT_TRUE(run_at(0xFFFF) == 255);
}

void test_repeat_clamp() {
    GoldenGPU gpu;
    // source rect at SRC_X=1, size 2 within 4-wide texture
    gpu.register_surface(SurfaceDesc{0x10000, 16, 4, 1, PixelFormat::RGB565}, "d");
    gpu.register_resource(RegisteredResource{0x20000, 8, 4, 1, "s"});
    // colors: t0 red t1 green t2 blue t3 white
    SurfaceView sv(&gpu.memory(), RegisteredResource{0x20000, 8, 4, 1, "s"}, 8,
                   PixelFormat::RGB565);
    sv.write_rgba(0, 0, Rgba8888::pack(255, 255, 0, 0), false, 0, 0);
    sv.write_rgba(1, 0, Rgba8888::pack(255, 0, 255, 0), false, 1, 0);
    sv.write_rgba(2, 0, Rgba8888::pack(255, 0, 0, 255), false, 2, 0);
    sv.write_rgba(3, 0, Rgba8888::pack(255, 255, 255, 255), false, 3, 0);

    auto sample_u = [&](i32 u_abs, AddressMode am, u32 src_x, u32 src_w) -> u16 {
        gpu.reset();
        gpu.register_surface(SurfaceDesc{0x10000, 16, 4, 1, PixelFormat::RGB565}, "d");
        gpu.register_resource(RegisteredResource{0x20000, 8, 4, 1, "s"});
        SurfaceView s2(&gpu.memory(), RegisteredResource{0x20000, 8, 4, 1, "s"}, 8,
                       PixelFormat::RGB565);
        s2.write_rgba(0, 0, Rgba8888::pack(255, 255, 0, 0), false, 0, 0);
        s2.write_rgba(1, 0, Rgba8888::pack(255, 0, 255, 0), false, 1, 0);
        s2.write_rgba(2, 0, Rgba8888::pack(255, 0, 0, 255), false, 2, 0);
        s2.write_rgba(3, 0, Rgba8888::pack(255, 255, 255, 255), false, 3, 0);
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 8;
        d.dst_stride = 16;
        d.src_x = src_x;
        d.w = 1;
        d.h = 1;
        d.addr_u = static_cast<u32>(am);
        d.blit_ext = true;
        d.ext_ptr = 0x30000;
        d.u0 = u_abs << 16;
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
            return 0xFFFF;
        }
        u16 p = 0;
        gpu.memory().read16(0x10000, p);
        return p;
    };

    // src rect [1,3) i.e. green, blue
    // CLAMP u=0 → src_x=1 green
    EXPECT_TRUE(sample_u(0, AddressMode::CLAMP, 1, 2) == 0x07E0);
    // CLAMP u=-1 → green
    EXPECT_TRUE(sample_u(-1, AddressMode::CLAMP, 1, 2) == 0x07E0);
    // CLAMP u=2 → last of rect = blue
    EXPECT_TRUE(sample_u(2, AddressMode::CLAMP, 1, 2) == 0x001F);
    // CLAMP u=5 → blue
    EXPECT_TRUE(sample_u(5, AddressMode::CLAMP, 1, 2) == 0x001F);
    // REPEAT domain is source rect [src_x, src_x+src_w)
    // abs=0 → rel=-1 → last of rect = blue
    EXPECT_TRUE(sample_u(0, AddressMode::REPEAT, 1, 2) == 0x001F);
    // abs=2 → rel=1 → blue
    EXPECT_TRUE(sample_u(2, AddressMode::REPEAT, 1, 2) == 0x001F);
    // abs=3 → rel=2 → wrap to 0 → green
    EXPECT_TRUE(sample_u(3, AddressMode::REPEAT, 1, 2) == 0x07E0);
}

void test_dither_4x4() {
    // Independent Bayer quant for R channel C=128, M=31
    // p=128*31=3968; base=3968/255=15; rem=3968%255=143
    // b=B4[y%4][x%4]
    // q=base+1 if base<31 and rem*32 > (2b+1)*255
    const u8 bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    auto expect_r5 = [&](int x, int y) -> u32 {
        const u32 M = 31;
        const u32 C = 128;
        const u32 p = C * M;
        const u32 base = p / 255;
        const u32 rem = p % 255;
        const u32 b = bayer[y & 3][x & 3];
        if (base < M && rem * 32 > (2 * b + 1) * 255) {
            return base + 1;
        }
        return base;
    };

    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 16, 4, 4, PixelFormat::RGB565}, "d");
    auto cmd = make_fill_rect_cmd(0x10000, 16, 0, 0, 4, 4,
                                  Rgba8888::pack(255, 128, 0, 0));
    cmd[12] |= (1u << 27);  // DITHER
    EXPECT_TRUE(gpu.execute_command(cmd).ok);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            u16 px = 0;
            gpu.memory().read16(0x10000 + y * 16 + x * 2, px);
            const u32 got_r = (px >> 11) & 31;
            EXPECT_TRUE(got_r == expect_r5(x, y));
        }
    }
}

void test_palette_nearest_exact() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 16, 2, 1, PixelFormat::RGB565}, "d");
    gpu.register_resource(RegisteredResource{0x20000, 2, 2, 1, "s"});
    gpu.memory().write_block(0x20000, std::vector<u8>{0, 1}.data(), 2);
    gpu.register_resource(RegisteredResource{0x40000, 1024, 256, 1, "p"});
    // pal[0]=red, pal[1]=blue
    std::vector<u8> pal(1024, 0);
    pal[0 * 4 + 0] = 0;
    pal[0 * 4 + 1] = 0;
    pal[0 * 4 + 2] = 255;
    pal[0 * 4 + 3] = 255;
    pal[1 * 4 + 0] = 255;
    pal[1 * 4 + 1] = 0;
    pal[1 * 4 + 2] = 0;
    pal[1 * 4 + 3] = 255;
    gpu.memory().write_block(0x40000, pal.data(), pal.size());
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 2;
    d.dst_stride = 16;
    d.src_format = static_cast<u32>(PixelFormat::INDEX8);
    d.palette_en = true;
    d.palette_addr = 0x40000;
    d.w = 2;
    d.h = 1;
    EXPECT_TRUE(gpu.execute_command(make_blit_cmd(d)).ok);
    u16 p0 = 0, p1 = 0;
    gpu.memory().read16(0x10000, p0);
    gpu.memory().read16(0x10002, p1);
    EXPECT_TRUE(p0 == 0xF800);
    EXPECT_TRUE(p1 == 0x001F);
}

}  // namespace

int main() {
    test_bilinear_fractions();
    test_repeat_clamp();
    test_dither_4x4();
    test_palette_nearest_exact();
    if (g_failures) {
        std::printf("golden_test_oracle_exact FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_oracle_exact PASS\n");
    return 0;
}
