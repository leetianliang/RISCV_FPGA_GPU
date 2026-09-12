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
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

using namespace golden;

struct Env {
    GoldenGPU gpu;
    SurfaceDesc dst{};
    SurfaceDesc src{};

    Env(PixelFormat src_fmt = PixelFormat::RGB565) {
        dst = SurfaceDesc{0x10000, 32, 16, 16, PixelFormat::RGB565};
        src.base = 0x20000;
        src.width = 8;
        src.height = 8;
        src.format = src_fmt;
        src.stride = src.width * bytes_per_pixel(src_fmt);
        gpu.register_surface(dst, "dst");
        gpu.register_surface(src, "src");
        std::vector<u8> z(dst.stride * dst.height, 0x20);
        std::vector<u8> z2(src.stride * src.height, 0);
        gpu.memory().write_block(dst.base, z.data(), z.size());
        gpu.memory().write_block(src.base, z2.data(), z2.size());
    }

    void fill_src(u8 r, u8 g, u8 b, u8 a = 255) {
        SurfaceView sv(&gpu.memory(),
                       RegisteredResource{src.base, src.stride * src.height, src.width,
                                          src.height, "s"},
                       src.stride, src.format);
        for (u32 y = 0; y < src.height; ++y) {
            for (u32 x = 0; x < src.width; ++x) {
                sv.write_rgba(static_cast<i32>(x), static_cast<i32>(y),
                              Rgba8888::pack(a, r, g, b), false, x, y);
            }
        }
    }

    u16 dst_px(u32 x, u32 y) const {
        u16 v = 0;
        gpu.memory().read16(dst.base + y * dst.stride + x * 2, v);
        return v;
    }
};

void test_copy() {
    Env e;
    e.fill_src(255, 0, 0);
    BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.src_x = 1;
    d.src_y = 1;
    d.dst_x = 2;
    d.dst_y = 3;
    d.w = 4;
    d.h = 4;
    EXPECT_TRUE(e.gpu.execute_command(make_blit_cmd(d)).ok);
    EXPECT_TRUE(e.dst_px(2, 3) == 0xF800);
}

void test_key_alpha_add() {
    Env e;
    e.fill_src(0, 255, 0);
    BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.w = 2;
    d.h = 2;
    d.color_key_en = true;
    d.color_key_rgb = 0x00FF00;
    EXPECT_TRUE(e.gpu.execute_command(make_blit_cmd(d)).ok);
    EXPECT_TRUE(e.dst_px(0, 0) != 0x07E0);

    Env e2;
    e2.fill_src(255, 255, 255);
    BlitCmdDesc a = d;
    a.color_key_en = false;
    a.blend = static_cast<u32>(BlendMode::STRAIGHT_ALPHA);
    a.global_alpha_en = true;
    a.global_alpha = 255;
    EXPECT_TRUE(e2.gpu.execute_command(make_blit_cmd(a)).ok);
    EXPECT_TRUE(e2.dst_px(0, 0) == 0xFFFF);

    Env e3;
    e3.fill_src(255, 255, 255);
    a.global_alpha = 0;
    EXPECT_TRUE(e3.gpu.execute_command(make_blit_cmd(a)).ok);
    EXPECT_TRUE(e3.dst_px(0, 0) != 0xFFFF);

    Env e4;
    e4.fill_src(255, 255, 255);
    a.blend = static_cast<u32>(BlendMode::ADD_SAT);
    a.global_alpha = 255;
    EXPECT_TRUE(e4.gpu.execute_command(make_blit_cmd(a)).ok);
    EXPECT_TRUE(e4.dst_px(0, 0) == 0xFFFF);
}

}  // namespace

int main() {
    test_copy();
    test_key_alpha_add();
    if (g_failures) {
        std::printf("golden_test_blit_directed FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_blit_directed PASS\n");
    return 0;
}
