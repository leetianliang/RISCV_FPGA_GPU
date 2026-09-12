#include "golden/gpu_math.hpp"

#include <limits>

namespace golden {

u8 sat_u8(int x) noexcept {
    if (x < 0) {
        return 0;
    }
    if (x > 255) {
        return 255;
    }
    return static_cast<u8>(x);
}

u8 div255_rn(u32 x) noexcept { return static_cast<u8>((x + 127u) / 255u); }

u8 mul8_rn(u8 a, u8 b) noexcept {
    return div255_rn(static_cast<u32>(a) * static_cast<u32>(b));
}

u8 expand5(u8 v) noexcept {
    return static_cast<u8>((static_cast<u8>(v << 3)) | static_cast<u8>(v >> 2));
}

u8 expand6(u8 v) noexcept {
    return static_cast<u8>((static_cast<u8>(v << 2)) | static_cast<u8>(v >> 4));
}

Rgba8888 rgb565_decode(u16 px) noexcept {
    const u8 r5 = static_cast<u8>((px >> 11) & 0x1Fu);
    const u8 g6 = static_cast<u8>((px >> 5) & 0x3Fu);
    const u8 b5 = static_cast<u8>(px & 0x1Fu);
    return Rgba8888::pack(255, expand5(r5), expand6(g6), expand5(b5));
}

u16 rgb565_encode(Rgba8888 c) noexcept {
    const u32 r5 = (static_cast<u32>(c.r()) * 31u + 127u) / 255u;
    const u32 g6 = (static_cast<u32>(c.g()) * 63u + 127u) / 255u;
    const u32 b5 = (static_cast<u32>(c.b()) * 31u + 127u) / 255u;
    return static_cast<u16>((r5 << 11) | (g6 << 5) | b5);
}

namespace {
constexpr u8 kBayer4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

u8 dither_quant(u8 c, u32 m, u8 b) {
    const u32 p = static_cast<u32>(c) * m;
    const u32 base = p / 255u;
    const u32 rem = p % 255u;
    if (base < m && rem * 32u > (2u * b + 1u) * 255u) {
        return static_cast<u8>(base + 1u);
    }
    return static_cast<u8>(base);
}
}  // namespace

u16 rgb565_encode_dither(Rgba8888 c, u32 x, u32 y) noexcept {
    const u8 b = kBayer4[y & 3u][x & 3u];
    const u32 r5 = dither_quant(c.r(), 31u, b);
    const u32 g6 = dither_quant(c.g(), 63u, b);
    const u32 b5 = dither_quant(c.b(), 31u, b);
    return static_cast<u16>((r5 << 11) | (g6 << 5) | b5);
}

i32 floor_q16_16(i32 q) noexcept {
    const i64 x = q;
    if (x >= 0) {
        return static_cast<i32>(x / 65536);
    }
    return -static_cast<i32>(((-x) + 65535) / 65536);
}

u16 frac_q16_16(i32 q) noexcept {
    return static_cast<u16>(static_cast<u32>(q) & 0xFFFFu);
}

i64 round_div_signed(i64 num, i64 den) noexcept {
    // den > 0; ties away from zero; quotient/remainder, no overflow-prone abs add.
    if (den <= 0) {
        return 0;
    }
    const bool neg = num < 0;
    u64 unneg;
    if (num == std::numeric_limits<i64>::min()) {
        unneg = static_cast<u64>(std::numeric_limits<i64>::max()) + 1ull;
    } else {
        unneg = static_cast<u64>(neg ? -num : num);
    }
    const u64 uden = static_cast<u64>(den);
    const u64 q = unneg / uden;
    const u64 r = unneg % uden;
    // round: if 2*r >= den, +1 (ties away from zero after abs)
    u64 qr = q;
    if (r * 2ull >= uden) {
        qr += 1ull;
    }
    if (!neg) {
        return static_cast<i64>(qr);
    }
    // Avoid converting out-of-range unsigned to signed: if qr > INT64_MAX, result is INT64_MIN
    if (qr > static_cast<u64>(std::numeric_limits<i64>::max())) {
        return std::numeric_limits<i64>::min();
    }
    return -static_cast<i64>(qr);
}

i32 nearest_index_q16(i32 q) noexcept {
    const i64 t = static_cast<i64>(q) + 32768;
    if (t > std::numeric_limits<i32>::max()) {
        return floor_q16_16(std::numeric_limits<i32>::max());
    }
    if (t < std::numeric_limits<i32>::min()) {
        return floor_q16_16(std::numeric_limits<i32>::min());
    }
    return floor_q16_16(static_cast<i32>(t));
}

u8 lerp16(u8 a, u8 b, u16 f) noexcept {
    const u32 num = static_cast<u32>(a) * (65536u - static_cast<u32>(f)) +
                    static_cast<u32>(b) * static_cast<u32>(f) + 32768u;
    return static_cast<u8>(num >> 16);
}

u8 effective_alpha(u8 src_a, bool pixel_alpha_en, u8 mod_a, bool mod_en, u8 global_a,
                   bool global_en) noexcept {
    const u8 a0 = pixel_alpha_en ? src_a : 255u;
    const u8 a1 = mul8_rn(a0, mod_en ? mod_a : 255u);
    const u8 a2 = mul8_rn(a1, global_en ? global_a : 255u);
    const u8 a3 = mul8_rn(a2, 255u);
    return a3;
}

u8 blend_straight_chan(u8 s, u8 d, u8 a) noexcept {
    const u32 num = static_cast<u32>(s) * a + static_cast<u32>(d) * (255u - a);
    return div255_rn(num);
}

u8 source_over_alpha(u8 a_src, u8 a_dst) noexcept {
    return static_cast<u8>(a_src + mul8_rn(a_dst, static_cast<u8>(255u - a_src)));
}

u8 add_sat_chan(u8 d, u8 src_contrib) noexcept {
    return sat_u8(static_cast<int>(d) + static_cast<int>(src_contrib));
}

u32 bytes_per_pixel(PixelFormat format) noexcept {
    switch (format) {
        case PixelFormat::RGB565:
            return 2;
        case PixelFormat::ARGB8888:
        case PixelFormat::XRGB8888:
            return 4;
        case PixelFormat::INDEX8:
            return 1;
    }
    return 0;
}

Rgba8888 decode_source_pixel(PixelFormat format, const u8* p) noexcept {
    switch (format) {
        case PixelFormat::RGB565: {
            const u16 px =
                static_cast<u16>(static_cast<u16>(p[0]) | (static_cast<u16>(p[1]) << 8));
            return rgb565_decode(px);
        }
        case PixelFormat::ARGB8888:
            return Rgba8888::pack(p[3], p[2], p[1], p[0]);
        case PixelFormat::XRGB8888:
            return Rgba8888::pack(255, p[2], p[1], p[0]);
        case PixelFormat::INDEX8:
            break;
    }
    return Rgba8888{};
}

Rgba8888 color_modulate(Rgba8888 s, Rgba8888 m, bool en) noexcept {
    if (!en) {
        return s;
    }
    return Rgba8888::pack(s.a(), mul8_rn(s.r(), m.r()), mul8_rn(s.g(), m.g()),
                          mul8_rn(s.b(), m.b()));
}

}  // namespace golden
