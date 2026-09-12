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

struct Fixture {
    golden::GoldenGPU gpu;
    golden::SurfaceDesc desc{};
    std::vector<golden::u8> initial;

    explicit Fixture(golden::u32 w = 8, golden::u32 h = 8,
                     golden::u32 stride = 16, golden::u32 base = 0x10000) {
        desc.base = base;
        desc.stride = stride;
        desc.width = w;
        desc.height = h;
        desc.format = golden::PixelFormat::RGB565;
        const auto st = gpu.register_surface(desc, "fb");
        EXPECT_TRUE(st.ok);
        initial.assign(static_cast<std::size_t>(stride) * h, 0x00);
        // seed pattern
        for (std::size_t i = 0; i < initial.size(); i += 2) {
            initial[i] = static_cast<golden::u8>(i);
            initial[i + 1] = static_cast<golden::u8>(0x10 + (i / 2));
        }
        gpu.memory().write_block(base, initial.data(), initial.size());
    }

    golden::u16 px(golden::u32 x, golden::u32 y) const {
        golden::u16 v = 0;
        gpu.memory().read16(desc.base + y * desc.stride + x * 2, v);
        return v;
    }
};

golden::u16 encode_or(golden::u8 r, golden::u8 g, golden::u8 b) {
    return golden::rgb565_encode(golden::Rgba8888::pack(255, r, g, b));
}

void test_fill_001_1x1() {
    Fixture f;
    const auto color = golden::Rgba8888::pack(255, 255, 0, 0);
    const auto cmd = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 3, 4, 1, 1, color);
    EXPECT_TRUE(f.gpu.execute_command(cmd).ok);
    EXPECT_TRUE(f.px(3, 4) == encode_or(255, 0, 0));
}

void test_fill_002_interior() {
    Fixture f;
    const auto color = golden::Rgba8888::pack(255, 0, 255, 0);
    const auto cmd = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 2, 2, 3, 2, color);
    EXPECT_TRUE(f.gpu.execute_command(cmd).ok);
    const golden::u16 expect = encode_or(0, 255, 0);
    EXPECT_TRUE(f.px(2, 2) == expect);
    EXPECT_TRUE(f.px(4, 3) == expect);
    EXPECT_TRUE(f.px(1, 2) != expect);
    EXPECT_TRUE(f.px(5, 2) != expect);
}

void test_fill_003_full_surface() {
    Fixture f;
    const auto color = golden::Rgba8888::pack(255, 0, 0, 255);
    const auto cmd = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0,
                                                f.desc.width, f.desc.height, color);
    EXPECT_TRUE(f.gpu.execute_command(cmd).ok);
    const golden::u16 expect = encode_or(0, 0, 255);
    for (golden::u32 y = 0; y < f.desc.height; ++y) {
        for (golden::u32 x = 0; x < f.desc.width; ++x) {
            EXPECT_TRUE(f.px(x, y) == expect);
        }
    }
}

void test_fill_004_overlap_later_wins() {
    Fixture f;
    const auto c1 = golden::Rgba8888::pack(255, 255, 0, 0);
    const auto c2 = golden::Rgba8888::pack(255, 0, 0, 255);
    const auto cmd1 = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 1, 1, 4, 4, c1);
    const auto cmd2 = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 2, 2, 2, 2, c2);
    EXPECT_TRUE(f.gpu.execute_command(cmd1).ok);
    EXPECT_TRUE(f.gpu.execute_command(cmd2).ok);
    EXPECT_TRUE(f.px(2, 2) == encode_or(0, 0, 255));
    EXPECT_TRUE(f.px(1, 1) == encode_or(255, 0, 0));
}

void test_fill_005_zero_width() {
    Fixture f;
    const auto before = f.px(0, 0);
    const auto cmd = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 0, 4,
                                                golden::Rgba8888::pack(255, 1, 2, 3));
    EXPECT_TRUE(f.gpu.execute_command(cmd).ok);
    EXPECT_TRUE(f.px(0, 0) == before);
}

void test_fill_006_zero_height() {
    Fixture f;
    const auto before = f.px(1, 1);
    const auto cmd = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 1, 1, 4, 0,
                                                golden::Rgba8888::pack(255, 1, 2, 3));
    EXPECT_TRUE(f.gpu.execute_command(cmd).ok);
    EXPECT_TRUE(f.px(1, 1) == before);
}

void test_fill_007_non_tight_stride() {
    Fixture f(4, 4, 16, 0x20000);
    const auto color = golden::Rgba8888::pack(255, 255, 255, 0);
    const auto cmd = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 1, 1, 2, 2, color);
    EXPECT_TRUE(f.gpu.execute_command(cmd).ok);
    EXPECT_TRUE(f.px(1, 1) == encode_or(255, 255, 0));
    EXPECT_TRUE(f.px(2, 2) == encode_or(255, 255, 0));
}

void test_fill_008_colors() {
    Fixture f;
    const golden::u8 colors[][3] = {
        {0, 0, 0}, {255, 255, 255}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {12, 34, 56}};
    for (std::size_t i = 0; i < 6; ++i) {
        const auto color = golden::Rgba8888::pack(255, colors[i][0], colors[i][1], colors[i][2]);
        const auto cmd = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride,
                                                    static_cast<golden::u32>(i), 0, 1, 1, color);
        EXPECT_TRUE(f.gpu.execute_command(cmd).ok);
        EXPECT_TRUE(f.px(static_cast<golden::u32>(i), 0) ==
                    encode_or(colors[i][0], colors[i][1], colors[i][2]));
    }
}

void test_fill_009_ignored_src_fields() {
    Fixture f;
    const auto color = golden::Rgba8888::pack(255, 8, 16, 32);
    auto cmd_a = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 1, 1, 2, 2, color);
    auto cmd_b = cmd_a;
    cmd_b[4] = 0xDEADBEEFu;  // SRC_BASE
    cmd_b[6] = 0x1234u;      // SRC_STRIDE
    cmd_b[8] = 0x00040003u;  // SRC_XY
    cmd_b[10] = 0x00020002u; // SRC_WH
    cmd_b[14] = 0xA5A5A5A5u; // ALPHA_KEY
    cmd_b[15] = 0x5A5A5A5Au; // PALETTE_ADDR
    EXPECT_TRUE(f.gpu.execute_command(cmd_a).ok);
    Fixture f2;
    // rebuild same initial
    f2.gpu.memory().write_block(f2.desc.base, f.initial.data(), f.initial.size());
    EXPECT_TRUE(f2.gpu.execute_command(cmd_b).ok);
    EXPECT_TRUE(f.px(1, 1) == f2.px(1, 1));
    EXPECT_TRUE(f.px(2, 2) == f2.px(2, 2));
}

void test_fill_010_invalid_no_mod() {
    Fixture f;
    const auto before = f.px(0, 0);
    auto cmd = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 4, 4,
                                          golden::Rgba8888::pack(255, 9, 9, 9));
    cmd[0] = golden::header_word(golden::kClassDraw2D, golden::kOpcodeFillRect,
                                 9, golden::kCmdLengthDw, 0);
    const auto st = f.gpu.execute_command(cmd);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == golden::FaultCode::BAD_VERSION);
    EXPECT_TRUE(f.px(0, 0) == before);
}

}  // namespace

int main() {
    test_fill_001_1x1();
    test_fill_002_interior();
    test_fill_003_full_surface();
    test_fill_004_overlap_later_wins();
    test_fill_005_zero_width();
    test_fill_006_zero_height();
    test_fill_007_non_tight_stride();
    test_fill_008_colors();
    test_fill_009_ignored_src_fields();
    test_fill_010_invalid_no_mod();
    if (g_failures != 0) {
        std::printf("test_fill_directed: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_fill_directed: PASS\n");
    return 0;
}
