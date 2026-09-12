#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_isa.hpp"

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

void test_clip_pair_same_ext() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 8, 8, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 16, 4, 4, PixelFormat::RGB565}, "s");
    std::vector<u8> tex(16 * 4, 0xFF);
    gpu.memory().write_block(0x20000, tex.data(), tex.size());

    BlitCmdDesc base;
    base.src_base = 0x20000;
    base.dst_base = 0x10000;
    base.src_stride = 16;
    base.dst_stride = 32;
    base.blit_ext = true;
    base.ext_ptr = 0x30000;
    base.clip_xmin = 2;
    base.clip_ymin = 2;
    base.clip_xmax = 4;
    base.clip_ymax = 4;
    base.w = 4;
    base.h = 4;
    auto ext = make_draw2d_ext_v1(base);
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());

    // CLIP_EN=0 → unclipped (full 4x4)
    auto off = base;
    off.clip_en = false;
    auto cmd0 = make_blit_ext_cmd(off);
    EXPECT_TRUE(gpu.execute_command(cmd0).ok);
    u16 outside = 0;
    gpu.memory().read16(0x10000, outside);  // (0,0) should be written if unclipped
    EXPECT_TRUE(outside != 0);

    gpu.reset();
    gpu.register_surface(SurfaceDesc{0x10000, 32, 8, 8, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 16, 4, 4, PixelFormat::RGB565}, "s");
    gpu.memory().write_block(0x20000, tex.data(), tex.size());
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());

    auto on = base;
    on.clip_en = true;
    auto cmd1 = make_blit_ext_cmd(on);
    EXPECT_TRUE(gpu.execute_command(cmd1).ok);
    u16 outside2 = 0;
    u16 inside = 0;
    gpu.memory().read16(0x10000, outside2);
    gpu.memory().read16(0x10000 + 2 * 32 + 2 * 2, inside);
    EXPECT_TRUE(outside2 == 0);
    EXPECT_TRUE(inside != 0);
}

void test_fill_ext_clip() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 8, 8, PixelFormat::RGB565}, "d");
    auto cmd = make_fill_rect_cmd(0x10000, 32, 0, 0, 8, 8,
                                  Rgba8888::pack(255, 255, 0, 0));
    BlitCmdDesc dummy;
    dummy.clip_xmin = 4;
    dummy.clip_ymin = 4;
    dummy.clip_xmax = 6;
    dummy.clip_ymax = 6;
    auto ext = make_draw2d_ext_v1(dummy);
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());
    cmd[0] = header_word(kClassDraw2D, kOpcodeFillRect, 1, 16, kHExtValid);
    cmd[3] = 0x30000;
    cmd[12] |= (1u << 25);  // CLIP_EN
    EXPECT_TRUE(gpu.execute_command(cmd).ok);
    u16 p00 = 0, p44 = 0;
    gpu.memory().read16(0x10000, p00);
    gpu.memory().read16(0x10000 + 4 * 32 + 4 * 2, p44);
    EXPECT_TRUE(p00 == 0);
    EXPECT_TRUE(p44 == 0xF800);
}

void test_blit_ext_flip_flag_illegal() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 4, 4, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 8, 2, 2, PixelFormat::RGB565}, "s");
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 8;
    d.dst_stride = 32;
    d.blit_ext = true;
    d.ext_ptr = 0x30000;
    d.flip_x = true;
    d.w = 2;
    d.h = 2;
    d.u0 = 0;
    d.du_dx = 65536;
    d.dv_dy = 65536;
    auto ext = make_draw2d_ext_v1(d);
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());
    auto cmd = make_blit_ext_cmd(d);
    const auto st = gpu.execute_command(cmd);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::UNSUPPORTED_FEATURE);
}

void test_ext_reserved_strict_pair() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 4, 4, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 8, 2, 2, PixelFormat::RGB565}, "s");
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 8;
    d.dst_stride = 32;
    d.blit_ext = true;
    d.ext_ptr = 0x30000;
    d.w = 2;
    d.h = 2;
    auto ext = make_draw2d_ext_v1(d);
    // set W12 nonzero
    ext[12 * 4] = 1;
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());
    auto cmd = make_blit_ext_cmd(d);
    // non-strict: reserved ignored
    EXPECT_TRUE(gpu.execute_command(cmd).ok);
    // strict: reserved fault
    cmd[0] |= kHStrict;
    const auto st = gpu.execute_command(cmd);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::RESERVED_NONZERO);
}

void test_q16_origin_reject() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 4, 4, PixelFormat::RGB565}, "d");
    gpu.register_resource(RegisteredResource{0x20000, 8, 2, 2, "s"});
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 4;
    d.dst_stride = 32;
    d.src_x = 40000;
    d.w = 1;
    d.h = 1;
    const auto st = gpu.execute_command(make_blit_cmd(d));
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::BAD_ADDRESS);
}

}  // namespace

int main() {
    test_clip_pair_same_ext();
    test_fill_ext_clip();
    test_blit_ext_flip_flag_illegal();
    test_ext_reserved_strict_pair();
    test_q16_origin_reject();
    if (g_failures) {
        std::printf("golden_test_semantics_pairs FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_semantics_pairs PASS\n");
    return 0;
}
