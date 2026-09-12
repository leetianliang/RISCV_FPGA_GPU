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

struct Fx {
    GoldenGPU gpu;
    SurfaceDesc desc{};
    std::vector<u8> initial;

    Fx() {
        desc = SurfaceDesc{0x10000, 32, 8, 8, PixelFormat::RGB565};
        gpu.register_surface(desc, "fb");
        initial.assign(32 * 8, 0x11);
        gpu.memory().write_block(desc.base, initial.data(), initial.size());
    }

    u16 px(u32 x, u32 y) const {
        u16 v = 0;
        gpu.memory().read16(desc.base + y * desc.stride + x * 2, v);
        return v;
    }
};

void test_fill_001_1x1() {
    Fx f;
    EXPECT_TRUE(f.gpu
                    .execute_command(make_fill_rect_cmd(f.desc.base, f.desc.stride, 3, 4, 1,
                                                        1, Rgba8888::pack(255, 255, 0, 0)))
                    .ok);
    EXPECT_TRUE(f.px(3, 4) == 0xF800);
}

void test_fill_002_interior() {
    Fx f;
    const auto c = Rgba8888::pack(255, 0, 255, 0);
    EXPECT_TRUE(f.gpu
                    .execute_command(make_fill_rect_cmd(f.desc.base, f.desc.stride, 2, 2, 3, 2, c))
                    .ok);
    EXPECT_TRUE(f.px(2, 2) == 0x07E0);
    EXPECT_TRUE(f.px(1, 2) != 0x07E0);
}

void test_fill_003_full() {
    Fx f;
    EXPECT_TRUE(f.gpu
                    .execute_command(make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 8, 8,
                                                        Rgba8888::pack(255, 0, 0, 255)))
                    .ok);
    for (u32 y = 0; y < 8; ++y) {
        for (u32 x = 0; x < 8; ++x) {
            EXPECT_TRUE(f.px(x, y) == 0x001F);
        }
    }
}

void test_fill_004_overlap() {
    Fx f;
    f.gpu.execute_command(make_fill_rect_cmd(f.desc.base, f.desc.stride, 1, 1, 4, 4,
                                             Rgba8888::pack(255, 255, 0, 0)));
    f.gpu.execute_command(make_fill_rect_cmd(f.desc.base, f.desc.stride, 2, 2, 2, 2,
                                             Rgba8888::pack(255, 0, 0, 255)));
    EXPECT_TRUE(f.px(2, 2) == 0x001F);
    EXPECT_TRUE(f.px(1, 1) == 0xF800);
}

void test_fill_005_006_zero() {
    Fx f;
    const auto b = f.px(0, 0);
    EXPECT_TRUE(f.gpu
                    .execute_command(make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 0, 4,
                                                        Rgba8888::from_u32(0xFFFFFFFFu)))
                    .ok);
    EXPECT_TRUE(f.px(0, 0) == b);
    EXPECT_TRUE(f.gpu
                    .execute_command(make_fill_rect_cmd(f.desc.base, f.desc.stride, 1, 1, 4, 0,
                                                        Rgba8888::from_u32(0xFFFFFFFFu)))
                    .ok);
}

void test_fill_007_non_tight() {
    Fx f;
    // command stride 32 already non-tight for 8px RGB565 (16 bytes needed)
    EXPECT_TRUE(f.gpu
                    .execute_command(make_fill_rect_cmd(f.desc.base, f.desc.stride, 1, 1, 2, 2,
                                                        Rgba8888::pack(255, 255, 255, 0)))
                    .ok);
    EXPECT_TRUE(f.px(1, 1) == 0xFFE0);
}

void test_fill_008_colors() {
    Fx f;
    const u8 cols[][3] = {{0, 0, 0}, {255, 255, 255}, {255, 0, 0}, {0, 255, 0},
                          {0, 0, 255}, {12, 34, 56}};
    for (size_t i = 0; i < 6; ++i) {
        f.gpu.execute_command(make_fill_rect_cmd(
            f.desc.base, f.desc.stride, static_cast<i32>(i), 0, 1, 1,
            Rgba8888::pack(255, cols[i][0], cols[i][1], cols[i][2])));
    }
    EXPECT_TRUE(f.px(0, 0) == 0x0000);
    EXPECT_TRUE(f.px(1, 0) == 0xFFFF);
}

void test_fill_009_ignored_src() {
    Fx f;
    auto a = make_fill_rect_cmd(f.desc.base, f.desc.stride, 1, 1, 2, 2,
                                Rgba8888::pack(255, 8, 16, 32));
    auto b = a;
    b[4] = 0xDEADBEEFu;
    b[6] = 0x1234u;
    b[8] = 0x00040003u;
    b[10] = 0x00020002u;
    b[14] = 0xA5A5A5A5u;
    b[15] = 0x5A5A5A5Au;
    f.gpu.execute_command(a);
    Fx f2;
    f2.gpu.memory().write_block(f2.desc.base, f.initial.data(), f.initial.size());
    f2.gpu.execute_command(b);
    EXPECT_TRUE(f.px(1, 1) == f2.px(1, 1));
}

void test_fill_010_invalid() {
    Fx f;
    const auto before = f.px(0, 0);
    auto c = make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 4, 4,
                                Rgba8888::pack(255, 9, 9, 9));
    c[0] = header_word(kClassDraw2D, kOpcodeFillRect, 9, kCmdLengthDw, 0);
    const auto st = f.gpu.execute_command(c);
    EXPECT_TRUE(!st.ok);
    EXPECT_TRUE(st.fault == FaultCode::BAD_VERSION);
    EXPECT_TRUE(f.px(0, 0) == before);
}

void test_alpha_boundary() {
    Fx f;
    // fill black then straight alpha white with aeff 0,1,127,128,254,255
    f.gpu.execute_command(make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 8, 8,
                                             Rgba8888::pack(255, 0, 0, 0)));
    for (int a : {0, 1, 127, 128, 254, 255}) {
        auto cmd = make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 1, 1,
                                      Rgba8888::pack(255, 255, 255, 255));
        u32 ds = cmd[12];
        ds = (ds & ~(0xFu << 8)) | (static_cast<u32>(BlendMode::STRAIGHT_ALPHA) << 8);
        ds |= (1u << 19);  // global alpha
        cmd[12] = ds;
        cmd[14] = (static_cast<u32>(a) << 24);
        EXPECT_TRUE(f.gpu.execute_command(cmd).ok);
    }
}

}  // namespace

int main() {
    test_fill_001_1x1();
    test_fill_002_interior();
    test_fill_003_full();
    test_fill_004_overlap();
    test_fill_005_006_zero();
    test_fill_007_non_tight();
    test_fill_008_colors();
    test_fill_009_ignored_src();
    test_fill_010_invalid();
    test_alpha_boundary();
    if (g_failures) {
        std::printf("golden_test_fill_directed FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_fill_directed PASS\n");
    return 0;
}
