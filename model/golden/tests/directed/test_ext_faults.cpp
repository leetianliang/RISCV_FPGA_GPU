#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_isa.hpp"

#include <cstdio>

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

void test_bad_version_beats_ext_memory() {
    GoldenGPU gpu;
    auto cmd = make_fill_rect_cmd(0x10000, 32, 0, 0, 1, 1,
                                  Rgba8888::pack(255, 1, 2, 3));
    cmd[0] = header_word(kClassDraw2D, kOpcodeFillRect, 9, kCmdLengthDw, kHExtValid);
    cmd[3] = 0x99999000u;  // unmapped
    const auto st = gpu.execute_command(cmd);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::BAD_VERSION);
}

void test_misaligned_ext_no_fetch() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 4, 4, PixelFormat::RGB565}, "d");
    auto cmd = make_blit_ext_cmd(BlitCmdDesc{});
    cmd[0] = header_word(kClassDraw2D, kOpcodeBlitExt, 1, 16, kHExtValid);
    cmd[3] = 0x30000001u;  // misaligned
    const auto st = gpu.execute_command(cmd);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::BAD_EXT_PTR);
}

void test_valid_header_unmapped_ext() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 4, 4, PixelFormat::RGB565}, "d");
    auto cmd = make_blit_ext_cmd(BlitCmdDesc{});
    cmd[0] = header_word(kClassDraw2D, kOpcodeBlitExt, 1, 16, kHExtValid);
    cmd[3] = 0x50000000u;
    const auto st = gpu.execute_command(cmd);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::MEMORY_ERROR);
}

void test_cross_term_rejected_nonstrict() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 64, 8, 8, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 16, 4, 4, PixelFormat::RGB565}, "s");
    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 16;
    d.dst_stride = 64;
    d.blit_ext = true;
    d.ext_ptr = 0x30000;
    d.clip_en = false;
    d.dv_dx = 1;  // cross term
    d.w = 8;
    d.h = 8;
    auto ext = make_draw2d_ext_v1(d);
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    gpu.memory().write_block(0x30000, ext.data(), ext.size());
    auto cmd = make_blit_ext_cmd(d);
    // ensure non-strict
    cmd[0] &= ~static_cast<u32>(kHStrict);
    const auto st = gpu.execute_command(cmd);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::UNSUPPORTED_FEATURE);
}

}  // namespace

int main() {
    test_bad_version_beats_ext_memory();
    test_misaligned_ext_no_fetch();
    test_valid_header_unmapped_ext();
    test_cross_term_rejected_nonstrict();
    if (g_failures) {
        std::printf("golden_test_ext_faults FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_ext_faults PASS\n");
    return 0;
}
