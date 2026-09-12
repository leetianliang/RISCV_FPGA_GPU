#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

#include <cstdio>
#include <limits>
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

void test_scale_nearest() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 64, 16, 16, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 16, 4, 4, PixelFormat::RGB565}, "s");
    // paint src
    SurfaceView sv(&gpu.memory(), RegisteredResource{0x20000, 64, 4, 4, "s"}, 16,
                   PixelFormat::RGB565);
    sv.write_rgba(0, 0, Rgba8888::pack(255, 255, 0, 0), false, 0, 0);
    sv.write_rgba(3, 3, Rgba8888::pack(255, 0, 0, 255), false, 3, 3);

    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 16;
    d.dst_stride = 64;
    d.blit_ext = true;
    d.ext_ptr = 0x30000;
    d.clip_en = true;
    d.w = 16;
    d.h = 16;
    compute_axis_aligned_uv(0, 4, 16, d.u0, d.du_dx);
    compute_axis_aligned_uv_v(0, 4, 16, d.v0, d.dv_dy);
    auto ext = make_draw2d_ext_v1(d);
    RegisteredResource er{0x30000, 64, 1, 1, "e"};
    gpu.register_resource(er);
    gpu.memory().write_block(0x30000, ext.data(), ext.size());
    auto cmd = make_blit_ext_cmd(d);
    cmd[10] = pack_wh(4, 4);
    cmd[11] = pack_wh(16, 16);
    EXPECT_TRUE(gpu.execute_command(cmd).ok);
    SurfaceView dv(&gpu.memory(), RegisteredResource{0x10000, 64 * 16, 16, 16, "d"}, 64,
                   PixelFormat::RGB565);
    Rgba8888 c{};
    dv.read_rgba(0, 0, c);
    EXPECT_TRUE(c.r() == 255);
}

void test_blit_ext_eq_blit() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 8, 8, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 16, 4, 4, PixelFormat::RGB565}, "s");
    std::vector<u8> tex(16 * 4, 0);
    for (size_t i = 0; i < tex.size(); i += 2) {
        tex[i] = static_cast<u8>(i);
        tex[i + 1] = 0x1C;  // some blue
    }
    gpu.memory().write_block(0x20000, tex.data(), tex.size());

    BlitCmdDesc b;
    b.src_base = 0x20000;
    b.dst_base = 0x10000;
    b.src_stride = 16;
    b.dst_stride = 32;
    b.w = 4;
    b.h = 4;
    gpu.execute_command(make_blit_cmd(b));
    std::vector<u8> fb1;
    gpu.memory().read_block(0x10000, 32 * 8, fb1);

    gpu.reset();
    gpu.register_surface(SurfaceDesc{0x10000, 32, 8, 8, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 16, 4, 4, PixelFormat::RGB565}, "s");
    gpu.memory().write_block(0x20000, tex.data(), tex.size());

    BlitCmdDesc e = b;
    e.blit_ext = true;
    e.ext_ptr = 0x30000;
    e.clip_en = true;
    e.u0 = 0;
    e.v0 = 0;
    e.du_dx = 65536;
    e.dv_dy = 65536;
    auto ext = make_draw2d_ext_v1(e);
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());
    gpu.execute_command(make_blit_ext_cmd(e));
    std::vector<u8> fb2;
    gpu.memory().read_block(0x10000, 32 * 8, fb2);
    EXPECT_TRUE(fb1 == fb2);
}

void test_flip() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 16, 2, 1, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 4, 2, 1, PixelFormat::RGB565}, "s");
    SurfaceView sv(&gpu.memory(), RegisteredResource{0x20000, 4, 2, 1, "s"}, 4,
                   PixelFormat::RGB565);
    sv.write_rgba(0, 0, Rgba8888::pack(255, 255, 0, 0), false, 0, 0);
    sv.write_rgba(1, 0, Rgba8888::pack(255, 0, 0, 255), false, 1, 0);
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 4;
    d.dst_stride = 16;
    d.w = 2;
    d.h = 1;
    d.flip_x = true;
    EXPECT_TRUE(gpu.execute_command(make_blit_cmd(d)).ok);
    u16 p0 = 0, p1 = 0;
    gpu.memory().read16(0x10000, p0);
    gpu.memory().read16(0x10002, p1);
    EXPECT_TRUE(p0 == 0x001F);  // flipped: blue first
    EXPECT_TRUE(p1 == 0xF800);
}

void test_clip() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 16, 4, 4, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 8, 4, 4, PixelFormat::RGB565}, "s");
    std::vector<u8> tex(8 * 4, 0xFF);
    gpu.memory().write_block(0x20000, tex.data(), tex.size());
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 8;
    d.dst_stride = 16;
    d.w = 4;
    d.h = 4;
    d.blit_ext = true;
    d.ext_ptr = 0x30000;
    d.clip_en = true;
    d.clip_xmin = 1;
    d.clip_ymin = 1;
    d.clip_xmax = 3;
    d.clip_ymax = 3;
    auto ext = make_draw2d_ext_v1(d);
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());
    EXPECT_TRUE(gpu.execute_command(make_blit_ext_cmd(d)).ok);
    u16 outside = 0, inside = 0;
    gpu.memory().read16(0x10000, outside);
    gpu.memory().read16(0x10000 + 1 * 16 + 2, inside);
    EXPECT_TRUE(outside == 0);
    EXPECT_TRUE(inside != 0);
}

void test_round_div_constants() {
    EXPECT_TRUE(round_div_signed(5, 2) == 3);
    EXPECT_TRUE(round_div_signed(-5, 2) == -3);
    EXPECT_TRUE(round_div_signed(std::numeric_limits<i64>::min(), 1) ==
                std::numeric_limits<i64>::min());
    // |INT64_MIN|/2 = 2^62 exactly
    EXPECT_TRUE(round_div_signed(std::numeric_limits<i64>::min(), 2) ==
                -static_cast<i64>(1ull << 62));
    EXPECT_TRUE(round_div_signed(std::numeric_limits<i64>::max(), 1) ==
                std::numeric_limits<i64>::max());
}

}  // namespace

int main() {
    test_scale_nearest();
    test_blit_ext_eq_blit();
    test_flip();
    test_clip();
    test_round_div_constants();
    if (g_failures) {
        std::printf("golden_test_sprite_ext FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_sprite_ext PASS\n");
    return 0;
}
