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

void test_endian() {
    golden::MemoryImage mem;
    EXPECT_TRUE(mem.register_region("a", 0x1000, 256).status ==
                golden::MemAccessStatus::OK);
    mem.write32(0x1000, 0x11223344u);
    golden::u8 b0 = 0, b1 = 0, b2 = 0, b3 = 0;
    mem.read8(0x1000, b0);
    mem.read8(0x1001, b1);
    mem.read8(0x1002, b2);
    mem.read8(0x1003, b3);
    EXPECT_TRUE(b0 == 0x44 && b1 == 0x33 && b2 == 0x22 && b3 == 0x11);
}

void test_unmapped_vs_oor() {
    golden::MemoryImage mem;
    mem.register_region("r", 0x100, 16);
    golden::u8 v = 0;
    EXPECT_TRUE(mem.read8(0x000, v).status == golden::MemAccessStatus::UNMAPPED);
    EXPECT_TRUE(mem.read8(0x10F, v).status == golden::MemAccessStatus::OK);
    // start in region, crosses end
    EXPECT_TRUE(mem.write32(0x10E, 1).status == golden::MemAccessStatus::OUT_OF_RANGE);
    // start outside
    EXPECT_TRUE(mem.read8(0x110, v).status == golden::MemAccessStatus::UNMAPPED);
}

void test_region_end_boundary() {
    golden::MemoryImage mem;
    EXPECT_TRUE(mem.register_region("hi", 0xFFFFFFF0u, 0x10).status ==
                golden::MemAccessStatus::OK);
    EXPECT_TRUE(mem.register_region("bad", 0xFFFFFFF0u, 0x11).status ==
                golden::MemAccessStatus::BAD_ARGUMENT);
}

void test_overlap() {
    golden::MemoryImage mem;
    EXPECT_TRUE(mem.register_region("r1", 0x1000, 0x100).status ==
                golden::MemAccessStatus::OK);
    EXPECT_TRUE(mem.register_region("r2", 0x1080, 0x100).status ==
                golden::MemAccessStatus::OVERLAP);
}

}  // namespace

int main() {
    test_endian();
    test_unmapped_vs_oor();
    test_region_end_boundary();
    test_overlap();
    if (g_failures) {
        std::printf("golden_test_memory_image FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_memory_image PASS\n");
    return 0;
}
