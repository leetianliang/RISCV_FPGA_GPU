#include "golden/gpu_math.hpp"

#include <cstdio>
#include <cstdlib>

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
            std::printf("FAIL %s:%d: %s == %s (%llu vs %llu)\n", __FILE__,    \
                        __LINE__, #a, #b,                                     \
                        static_cast<unsigned long long>(va),                  \
                        static_cast<unsigned long long>(vb));                 \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

void test_sat_u8() {
    EXPECT_EQ(golden::sat_u8(-1), 0);
    EXPECT_EQ(golden::sat_u8(0), 0);
    EXPECT_EQ(golden::sat_u8(255), 255);
    EXPECT_EQ(golden::sat_u8(256), 255);
    EXPECT_EQ(golden::sat_u8(128), 128);
}

void test_div255_rn_exhaustive() {
    // Legal domain 0..65025 inclusive.
    for (golden::u32 x = 0; x <= 65025u; ++x) {
        const golden::u8 expected =
            static_cast<golden::u8>((x + 127u) / 255u);
        const golden::u8 got = golden::div255_rn(x);
        if (got != expected) {
            std::printf("FAIL div255_rn(%u) = %u expected %u\n", x, got, expected);
            ++g_failures;
            return;
        }
    }
}

void test_mul8_rn_exhaustive() {
    for (int a = 0; a <= 255; ++a) {
        for (int b = 0; b <= 255; ++b) {
            const golden::u8 expected = golden::div255_rn(
                static_cast<golden::u32>(a) * static_cast<golden::u32>(b));
            const golden::u8 got =
                golden::mul8_rn(static_cast<golden::u8>(a),
                                static_cast<golden::u8>(b));
            if (got != expected) {
                std::printf("FAIL mul8_rn(%d,%d) = %u expected %u\n", a, b, got,
                            expected);
                ++g_failures;
                return;
            }
        }
    }
}

void test_rgb565_roundtrip_exhaustive() {
    for (golden::u32 x = 0; x <= 0xFFFFu; ++x) {
        const auto px = static_cast<golden::u16>(x);
        const golden::Rgba8888 dec = golden::rgb565_decode(px);
        const golden::u16 enc = golden::rgb565_encode(dec);
        if (enc != px) {
            std::printf("FAIL rgb565 roundtrip %04X -> %04X\n", px, enc);
            ++g_failures;
            return;
        }
        if (dec.a() != 255) {
            std::printf("FAIL rgb565 alpha %04X -> %u\n", px, dec.a());
            ++g_failures;
            return;
        }
    }
}

void test_rgb565_directed() {
    const auto black = golden::rgb565_encode(golden::Rgba8888::pack(255, 0, 0, 0));
    const auto white = golden::rgb565_encode(golden::Rgba8888::pack(255, 255, 255, 255));
    const auto red = golden::rgb565_encode(golden::Rgba8888::pack(255, 255, 0, 0));
    const auto green = golden::rgb565_encode(golden::Rgba8888::pack(255, 0, 255, 0));
    const auto blue = golden::rgb565_encode(golden::Rgba8888::pack(255, 0, 0, 255));
    EXPECT_EQ(black, 0x0000);
    EXPECT_EQ(white, 0xFFFF);
    EXPECT_EQ(red, 0xF800);
    EXPECT_EQ(green, 0x07E0);
    EXPECT_EQ(blue, 0x001F);

    // Nontrivial: R=8,G=4,B=2 quantizes then expands (not a full inverse on 8-bit).
    const auto px = golden::rgb565_encode(golden::Rgba8888::pack(255, 8, 4, 2));
    const golden::Rgba8888 dec = golden::rgb565_decode(px);
    EXPECT_EQ(dec.r(), 8);
    EXPECT_EQ(dec.g(), 4);
    EXPECT_EQ(dec.b(), 0);
}

void test_q16() {
    EXPECT_EQ(golden::floor_q16_16(0x00000000), 0);
    EXPECT_EQ(golden::floor_q16_16(0x0000FFFF), 0);
    EXPECT_EQ(golden::floor_q16_16(0x00010000), 1);
    EXPECT_EQ(golden::floor_q16_16(0x00018000), 1);
    EXPECT_EQ(golden::floor_q16_16(static_cast<golden::i32>(0xFFFF0000)), -1);
    EXPECT_EQ(golden::floor_q16_16(static_cast<golden::i32>(0xFFFF8000)), -1);
    EXPECT_EQ(golden::floor_q16_16(static_cast<golden::i32>(0xFFFEFFFF)), -2);

    EXPECT_EQ(golden::frac_q16_16(0x00018000), 0x8000);
    EXPECT_EQ(golden::frac_q16_16(static_cast<golden::i32>(0xFFFF8000)), 0x8000);
}

void test_round_div_signed() {
    EXPECT_EQ(golden::round_div_signed(0, 2), 0);
    EXPECT_EQ(golden::round_div_signed(1, 2), 1);   // 0.5 -> 1
    EXPECT_EQ(golden::round_div_signed(3, 2), 2);   // 1.5 -> 2
    // Spec: (num + den/2)/den for num>=0 => (5+1)/2 = 3
    EXPECT_EQ(golden::round_div_signed(5, 2), 3);
    EXPECT_EQ(golden::round_div_signed(-1, 2), -1);
    EXPECT_EQ(golden::round_div_signed(-3, 2), -2);
    EXPECT_EQ(golden::round_div_signed(-5, 2), -3);
    EXPECT_EQ(golden::round_div_signed(2, 2), 1);
    EXPECT_EQ(golden::round_div_signed(-2, 2), -1);
}

void test_lerp16() {
    EXPECT_EQ(golden::lerp16(0, 255, 0x0000), 0);
    EXPECT_EQ(golden::lerp16(0, 255, 0x8000), 128);
    EXPECT_EQ(golden::lerp16(0, 255, 0xFFFF), 255);
    EXPECT_EQ(golden::lerp16(100, 200, 0x0000), 100);
    EXPECT_EQ(golden::lerp16(100, 200, 0xFFFF), 200);
}

}  // namespace

int main() {
    test_sat_u8();
    test_div255_rn_exhaustive();
    test_mul8_rn_exhaustive();
    test_rgb565_roundtrip_exhaustive();
    test_rgb565_directed();
    test_q16();
    test_round_div_signed();
    test_lerp16();

    if (g_failures != 0) {
        std::printf("golden_unit_tests: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("golden_unit_tests: PASS\n");
    return 0;
}
