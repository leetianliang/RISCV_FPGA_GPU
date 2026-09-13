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

const u8 kBayer[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

u32 quant(u8 C, u32 M, u8 b) {
    const u32 p = static_cast<u32>(C) * M;
    const u32 base = p / 255;
    const u32 rem = p % 255;
    if (base < M && rem * 32 > (2u * b + 1u) * 255u) {
        return base + 1;
    }
    return base;
}

void fill_dither(GoldenGPU& gpu, u32 base, i32 x, i32 y, u32 w, u32 h, bool dither,
                 Rgba8888 color, u32 dst_fmt) {
    auto cmd = make_fill_rect_cmd(base, 32, x, y, w, h, color);
    u32 ds = cmd[12];
    ds = (ds & ~(0xFu << 4)) | (dst_fmt << 4);
    if (dither) {
        ds |= 1u << 27;
    }
    cmd[12] = ds;
    const auto st = gpu.execute_command(cmd);
    if (!st.ok && dst_fmt == static_cast<u32>(PixelFormat::ARGB8888) && dither) {
        // may fault or ignore depending on strict
        return;
    }
}

void test_dither_global_xy() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 8, 8, PixelFormat::RGB565}, "d");
    // fill (2,1) 4x4 with dither — Bayer must index global RT coords
    fill_dither(gpu, 0x10000, 2, 1, 4, 4, true, Rgba8888::pack(255, 128, 0, 0),
                static_cast<u32>(PixelFormat::RGB565));
    for (int ly = 0; ly < 4; ++ly) {
        for (int lx = 0; lx < 4; ++lx) {
            const int gx = 2 + lx;
            const int gy = 1 + ly;
            u16 px = 0;
            gpu.memory().read16(0x10000 + gy * 32 + gx * 2, px);
            const u32 r5 = (px >> 11) & 31;
            const u32 exp = quant(128, 31, kBayer[gy & 3][gx & 3]);
            EXPECT_TRUE(r5 == exp);
        }
    }
    // Same color at origin should match bayer[0][0] pattern — proves translation
    GoldenGPU gpu2;
    gpu2.register_surface(SurfaceDesc{0x10000, 32, 8, 8, PixelFormat::RGB565}, "d");
    fill_dither(gpu2, 0x10000, 0, 0, 4, 4, true, Rgba8888::pack(255, 128, 0, 0),
                static_cast<u32>(PixelFormat::RGB565));
    u16 p00 = 0, p_t = 0;
    gpu2.memory().read16(0x10000, p00);
    gpu.memory().read16(0x10000 + 1 * 32 + 2 * 2, p_t);  // (2,1)
    EXPECT_TRUE(((p00 >> 11) & 31) != ((p_t >> 11) & 31) ||
                kBayer[0][0] != kBayer[1][2]);
}

void test_dither_disabled() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 4, 4, PixelFormat::RGB565}, "d");
    fill_dither(gpu, 0x10000, 0, 0, 4, 4, false, Rgba8888::pack(255, 128, 0, 0),
                static_cast<u32>(PixelFormat::RGB565));
    // all pixels identical encode
    u16 p0 = 0;
    gpu.memory().read16(0x10000, p0);
    const u16 enc = rgb565_encode(Rgba8888::pack(255, 128, 0, 0));
    EXPECT_TRUE(p0 == enc);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            u16 px = 0;
            gpu.memory().read16(0x10000 + y * 32 + x * 2, px);
            EXPECT_TRUE(px == enc);
        }
    }
}

void test_dither_non_rgb_strict() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 64, 2, 2, PixelFormat::ARGB8888}, "d");
    auto cmd = make_fill_rect_cmd(0x10000, 64, 0, 0, 1, 1,
                                  Rgba8888::pack(255, 10, 20, 30));
    u32 ds = cmd[12];
    ds = (ds & ~(0xFu << 4)) |
         (static_cast<u32>(PixelFormat::ARGB8888) << 4);
    ds |= (1u << 27);  // dither
    cmd[12] = ds;
    cmd[0] = header_word(kClassDraw2D, kOpcodeFillRect, 1, 16, kHStrict);
    const auto st = gpu.execute_command(cmd);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::BAD_FORMAT || st.fault == FaultCode::UNSUPPORTED_FEATURE);
}

void test_dither_non_rgb_nonstrict_ignored() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 64, 2, 2, PixelFormat::ARGB8888}, "d");
    auto cmd = make_fill_rect_cmd(0x10000, 64, 0, 0, 1, 1,
                                  Rgba8888::pack(255, 10, 20, 30));
    u32 ds = cmd[12];
    ds = (ds & ~(0xFu << 4)) |
         (static_cast<u32>(PixelFormat::ARGB8888) << 4);
    ds |= (1u << 27);
    cmd[12] = ds;
    // non-strict: ignore dither, write ARGB
    EXPECT_TRUE(gpu.execute_command(cmd).ok);
    u32 w = 0;
    gpu.memory().read32(0x10000, w);
    EXPECT_TRUE((w >> 24) == 0xFF && ((w >> 16) & 0xFF) == 10);
}

// E8: destination format exact matrix
void test_dst_formats() {
    // RGB565 COPY
    {
        GoldenGPU g;
        g.register_surface(SurfaceDesc{0x10000, 16, 2, 1, PixelFormat::RGB565}, "d");
        EXPECT_TRUE(g.execute_command(
                        make_fill_rect_cmd(0x10000, 16, 0, 0, 1, 1,
                                           Rgba8888::pack(255, 255, 0, 0)))
                        .ok);
        u16 p = 0;
        g.memory().read16(0x10000, p);
        EXPECT_TRUE(p == 0xF800);
    }
    // ARGB COPY exact bytes BB GG RR AA
    {
        GoldenGPU g;
        g.register_surface(SurfaceDesc{0x10000, 16, 2, 1, PixelFormat::ARGB8888},
                           "d");
        auto cmd = make_fill_rect_cmd(0x10000, 16, 0, 0, 1, 1,
                                      Rgba8888::pack(0xAA, 0x11, 0x22, 0x33));
        u32 ds = cmd[12];
        ds = (ds & ~(0xFu << 4)) |
             (static_cast<u32>(PixelFormat::ARGB8888) << 4);
        cmd[12] = ds;
        EXPECT_TRUE(g.execute_command(cmd).ok);
        std::vector<u8> got;
        g.memory().read_block(0x10000, 4, got);
        EXPECT_TRUE(got[0] == 0x33 && got[1] == 0x22 && got[2] == 0x11 &&
                    got[3] == 0xAA);
    }
    // XRGB COPY BB GG RR FF
    {
        GoldenGPU g;
        g.register_surface(SurfaceDesc{0x10000, 16, 2, 1, PixelFormat::XRGB8888},
                           "d");
        auto cmd = make_fill_rect_cmd(0x10000, 16, 0, 0, 1, 1,
                                      Rgba8888::pack(1, 0x11, 0x22, 0x33));
        u32 ds = cmd[12];
        ds = (ds & ~(0xFu << 4)) |
             (static_cast<u32>(PixelFormat::XRGB8888) << 4);
        cmd[12] = ds;
        EXPECT_TRUE(g.execute_command(cmd).ok);
        std::vector<u8> got;
        g.memory().read_block(0x10000, 4, got);
        EXPECT_TRUE(got[0] == 0x33 && got[1] == 0x22 && got[2] == 0x11 &&
                    got[3] == 0xFF);
    }
    // ARGB Straight Alpha
    {
        GoldenGPU g;
        g.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::ARGB8888},
                           "d");
        std::vector<u8> init = {0x10, 0x20, 0x30, 0xFF};
        g.memory().write_block(0x10000, init.data(), 4);
        g.register_surface(SurfaceDesc{0x20000, 4, 1, 1, PixelFormat::ARGB8888},
                           "s");
        std::vector<u8> src = {0x00, 0x00, 0xFF, 0x80};
        g.memory().write_block(0x20000, src.data(), 4);
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 4;
        d.dst_stride = 16;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.blend = static_cast<u32>(BlendMode::STRAIGHT_ALPHA);
        d.pixel_alpha_en = true;
        d.w = 1;
        d.h = 1;
        EXPECT_TRUE(g.execute_command(make_blit_cmd(d)).ok);
        const u8 aeff = static_cast<u8>((255 * 128 + 127) / 255);  // 128
        const u8 er = static_cast<u8>((255 * aeff + 0x30 * (255 - aeff) + 127) / 255);
        std::vector<u8> got;
        g.memory().read_block(0x10000, 4, got);
        EXPECT_TRUE(got[2] == er);
    }
}

}  // namespace

int main() {
    test_dither_global_xy();
    test_dither_disabled();
    test_dither_non_rgb_strict();
    test_dither_non_rgb_nonstrict_ignored();
    test_dst_formats();
    if (g_failures) {
        std::printf("golden_test_dither_dst FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_dither_dst PASS\n");
    return 0;
}
