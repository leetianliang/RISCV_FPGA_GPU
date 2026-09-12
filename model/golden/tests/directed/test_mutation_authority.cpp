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

void test_src_xy_unsigned() {
    GpuCmd64 cmd{};
    cmd[0] = header_word(kClassDraw2D, kOpcodeBlit, 1, 16, 0);
    cmd[8] = 0x80008000u;
    const auto d = decode_draw_2d(cmd, nullptr, 0);
    EXPECT_TRUE(d.state.src_x == 32768u);
    EXPECT_TRUE(d.state.src_y == 32768u);
}

void test_reserved_format_bad() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 4, 4, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 8, 4, 4, PixelFormat::RGB565}, "s");
    auto rf = make_blit_cmd(BlitCmdDesc{});
    // rebuild valid then mutate format
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 8;
    d.dst_stride = 32;
    d.w = 1;
    d.h = 1;
    rf = make_blit_cmd(d);
    rf[12] = (rf[12] & ~0xFu) | 0x7u;
    EXPECT_TRUE(gpu.execute_command(rf).fault == FaultCode::BAD_FORMAT);
}

void test_dst_stride_too_small() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 16, 8, 2, PixelFormat::RGB565}, "d");
    auto cmd = make_fill_rect_cmd(0x10000, 4, 0, 0, 8, 1,
                                  Rgba8888::pack(255, 255, 0, 0));
    EXPECT_TRUE(gpu.execute_command(cmd).fault == FaultCode::BAD_RECT);
}

void test_xrgb_write_ff() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x30000, 16, 2, 2, PixelFormat::XRGB8888}, "x");
    auto cmd = make_fill_rect_cmd(0x30000, 16, 0, 0, 1, 1,
                                  Rgba8888::pack(1, 0x11, 0x22, 0x33));
    u32 ds = cmd[12];
    ds = (ds & ~(0xFu << 4)) | (static_cast<u32>(PixelFormat::XRGB8888) << 4);
    cmd[12] = ds;
    EXPECT_TRUE(gpu.execute_command(cmd).ok);
    u32 w = 0;
    gpu.memory().read32(0x30000, w);
    EXPECT_TRUE((w >> 24) == 0xFF);
    EXPECT_TRUE((w & 0xFF) == 0x33);
}

}  // namespace

int main() {
    test_src_xy_unsigned();
    test_reserved_format_bad();
    test_dst_stride_too_small();
    test_xrgb_write_ff();
    if (g_failures) {
        std::printf("golden_test_mutation FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_mutation PASS\n");
    return 0;
}
