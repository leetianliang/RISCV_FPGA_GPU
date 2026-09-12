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
        if (!(va == vb)) {                                                  \
            std::printf("FAIL %s:%d\n", __FILE__, __LINE__);                  \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

#define EXPECT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d\n", __FILE__, __LINE__);                  \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

using namespace golden;

u8 m8(u8 a, u8 b) { return static_cast<u8>((static_cast<u32>(a) * b + 127u) / 255u); }
u8 sat(int x) { return static_cast<u8>(x < 0 ? 0 : (x > 255 ? 255 : x)); }

struct Env {
    GoldenGPU gpu;
    Env() {
        gpu.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::ARGB8888}, "d");
        gpu.register_surface(SurfaceDesc{0x20000, 4, 1, 1, PixelFormat::ARGB8888}, "s");
    }
    void dst(u8 b, u8 g, u8 r, u8 a) {
        std::vector<u8> v = {b, g, r, a};
        gpu.memory().write_block(0x10000, v.data(), 4);
    }
    void src(u8 b, u8 g, u8 r, u8 a) {
        std::vector<u8> v = {b, g, r, a};
        gpu.memory().write_block(0x20000, v.data(), 4);
    }
    u32 read() {
        u32 w = 0;
        gpu.memory().read32(0x10000, w);
        return w;
    }
    ExecResult blit_premult(bool mod_en, Rgba8888 mod, bool global_en, u8 galpha,
                            bool pixel_en) {
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 4;
        d.dst_stride = 16;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.blend = static_cast<u32>(BlendMode::PREMULT_ALPHA);
        d.premult = true;
        d.pixel_alpha_en = pixel_en;
        d.color_mod_en = mod_en;
        d.primary_color = mod;
        d.global_alpha_en = global_en;
        d.global_alpha = galpha;
        d.w = 1;
        d.h = 1;
        return gpu.execute_command(make_blit_cmd(d));
    }
};

void expect_premult_pixel(u8 Sa, u8 Sr, u8 Sg, u8 Sb, u8 Da, u8 Dr, u8 Dg, u8 Db,
                          bool mod_en, u8 Mr, u8 Mg, u8 Mb, u8 Ma, bool g_en, u8 Ga,
                          u32 actual) {
    const u8 e1 = m8(255, mod_en ? Ma : 255);
    const u8 e2 = m8(e1, g_en ? Ga : 255);
    const u8 aextra = m8(e2, 255);
    const u8 aeff = m8(Sa, aextra);
    const u8 smr = mod_en ? m8(Sr, Mr) : Sr;
    const u8 smg = mod_en ? m8(Sg, Mg) : Sg;
    const u8 smb = mod_en ? m8(Sb, Mb) : Sb;
    const u8 dmul = static_cast<u8>(255 - aeff);
    const u8 er = sat(m8(smr, aextra) + m8(Dr, dmul));
    const u8 eg = sat(m8(smg, aextra) + m8(Dg, dmul));
    const u8 eb = sat(m8(smb, aextra) + m8(Db, dmul));
    const u8 ea = static_cast<u8>(aeff + m8(Da, dmul));
    // LE ARGB
    if ((actual & 0xFF) != eb || ((actual >> 8) & 0xFF) != eg ||
        ((actual >> 16) & 0xFF) != er || ((actual >> 24) & 0xFF) != ea) {
        std::printf("premult exp AARRGGBB=%02X%02X%02X%02X got=%08X\n", ea, er, eg, eb,
                    actual);
    }
    EXPECT_EQ(actual & 0xFF, eb);
    EXPECT_EQ((actual >> 8) & 0xFF, eg);
    EXPECT_EQ((actual >> 16) & 0xFF, er);
    EXPECT_EQ((actual >> 24) & 0xFF, ea);
}

void test_transparent() {
    Env e;
    e.dst(0xFF, 0x00, 0x00, 0xFF);  // opaque blue
    e.src(0x00, 0x00, 0x00, 0x00);  // transparent premult black
    EXPECT_TRUE(e.blit_premult(false, {}, false, 255, true).ok);
    // Aeff=0 → dest unchanged
    EXPECT_TRUE(e.read() == 0xFF0000FFu);
}

void test_opaque_premult() {
    Env e;
    e.dst(0x10, 0x20, 0x30, 0x40);
    e.src(0x00, 0x00, 0xC0, 0xFF);  // opaque premult R=192
    EXPECT_TRUE(e.blit_premult(false, {}, false, 255, true).ok);
    expect_premult_pixel(255, 192, 0, 0, 0x40, 0x30, 0x20, 0x10, false, 0, 0, 0, 255,
                         false, 255, e.read());
}

void test_mod_rgb_and_alpha() {
    Env e;
    e.dst(0x11, 0x22, 0x33, 0x44);
    e.src(0x00, 0x00, 0x80, 0x80);
    // Color-Mod RGB white; Color-Mod alpha = 50
    const auto mod = Rgba8888::pack(50, 255, 255, 255);
    EXPECT_TRUE(e.blit_premult(true, mod, false, 255, true).ok);
    expect_premult_pixel(128, 128, 0, 0, 0x44, 0x33, 0x22, 0x11, true, 255, 255, 255, 50,
                         false, 255, e.read());
}

void test_mod_rgb_mul() {
    Env e;
    e.dst(0x00, 0x00, 0x00, 0x00);
    e.src(0x00, 0x00, 0xC0, 0xFF);  // opaque R=192
    // A=255, R=128 modulator
    const auto mod = Rgba8888::pack(255, 128, 255, 255);
    EXPECT_TRUE(e.blit_premult(true, mod, false, 255, true).ok);
    const u8 smr = m8(192, 128);
    const u8 er = smr;  // opaque, aextra=255
    u32 w = e.read();
    EXPECT_EQ((w >> 16) & 0xFF, er);
}

void test_global_alpha() {
    Env e;
    e.dst(0x50, 0x60, 0x70, 0x80);
    e.src(0x00, 0x40, 0x40, 0xFF);
    EXPECT_TRUE(e.blit_premult(false, {}, true, 128, true).ok);
    expect_premult_pixel(255, 64, 64, 0, 0x80, 0x70, 0x60, 0x50, false, 0, 0, 0, 255,
                         true, 128, e.read());
}

void test_mod_plus_global() {
    Env e;
    e.dst(0x01, 0x02, 0x03, 0x04);
    // B=0, G=160, R=0, A=0  → Sr=0 Sg=160
    e.src(0x00, 0xA0, 0x00, 0x00);
    // A=200 R=255 G=0 B=0
    const auto mod = Rgba8888::pack(200, 255, 0, 0);
    EXPECT_TRUE(e.blit_premult(true, mod, true, 100, true).ok);
    expect_premult_pixel(0, 0, 160, 0, 0x04, 0x03, 0x02, 0x01, true, 255, 0, 0, 200, true,
                         100, e.read());
}

}  // namespace

int main() {
    test_transparent();
    test_opaque_premult();
    test_mod_rgb_and_alpha();
    test_mod_rgb_mul();
    test_global_alpha();
    test_mod_plus_global();
    if (g_failures) {
        std::printf("golden_test_premult_matrix FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_premult_matrix PASS\n");
    return 0;
}
