#include "golden/gpu_math.hpp"

#include <cstdio>
#include <limits>

namespace {

int g_failures = 0;

#define EXPECT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

#define EXPECT_EQ(a, b)                                                       \
    do {                                                                      \
        const auto va = (a);                                                  \
        const auto vb = (b);                                                  \
        if (!(va == vb)) {                                                    \
            std::printf("FAIL %s:%d\n", __FILE__, __LINE__);                  \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

void test_effective_alpha() {
    EXPECT_EQ(golden::effective_alpha(100, false, 255, false, 200, false), 255);
    EXPECT_EQ(golden::effective_alpha(100, true, 255, false, 200, false), 100);
    EXPECT_EQ(golden::effective_alpha(100, true, 255, false, 128, true), 50);
    EXPECT_EQ(golden::effective_alpha(255, true, 255, false, 0, true), 0);
    EXPECT_EQ(golden::effective_alpha(255, true, 255, false, 255, true), 255);
}

void test_straight_and_add() {
    EXPECT_EQ(golden::blend_straight_chan(255, 0, 0), 0);
    EXPECT_EQ(golden::blend_straight_chan(255, 0, 255), 255);
    EXPECT_EQ(golden::source_over_alpha(0, 128), 128);
    EXPECT_EQ(golden::source_over_alpha(255, 128), 255);
    EXPECT_EQ(golden::add_sat_chan(200, 100), 255);
}

void test_xrgb_bytes() {
    const golden::u8 b[4] = {0x11, 0x22, 0x33, 0xFF};
    const auto c = golden::decode_source_pixel(golden::PixelFormat::XRGB8888, b);
    EXPECT_EQ(c.a(), 255);
    EXPECT_EQ(c.r(), 0x33);
}

}  // namespace

int main() {
    test_effective_alpha();
    test_straight_and_add();
    test_xrgb_bytes();
    if (g_failures) {
        std::printf("golden_test_pixel_pipeline FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_pixel_pipeline PASS\n");
    return 0;
}
