#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;

#define EXPECT_EQ(a, b)                                                       \
    do {                                                                      \
        const auto va = (a);                                                  \
        const auto vb = (b);                                                  \
        if (!(va == vb)) {                                                    \
            std::printf("FAIL %s:%d (%d vs %d)\n", __FILE__, __LINE__,        \
                        static_cast<int>(va), static_cast<int>(vb));          \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

using namespace golden;

// Independent expected premult math (does not call production blend).
u8 indep_mul8(u8 a, u8 b) { return static_cast<u8>((static_cast<u32>(a) * b + 127u) / 255u); }
u8 indep_sat(int x) { return x < 0 ? 0 : (x > 255 ? 255 : static_cast<u8>(x)); }

void test_premult_over_dst() {
    // Source premult: A=128, RGB already *128/255 approx: use 64,0,0 with A=128
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::ARGB8888}, "d");
    // init dst opaque blue ARGB LE B,G,R,A = FF,00,00,FF → 0xFFFF0000 logical AA RR GG BB = FF 00 00 FF
    // logical 0xFF0000FF is A=FF R=00 G=00 B=FF
    std::vector<u8> init = {0xFF, 0x00, 0x00, 0xFF};  // B,G,R,A
    gpu.memory().write_block(0x10000, init.data(), init.size());

    // FILL with PREMULT path via BLIT 1x1 from small ARGB texture is heavier;
    // use BLIT from ARGB source.
    gpu.register_surface(SurfaceDesc{0x20000, 4, 1, 1, PixelFormat::ARGB8888}, "s");
    // premult red A=128: R=64 (approx), store B=0,G=0,R=64,A=128
    std::vector<u8> src = {0x00, 0x00, 0x40, 0x80};
    gpu.memory().write_block(0x20000, src.data(), src.size());

    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 4;
    d.dst_stride = 16;
    d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
    d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
    d.blend = static_cast<u32>(BlendMode::PREMULT_ALPHA);
    d.premult = true;
    d.pixel_alpha_en = true;
    d.w = 1;
    d.h = 1;
    EXPECT_EQ(static_cast<int>(gpu.execute_command(make_blit_cmd(d)).ok), 1);

    // Expected: Sa=128, Aextra=255, Aeff=MUL8(128,255)=128
    // Scontrib = MUL8(64,255)=64
    // O.r = SAT(64 + MUL8(0, 127)) = 64
    // O.b = SAT(0 + MUL8(255, 127)) = 127
    // O.a = 128 + MUL8(255,127)
    const u8 aeff = indep_mul8(128, 255);
    const u8 dmul = static_cast<u8>(255 - aeff);
    const u8 expect_r = indep_sat(indep_mul8(64, 255) + indep_mul8(0, dmul));
    const u8 expect_b = indep_sat(0 + indep_mul8(255, dmul));
    const u8 expect_a = static_cast<u8>(aeff + indep_mul8(255, dmul));

    u32 word = 0;
    gpu.memory().read32(0x10000, word);
    const u8 b = static_cast<u8>(word & 0xFF);
    const u8 g = static_cast<u8>((word >> 8) & 0xFF);
    const u8 r = static_cast<u8>((word >> 16) & 0xFF);
    const u8 a = static_cast<u8>((word >> 24) & 0xFF);
    EXPECT_EQ(r, expect_r);
    EXPECT_EQ(g, 0);
    EXPECT_EQ(b, expect_b);
    EXPECT_EQ(a, expect_a);
    // Destination attenuation must have occurred (b != original 255)
    EXPECT_EQ(b == 255 ? 0 : 1, 1);
}

void test_no_double_color_mod() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::ARGB8888}, "d");
    std::vector<u8> init = {0x00, 0x00, 0x00, 0x00};
    gpu.memory().write_block(0x10000, init.data(), init.size());
    gpu.register_surface(SurfaceDesc{0x20000, 4, 1, 1, PixelFormat::ARGB8888}, "s");
    std::vector<u8> src = {0x00, 0x00, 0xC0, 0xFF};  // A=255 R=192
    gpu.memory().write_block(0x20000, src.data(), src.size());

    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 4;
    d.dst_stride = 16;
    d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
    d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
    d.blend = static_cast<u32>(BlendMode::COPY);
    d.color_mod_en = false;
    d.primary_color = Rgba8888::pack(0, 0, 0, 0);  // zeroed default must not wipe
    d.w = 1;
    d.h = 1;
    EXPECT_EQ(static_cast<int>(gpu.execute_command(make_blit_cmd(d)).ok), 1);
    u32 word = 0;
    gpu.memory().read32(0x10000, word);
    const u8 r = static_cast<u8>((word >> 16) & 0xFF);
    EXPECT_EQ(r, 192);
}

}  // namespace

int main() {
    test_premult_over_dst();
    test_no_double_color_mod();
    if (g_failures) {
        std::printf("golden_test_premult_exact FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_premult_exact PASS\n");
    return 0;
}
