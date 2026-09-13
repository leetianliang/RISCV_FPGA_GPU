#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;

using namespace golden;

// Independent primitives (do not call production blend).
u8 im8(u8 a, u8 b) { return static_cast<u8>((static_cast<u32>(a) * b + 127u) / 255u); }
u8 isat(int x) { return static_cast<u8>(x < 0 ? 0 : (x > 255 ? 255 : x)); }
u8 ialpha(u8 sa, bool pe, u8 ma, bool me, u8 ga, bool ge) {
    u8 a0 = pe ? sa : 255;
    u8 a1 = im8(a0, me ? ma : 255);
    u8 a2 = im8(a1, ge ? ga : 255);
    return im8(a2, 255);
}

u32 blend_pixel(BlendMode mode, Rgba8888 s, Rgba8888 d, bool pe, bool me, Rgba8888 mod,
                bool ge, u8 ga) {
    // key handled outside
    const u8 sm_r = me ? im8(s.r(), mod.r()) : s.r();
    const u8 sm_g = me ? im8(s.g(), mod.g()) : s.g();
    const u8 sm_b = me ? im8(s.b(), mod.b()) : s.b();
    const u8 aeff = ialpha(s.a(), pe, mod.a(), me, ga, ge);
    Rgba8888 o{};
    if (mode == BlendMode::COPY) {
        o = Rgba8888::pack(s.a(), sm_r, sm_g, sm_b);
    } else if (mode == BlendMode::STRAIGHT_ALPHA) {
        o = Rgba8888::pack(
            static_cast<u8>(aeff + im8(d.a(), static_cast<u8>(255 - aeff))),
            im8(sm_r, aeff) ? static_cast<u8>(im8(sm_r, aeff) + im8(d.r(), static_cast<u8>(255 - aeff)))
                            : im8(d.r(), static_cast<u8>(255 - aeff)),
            // use exact formula
            static_cast<u8>((static_cast<u32>(sm_g) * aeff +
                             static_cast<u32>(d.g()) * (255 - aeff) + 127) /
                            255),
            static_cast<u8>((static_cast<u32>(sm_b) * aeff +
                             static_cast<u32>(d.b()) * (255 - aeff) + 127) /
                            255));
        o = Rgba8888::pack(
            static_cast<u8>(aeff + im8(d.a(), static_cast<u8>(255 - aeff))),
            static_cast<u8>((static_cast<u32>(sm_r) * aeff +
                             static_cast<u32>(d.r()) * (255 - aeff) + 127) /
                            255),
            static_cast<u8>((static_cast<u32>(sm_g) * aeff +
                             static_cast<u32>(d.g()) * (255 - aeff) + 127) /
                            255),
            static_cast<u8>((static_cast<u32>(sm_b) * aeff +
                             static_cast<u32>(d.b()) * (255 - aeff) + 127) /
                            255));
    } else if (mode == BlendMode::ADD_SAT) {
        o = Rgba8888::pack(
            isat(d.a() + aeff), isat(d.r() + im8(sm_r, aeff)),
            isat(d.g() + im8(sm_g, aeff)), isat(d.b() + im8(sm_b, aeff)));
    }
    return o.value;
}

struct Rng {
    u32 s;
    explicit Rng(u32 seed) : s(seed) {}
    u32 next() {
        s = s * 1664525u + 1013904223u;
        return s;
    }
    u32 range(u32 n) { return n ? next() % n : 0; }
    u8 byte() { return static_cast<u8>(next() >> 24); }
};

void load_argb(MemoryImage& mem, u32 addr, u8 r, u8 g, u8 b, u8 a) {
    u8 px[4] = {b, g, r, a};
    mem.write_block(addr, px, 4);
}

void run_case(u32 seed) {
    Rng rng(seed);
    const u32 w = 8, h = 8, stride = 32;
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, stride, w, h, PixelFormat::ARGB8888}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 16, 4, 4, PixelFormat::ARGB8888}, "s");

    std::vector<Rgba8888> dst_log(w * h);
    std::vector<Rgba8888> src_log(4 * 4);
    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            Rgba8888 c = Rgba8888::pack(rng.byte(), rng.byte(), rng.byte(), rng.byte());
            dst_log[y * w + x] = c;
            load_argb(gpu.memory(), 0x10000 + y * stride + x * 4, c.r(), c.g(), c.b(),
                      c.a());
        }
    }
    for (u32 y = 0; y < 4; ++y) {
        for (u32 x = 0; x < 4; ++x) {
            Rgba8888 c = Rgba8888::pack(rng.byte(), rng.byte(), rng.byte(), rng.byte());
            src_log[y * 4 + x] = c;
            load_argb(gpu.memory(), 0x20000 + y * 16 + x * 4, c.r(), c.g(), c.b(),
                      c.a());
        }
    }

    BlitCmdDesc d;
    d.src_base = 0x20000;
    d.dst_base = 0x10000;
    d.src_stride = 16;
    d.dst_stride = stride;
    d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
    d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
    d.w = 3;
    d.h = 2;
    d.src_x = rng.range(2);
    d.src_y = rng.range(3);
    d.dst_x = static_cast<i32>(rng.range(6));
    d.dst_y = static_cast<i32>(rng.range(7));

    const u32 pick = rng.range(4);
    BlendMode mode = BlendMode::COPY;
    if (pick == 0) {
        mode = BlendMode::COPY;
    } else if (pick == 1) {
        mode = BlendMode::STRAIGHT_ALPHA;
        d.pixel_alpha_en = true;
        d.global_alpha_en = true;
        d.global_alpha = rng.byte();
    } else if (pick == 2) {
        mode = BlendMode::ADD_SAT;
        d.global_alpha_en = true;
        d.global_alpha = rng.byte();
    } else {
        mode = BlendMode::STRAIGHT_ALPHA;
        d.color_key_en = true;
        d.color_key_rgb = 0x00112233;
        d.global_alpha_en = true;
        d.global_alpha = rng.byte();
        d.pixel_alpha_en = true;
    }
    d.blend = static_cast<u32>(mode);
    d.primary_color = Rgba8888::pack(255, 255, 255, 255);

    const auto st = gpu.execute_command(make_blit_cmd(d));
    if (!st.ok) {
        std::printf("core_alpha exec fail seed=%u fault=%u\n", seed,
                    static_cast<u32>(st.fault));
        ++g_failures;
        return;
    }

    // Independent expected frame
    std::vector<Rgba8888> exp = dst_log;
    for (u32 ly = 0; ly < d.h; ++ly) {
        for (u32 lx = 0; lx < d.w; ++lx) {
            const u32 sx = d.src_x + lx;
            const u32 sy = d.src_y + ly;
            const u32 dx = static_cast<u32>(d.dst_x) + lx;
            const u32 dy = static_cast<u32>(d.dst_y) + ly;
            Rgba8888 s = src_log[sy * 4 + sx];
            if (d.color_key_en) {
                const u32 rgb = (static_cast<u32>(s.r()) << 16) |
                                (static_cast<u32>(s.g()) << 8) | s.b();
                if (rgb == d.color_key_rgb) {
                    continue;
                }
            }
            exp[dy * w + dx] =
                Rgba8888::from_u32(blend_pixel(mode, s, exp[dy * w + dx],
                                               d.pixel_alpha_en, false,
                                               Rgba8888::pack(255, 255, 255, 255),
                                               d.global_alpha_en, d.global_alpha));
        }
    }

    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            u32 word = 0;
            gpu.memory().read32(0x10000 + y * stride + x * 4, word);
            if (word != exp[y * w + x].value) {
                std::printf("core_alpha pixel mismatch seed=%u (%u,%u) exp=%08X got=%08X mode=%u\n",
                            seed, x, y, exp[y * w + x].value, word,
                            static_cast<u32>(mode));
                ++g_failures;
                return;
            }
        }
    }
}

}  // namespace

int main() {
    for (u32 i = 0; i < 40; ++i) {
        run_case(1000u + i * 37u);
    }
    if (g_failures) {
        std::printf("golden_test_core_alpha_diff FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_core_alpha_diff PASS\n");
    return 0;
}
