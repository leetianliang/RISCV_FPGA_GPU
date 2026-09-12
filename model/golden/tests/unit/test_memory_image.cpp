#include "golden/memory_image.hpp"

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

#define EXPECT_EQ(a, b)                                                       \
    do {                                                                      \
        const auto va = (a);                                                  \
        const auto vb = (b);                                                  \
        if (!(va == vb)) {                                                    \
            std::printf("FAIL %s:%d\n", __FILE__, __LINE__);                  \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

void test_endianness_and_rw() {
    golden::MemoryImage mem;
    EXPECT_EQ(mem.register_region("fb", 0x1000, 256).status,
              golden::MemAccessStatus::OK);

    EXPECT_EQ(mem.write32(0x1000, 0x11223344u).status, golden::MemAccessStatus::OK);
    golden::u8 b0 = 0, b1 = 0, b2 = 0, b3 = 0;
    EXPECT_EQ(mem.read8(0x1000, b0).status, golden::MemAccessStatus::OK);
    EXPECT_EQ(mem.read8(0x1001, b1).status, golden::MemAccessStatus::OK);
    EXPECT_EQ(mem.read8(0x1002, b2).status, golden::MemAccessStatus::OK);
    EXPECT_EQ(mem.read8(0x1003, b3).status, golden::MemAccessStatus::OK);
    EXPECT_EQ(b0, 0x44);
    EXPECT_EQ(b1, 0x33);
    EXPECT_EQ(b2, 0x22);
    EXPECT_EQ(b3, 0x11);

    EXPECT_EQ(mem.write16(0x1010, 0xABCDu).status, golden::MemAccessStatus::OK);
    golden::u16 v16 = 0;
    EXPECT_EQ(mem.read16(0x1010, v16).status, golden::MemAccessStatus::OK);
    EXPECT_EQ(v16, 0xABCD);

    golden::u32 v32 = 0;
    EXPECT_EQ(mem.read32(0x1000, v32).status, golden::MemAccessStatus::OK);
    EXPECT_EQ(v32, 0x11223344u);
}

void test_bounds_and_unmapped() {
    golden::MemoryImage mem;
    mem.register_region("a", 0x100, 16);
    golden::u8 v = 0;
    EXPECT_EQ(mem.read8(0x10F, v).status, golden::MemAccessStatus::OK);
    EXPECT_EQ(mem.read8(0x110, v).status, golden::MemAccessStatus::UNMAPPED);
    EXPECT_EQ(mem.read8(0x000, v).status, golden::MemAccessStatus::UNMAPPED);
    EXPECT_EQ(mem.write32(0x10E, 1).status, golden::MemAccessStatus::UNMAPPED);
}

void test_overlap_reject() {
    golden::MemoryImage mem;
    EXPECT_EQ(mem.register_region("r1", 0x1000, 0x100).status,
              golden::MemAccessStatus::OK);
    EXPECT_EQ(mem.register_region("r2", 0x1080, 0x100).status,
              golden::MemAccessStatus::OVERLAP);
    EXPECT_EQ(mem.register_region("r3", 0x2000, 0x100).status,
              golden::MemAccessStatus::OK);
}

void test_block_rw() {
    golden::MemoryImage mem;
    mem.register_region("blk", 0x8000, 64);
    std::vector<golden::u8> pattern(32);
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        pattern[i] = static_cast<golden::u8>(i * 3 + 1);
    }
    EXPECT_EQ(mem.write_block(0x8010, pattern.data(), pattern.size()).status,
              golden::MemAccessStatus::OK);
    std::vector<golden::u8> got;
    EXPECT_EQ(mem.read_block(0x8010, 32, got).status, golden::MemAccessStatus::OK);
    EXPECT_TRUE(got == pattern);
}

}  // namespace

int main() {
    test_endianness_and_rw();
    test_bounds_and_unmapped();
    test_overlap_reject();
    test_block_rw();
    if (g_failures != 0) {
        std::printf("test_memory_image: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_memory_image: PASS\n");
    return 0;
}
