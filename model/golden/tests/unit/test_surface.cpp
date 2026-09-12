#include "golden/gpu_math.hpp"
#include "golden/memory_image.hpp"
#include "golden/surface.hpp"

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

void test_rgb565_surface_rw() {
    golden::MemoryImage mem;
    mem.register_region("fb", 0x10000, 16 * 32);
    golden::SurfaceDesc desc;
    desc.base = 0x10000;
    desc.stride = 32;
    desc.width = 16;
    desc.height = 16;
    desc.format = golden::PixelFormat::RGB565;
    golden::Surface surface(&mem, desc);

    const auto color = golden::Rgba8888::pack(255, 255, 0, 0);
    EXPECT_TRUE(surface.write_pixel(2, 3, color).ok);

    golden::u16 raw = 0;
    EXPECT_EQ(mem.read16(0x10000 + 3 * 32 + 2 * 2, raw).status,
              golden::MemAccessStatus::OK);
    EXPECT_EQ(raw, 0xF800);

    golden::Rgba8888 back{};
    EXPECT_TRUE(surface.read_pixel(2, 3, back).ok);
    EXPECT_EQ(back.r(), 255);
    EXPECT_EQ(back.g(), 0);
    EXPECT_EQ(back.b(), 0);
    EXPECT_EQ(back.a(), 255);

    // Out of bounds rejected
    EXPECT_TRUE(!surface.write_pixel(16, 0, color).ok);
    EXPECT_TRUE(!surface.write_pixel(0, 16, color).ok);
}

void test_non_tight_stride() {
    golden::MemoryImage mem;
    // width=4 RGB565 => 8 bytes; stride=16
    mem.register_region("fb", 0x20000, 16 * 4);
    golden::SurfaceDesc desc;
    desc.base = 0x20000;
    desc.stride = 16;
    desc.width = 4;
    desc.height = 4;
    desc.format = golden::PixelFormat::RGB565;
    golden::Surface surface(&mem, desc);

    const auto c = golden::Rgba8888::pack(255, 0, 0, 255);
    EXPECT_TRUE(surface.write_pixel(3, 2, c).ok);
    golden::u16 raw = 0;
    mem.read16(0x20000 + 2 * 16 + 3 * 2, raw);
    EXPECT_EQ(raw, 0x001F);
}

}  // namespace

int main() {
    test_rgb565_surface_rw();
    test_non_tight_stride();
    if (g_failures != 0) {
        std::printf("test_surface: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_surface: PASS\n");
    return 0;
}
