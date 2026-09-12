#include "golden/gpu_math.hpp"

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

u8 div255_rn(u32 x) noexcept {
    // Spec: floor((x + 127) / 255). Legal domain x <= 65025.
    return static_cast<u8>((x + 127u) / 255u);
}

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
    // den > 0
    if (num >= 0) {
        return (num + den / 2) / den;
    }
    return -((-num + den / 2) / den);
}

u8 lerp16(u8 a, u8 b, u16 f) noexcept {
    const u32 num = static_cast<u32>(a) * (65536u - static_cast<u32>(f)) +
                    static_cast<u32>(b) * static_cast<u32>(f) + 32768u;
    return static_cast<u8>(num >> 16);
}

}  // namespace golden
