#include "golden/gpu_math.hpp"

#include <cstdio>

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
    // pixel off -> 255; global off -> 255
    EXPECT_EQ(golden::effective_alpha(100, false, 200, false), 255);
    // pixel on, global off
    EXPECT_EQ(golden::effective_alpha(100, true, 200, false), 100);
    // staged: mul(100,255)=100, mul(100,128)=50, mul(50,255)=50
    EXPECT_EQ(golden::effective_alpha(100, true, 128, true), 50);
    EXPECT_EQ(golden::effective_alpha(255, true, 0, true), 0);
    EXPECT_EQ(golden::effective_alpha(255, true, 255, true), 255);
}

void test_straight_and_add() {
    EXPECT_EQ(golden::blend_straight_chan(255, 0, 0), 0);
    EXPECT_EQ(golden::blend_straight_chan(255, 0, 255), 255);
    EXPECT_EQ(golden::blend_straight_chan(0, 255, 0), 255);
    EXPECT_EQ(golden::source_over_alpha(0, 128), 128);
    EXPECT_EQ(golden::source_over_alpha(255, 128), 255);
    EXPECT_EQ(golden::add_sat_chan(200, 100), 255);
    EXPECT_EQ(golden::add_sat_chan(100, 50), 150);
}

void test_key_compare_semantics() {
    const auto c = golden::Rgba8888::pack(128, 0x12, 0x34, 0x56);
    const golden::u32 rgb =
        (static_cast<golden::u32>(c.r()) << 16) | (c.g() << 8) | c.b();
    EXPECT_EQ(rgb, 0x00123456u);
}

}  // namespace

int main() {
    test_effective_alpha();
    test_straight_and_add();
    test_key_compare_semantics();
    if (g_failures) {
        std::printf("golden_test_pixel_pipeline FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_pixel_pipeline PASS\n");
    return 0;
}
