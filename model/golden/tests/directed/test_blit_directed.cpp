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

struct Env {
    golden::GoldenGPU gpu;
    golden::SurfaceDesc dst{};
    golden::SurfaceDesc src{};
    std::vector<golden::u8> dst_init;
    std::vector<golden::u8> src_data;

    Env(golden::PixelFormat src_fmt = golden::PixelFormat::RGB565) {
        dst.base = 0x10000;
        dst.stride = 32;
        dst.width = 16;
        dst.height = 16;
        dst.format = golden::PixelFormat::RGB565;
        src.base = 0x20000;
        src.width = 8;
        src.height = 8;
        src.format = src_fmt;
        src.stride = src.width * golden::bytes_per_pixel(src_fmt);
        gpu.register_surface(dst, "dst");
        gpu.register_surface(src, "src");
        dst_init.assign(dst.stride * dst.height, 0x20);
        src_data.assign(src.stride * src.height, 0x00);
        gpu.memory().write_block(dst.base, dst_init.data(), dst_init.size());
        gpu.memory().write_block(src.base, src_data.data(), src_data.size());
    }

    void fill_src_solid(golden::u8 r, golden::u8 g, golden::u8 b, golden::u8 a = 255) {
        golden::Surface s(&gpu.memory(), src);
        for (golden::u32 y = 0; y < src.height; ++y) {
            for (golden::u32 x = 0; x < src.width; ++x) {
                s.write_pixel(x, y, golden::Rgba8888::pack(a, r, g, b));
            }
        }
    }

    golden::u16 dst_px(golden::u32 x, golden::u32 y) const {
        golden::u16 v = 0;
        gpu.memory().read16(dst.base + y * dst.stride + x * 2, v);
        return v;
    }
};

void test_copy_rgb565() {
    Env e;
    e.fill_src_solid(255, 0, 0);
    golden::BlitCmdDesc d;
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
    EXPECT_TRUE(e.gpu.execute_command(golden::make_blit_cmd(d)).ok);
    EXPECT_TRUE(e.dst_px(2, 3) == 0xF800);
    EXPECT_TRUE(e.dst_px(5, 6) == 0xF800);
    EXPECT_TRUE(e.dst_px(1, 3) != 0xF800);
}

void test_argb_to_rgb565() {
    Env e(golden::PixelFormat::ARGB8888);
    e.fill_src_solid(0, 255, 0, 255);
    golden::BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.src_format = static_cast<golden::u32>(golden::PixelFormat::ARGB8888);
    d.w = 2;
    d.h = 2;
    EXPECT_TRUE(e.gpu.execute_command(golden::make_blit_cmd(d)).ok);
    EXPECT_TRUE(e.dst_px(0, 0) == 0x07E0);
}

void test_colorkey() {
    Env e;
    e.fill_src_solid(0, 255, 0);  // green key
    // punch a red pixel
    golden::Surface s(&e.gpu.memory(), e.src);
    s.write_pixel(1, 1, golden::Rgba8888::pack(255, 255, 0, 0));
    golden::BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.w = 2;
    d.h = 2;
    d.color_key_en = true;
    d.color_key_rgb = 0x00FF00;
    EXPECT_TRUE(e.gpu.execute_command(golden::make_blit_cmd(d)).ok);
    EXPECT_TRUE(e.dst_px(0, 0) != 0x07E0);  // keyed out, still background
    EXPECT_TRUE(e.dst_px(1, 1) == 0xF800);  // miss key, copied red
}

void test_global_alpha() {
    Env e;
    e.fill_src_solid(255, 255, 255);
    golden::BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.w = 2;
    d.h = 2;
    d.blend = static_cast<golden::u32>(golden::BlendMode::STRAIGHT_ALPHA);
    d.global_alpha_en = true;
    d.global_alpha = 0;
    EXPECT_TRUE(e.gpu.execute_command(golden::make_blit_cmd(d)).ok);
    // A=0 leaves dest (encoded 0x20 pattern)
    EXPECT_TRUE(e.dst_px(0, 0) != 0xFFFF);

    d.global_alpha = 255;
    EXPECT_TRUE(e.gpu.execute_command(golden::make_blit_cmd(d)).ok);
    EXPECT_TRUE(e.dst_px(0, 0) == 0xFFFF);
}

void test_pixel_alpha_argb() {
    Env e(golden::PixelFormat::ARGB8888);
    golden::Surface s(&e.gpu.memory(), e.src);
    s.write_pixel(0, 0, golden::Rgba8888::pack(0, 255, 0, 0));
    s.write_pixel(1, 0, golden::Rgba8888::pack(255, 255, 0, 0));
    golden::BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.src_format = static_cast<golden::u32>(golden::PixelFormat::ARGB8888);
    d.w = 2;
    d.h = 1;
    d.blend = static_cast<golden::u32>(golden::BlendMode::STRAIGHT_ALPHA);
    d.pixel_alpha_en = true;
    EXPECT_TRUE(e.gpu.execute_command(golden::make_blit_cmd(d)).ok);
    EXPECT_TRUE(e.dst_px(1, 0) == 0xF800);
}

void test_additive_sat() {
    Env e;
    e.fill_src_solid(255, 255, 255);
    golden::BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.w = 2;
    d.h = 2;
    d.blend = static_cast<golden::u32>(golden::BlendMode::ADD_SAT);
    EXPECT_TRUE(e.gpu.execute_command(golden::make_blit_cmd(d)).ok);
    EXPECT_TRUE(e.dst_px(0, 0) == 0xFFFF);
}

void test_copy_ignores_alpha_flags() {
    Env e(golden::PixelFormat::ARGB8888);
    e.fill_src_solid(255, 0, 0, 0);  // alpha 0
    golden::BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.src_format = static_cast<golden::u32>(golden::PixelFormat::ARGB8888);
    d.w = 2;
    d.h = 2;
    d.blend = static_cast<golden::u32>(golden::BlendMode::COPY);
    d.pixel_alpha_en = true;
    d.global_alpha_en = true;
    d.global_alpha = 0;
    EXPECT_TRUE(e.gpu.execute_command(golden::make_blit_cmd(d)).ok);
    EXPECT_TRUE(e.dst_px(0, 0) == 0xF800);
}

void test_unsupported_no_silent() {
    Env e;
    golden::BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.w = 2;
    d.h = 2;
    d.blend = static_cast<golden::u32>(golden::BlendMode::MULTIPLY);
    const auto st = e.gpu.execute_command(golden::make_blit_cmd(d));
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == golden::FaultCode::UNSUPPORTED_FEATURE);
}

void test_overdraw_quantize() {
    Env e;
    e.fill_src_solid(255, 0, 0, 255);
    golden::BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.w = 1;
    d.h = 1;
    d.blend = static_cast<golden::u32>(golden::BlendMode::STRAIGHT_ALPHA);
    d.pixel_alpha_en = true;
    d.global_alpha_en = true;
    d.global_alpha = 128;
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(e.gpu.execute_command(golden::make_blit_cmd(d)).ok);
    }
    // still a defined RGB565 red-ish value
    EXPECT_TRUE(e.dst_px(0, 0) != 0);
}

void test_stream_order() {
    Env e;
    std::vector<golden::GpuCmd64> cmds;
    cmds.push_back(golden::make_fill_rect_cmd(e.dst.base, e.dst.stride, 0, 0, 4, 4,
                                              golden::Rgba8888::pack(255, 0, 0, 255)));
    golden::BlitCmdDesc d;
    d.src_base = e.src.base;
    d.dst_base = e.dst.base;
    d.src_stride = e.src.stride;
    d.dst_stride = e.dst.stride;
    d.w = 1;
    d.h = 1;
    cmds.push_back(golden::make_blit_cmd(d));
    // bad command
    auto bad = cmds[0];
    bad[0] = golden::header_word(golden::kClassDraw2D, 0x11, 1, 16, 0);
    cmds.push_back(bad);
    const auto st = e.gpu.execute_stream(cmds);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault_index == 2);
    EXPECT_TRUE(st.fault == golden::FaultCode::BAD_OPCODE);
}

}  // namespace

int main() {
    test_copy_rgb565();
    test_argb_to_rgb565();
    test_colorkey();
    test_global_alpha();
    test_pixel_alpha_argb();
    test_additive_sat();
    test_copy_ignores_alpha_flags();
    test_unsupported_no_silent();
    test_overdraw_quantize();
    test_stream_order();
    if (g_failures) {
        std::printf("golden_test_blit_directed FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_blit_directed PASS\n");
    return 0;
}
