#include "golden/gpu_isa.hpp"

#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

#define EXPECT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

void test_known_bytes() {
    golden::GpuCmd64 cmd{};
    cmd[0] = 0x10001000u;  // example
    const auto bytes = golden::serialize_cmd_le(cmd);
    EXPECT_TRUE(bytes[0] == 0x00 && bytes[1] == 0x10 && bytes[2] == 0x00 &&
                bytes[3] == 0x10);
    golden::GpuCmd64 back{};
    EXPECT_TRUE(golden::deserialize_cmd_le(bytes.data(), bytes.size(), back));
    EXPECT_TRUE(back == cmd);
}

void test_roundtrip_pattern() {
    golden::GpuCmd64 cmd{};
    for (golden::u32 i = 0; i < 16; ++i) {
        cmd[i] = 0xA5A50000u | i;
    }
    const auto bytes = golden::serialize_cmd_le(cmd);
    golden::GpuCmd64 back{};
    EXPECT_TRUE(golden::deserialize_cmd_le(bytes.data(), 64, back));
    EXPECT_TRUE(back == cmd);
}

void test_wrong_size() {
    golden::GpuCmd64 cmd{};
    const auto bytes = golden::serialize_cmd_le(cmd);
    golden::GpuCmd64 back{};
    EXPECT_TRUE(!golden::deserialize_cmd_le(bytes.data(), 63, back));
    EXPECT_TRUE(!golden::deserialize_cmd_le(bytes.data(), 65, back));
    EXPECT_TRUE(!golden::deserialize_cmd_le(nullptr, 64, back));
}

}  // namespace

int main() {
    test_known_bytes();
    test_roundtrip_pattern();
    test_wrong_size();
    if (g_failures) {
        std::printf("golden_test_cmd_io FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_cmd_io PASS\n");
    return 0;
}
