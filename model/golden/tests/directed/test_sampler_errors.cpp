#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"

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

void test_src_out_of_resource() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 4, 4, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 8, 2, 2, PixelFormat::RGB565}, "s");
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 8;
    d.dst_stride = 32;
    d.src_x = 0;
    d.src_y = 0;
    d.w = 4;
    d.h = 4;  // exceeds 2x2 source
    const auto st = gpu.execute_command(make_blit_cmd(d));
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::MEMORY_ERROR || st.fault == FaultCode::BAD_RECT);
}

void test_palette_unmapped() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 4, 4, PixelFormat::RGB565}, "d");
    gpu.register_resource(RegisteredResource{0x20000, 4, 2, 2, "t"});
    // INDEX8 texture
    std::vector<u8> tex = {0, 1, 2, 3};
    gpu.memory().write_block(0x20000, tex.data(), tex.size());
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 2;
    d.dst_stride = 32;
    d.src_format = static_cast<u32>(PixelFormat::INDEX8);
    d.palette_en = true;
    d.palette_addr = 0x90000;  // unmapped
    d.w = 2;
    d.h = 2;
    const auto st = gpu.execute_command(make_blit_cmd(d));
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::MEMORY_ERROR);
}

void test_bad_stride_before_render() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 8, 2, PixelFormat::RGB565}, "d");
    auto cmd = make_fill_rect_cmd(0x10000, 2 /* < 16 */, 0, 0, 8, 1,
                                  Rgba8888::pack(255, 255, 0, 0));
    const auto st = gpu.execute_command(cmd);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::BAD_RECT);
}

}  // namespace

int main() {
    test_src_out_of_resource();
    test_palette_unmapped();
    test_bad_stride_before_render();
    if (g_failures) {
        std::printf("golden_test_sampler_errors FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_sampler_errors PASS\n");
    return 0;
}
