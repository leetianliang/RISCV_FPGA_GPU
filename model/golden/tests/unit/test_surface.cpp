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

void test_rgb565_rw() {
    golden::MemoryImage mem;
    mem.register_region("fb", 0x10000, 16 * 32);
    golden::SurfaceDesc d;
    d.base = 0x10000;
    d.stride = 32;
    d.width = 16;
    d.height = 16;
    d.format = golden::PixelFormat::RGB565;
    golden::Surface s(&mem, d);
    EXPECT_TRUE(s.write_pixel(2, 3, golden::Rgba8888::pack(255, 255, 0, 0)).ok);
    golden::u16 raw = 0;
    mem.read16(0x10000 + 3 * 32 + 4, raw);
    EXPECT_TRUE(raw == 0xF800);
    EXPECT_TRUE(!s.write_pixel(16, 0, golden::Rgba8888{}).ok);
}

void test_argb_decode() {
    const golden::u8 bytes[4] = {0x11, 0x22, 0x33, 0x44};  // B,G,R,A
    const auto c = golden::decode_source_pixel(golden::PixelFormat::ARGB8888, bytes);
    EXPECT_TRUE(c.a() == 0x44 && c.r() == 0x33 && c.g() == 0x22 && c.b() == 0x11);
    const auto x = golden::decode_source_pixel(golden::PixelFormat::XRGB8888, bytes);
    EXPECT_TRUE(x.a() == 255 && x.r() == 0x33);
}

}  // namespace

int main() {
    test_rgb565_rw();
    test_argb_decode();
    if (g_failures) {
        std::printf("golden_test_surface FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_surface PASS\n");
    return 0;
}
