#include "golden/gpu_math.hpp"

#include <cstring>
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
    // den > 0; ties away from zero; overflow-safe for INT64_MIN.
    if (den <= 0) {
        return 0;
    }
    if (num >= 0) {
        return (num + den / 2) / den;
    }
    const u64 unneg =
        (num == std::numeric_limits<i64>::min())
            ? (0u - static_cast<u64>(num))  // well-defined modular negation
            : static_cast<u64>(-num);
    const u64 q = (unneg + static_cast<u64>(den / 2)) / static_cast<u64>(den);
    return -static_cast<i64>(q);
}

u8 lerp16(u8 a, u8 b, u16 f) noexcept {
    const u32 num = static_cast<u32>(a) * (65536u - static_cast<u32>(f)) +
                    static_cast<u32>(b) * static_cast<u32>(f) + 32768u;
    return static_cast<u8>(num >> 16);
}

u8 effective_alpha(u8 src_a, bool pixel_alpha_en, u8 global_a, bool global_en) noexcept {
    // Staged rounding; coverage=255; color-mod deferred=255.
    const u8 a0 = pixel_alpha_en ? src_a : 255u;
    const u8 a1 = mul8_rn(a0, 255u);  // color-mod factor 255
    const u8 a2 = mul8_rn(a1, global_en ? global_a : 255u);
    const u8 a3 = mul8_rn(a2, 255u);  // coverage 255
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
    const int sum = static_cast<int>(d) + static_cast<int>(src_contrib);
    return sat_u8(sum);
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
            const u16 px = static_cast<u16>(static_cast<u16>(p[0]) |
                                            (static_cast<u16>(p[1]) << 8));
            return rgb565_decode(px);
        }
        case PixelFormat::ARGB8888: {
            // LE bytes: B,G,R,A -> logical 0xAARRGGBB
            return Rgba8888::pack(p[3], p[2], p[1], p[0]);
        }
        case PixelFormat::XRGB8888: {
            // LE bytes: B,G,R,X -> RGB, A=255
            return Rgba8888::pack(255, p[2], p[1], p[0]);
        }
        case PixelFormat::INDEX8:
            break;
    }
    return Rgba8888{};
}

}  // namespace golden
