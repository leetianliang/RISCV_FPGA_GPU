#include "golden/gpu_isa.hpp"
#include "golden/gpu_math.hpp"
#include "golden/gpu_types.hpp"

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
            std::printf("FAIL %s:%d (%llu vs %llu)\n", __FILE__, __LINE__,    \
                        static_cast<unsigned long long>(va),                  \
                        static_cast<unsigned long long>(vb));                 \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

void test_fault_codes() {
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::NONE), 0x0000u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_CMD_CLASS), 0x0001u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_OPCODE), 0x0002u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_VERSION), 0x0003u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_LENGTH), 0x0004u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::RESERVED_NONZERO), 0x0005u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::UNSUPPORTED_FEATURE), 0x0006u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_ALIGNMENT), 0x0007u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_EXT_PTR), 0x0008u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_EXT_TYPE), 0x0009u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_FORMAT), 0x000Au);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_BLEND), 0x000Bu);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_FILTER), 0x000Cu);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_RECT), 0x000Du);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_RING_CONFIG), 0x000Eu);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_TILE_CONFIG), 0x000Fu);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::MEMORY_ERROR), 0x0013u);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_BLEND_STATE), 0x001Au);
    EXPECT_EQ(static_cast<unsigned>(golden::FaultCode::BAD_ADDRESS), 0x001Bu);
}

void test_div_mul_rgb565() {
    for (golden::u32 x = 0; x <= 65025u; ++x) {
        if (golden::div255_rn(x) != static_cast<golden::u8>((x + 127u) / 255u)) {
            EXPECT_TRUE(false);
            break;
        }
    }
    for (int a = 0; a <= 255; ++a) {
        for (int b = 0; b <= 255; ++b) {
            if (golden::mul8_rn(static_cast<golden::u8>(a),
                                static_cast<golden::u8>(b)) !=
                golden::div255_rn(static_cast<golden::u32>(a) * b)) {
                EXPECT_TRUE(false);
                return;
            }
        }
    }
    for (golden::u32 x = 0; x <= 0xFFFFu; ++x) {
        const auto px = static_cast<golden::u16>(x);
        if (golden::rgb565_encode(golden::rgb565_decode(px)) != px) {
            EXPECT_TRUE(false);
            return;
        }
    }
}

void test_round_div_int64_min() {
    EXPECT_EQ(golden::round_div_signed(std::numeric_limits<golden::i64>::min(), 2),
              golden::round_div_signed(static_cast<golden::i64>(-9223372036854775807LL - 1), 2));
    // ties away from zero
    EXPECT_EQ(golden::round_div_signed(5, 2), 3);
    EXPECT_EQ(golden::round_div_signed(-5, 2), -3);
    EXPECT_EQ(golden::round_div_signed(1, 2), 1);
    EXPECT_EQ(golden::round_div_signed(-1, 2), -1);
}

void test_lerp_and_q16() {
    EXPECT_EQ(golden::lerp16(0, 255, 0x8000), 128);
    EXPECT_EQ(golden::floor_q16_16(0x00018000), 1);
    EXPECT_EQ(golden::floor_q16_16(static_cast<golden::i32>(0xFFFF8000)), -1);
}

}  // namespace

int main() {
    test_fault_codes();
    test_div_mul_rgb565();
    test_round_div_int64_min();
    test_lerp_and_q16();
    if (g_failures) {
        std::printf("golden_test_gpu_math FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_gpu_math PASS\n");
    return 0;
}
