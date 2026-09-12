#pragma once

#include "golden/gpu_types.hpp"

namespace golden {

// Bit-exact scalar helpers. No floating point, no SIMD.

u8 sat_u8(int x) noexcept;

// DIV255_RN(x) = floor((x + 127) / 255) for legal x in [0, 65025].
u8 div255_rn(u32 x) noexcept;

// MUL8_RN(a,b) = DIV255_RN(a * b)
u8 mul8_rn(u8 a, u8 b) noexcept;

u8 expand5(u8 v) noexcept;
u8 expand6(u8 v) noexcept;

// RGB565 -> canonical RGBA8888 with A=255 (bit replication).
Rgba8888 rgb565_decode(u16 px) noexcept;

// RGBA8888 RGB channels -> RGB565, alpha discarded (no dither).
u16 rgb565_encode(Rgba8888 c) noexcept;

i32 floor_q16_16(i32 q) noexcept;
u16 frac_q16_16(i32 q) noexcept;

// den must be > 0.
i64 round_div_signed(i64 num, i64 den) noexcept;

u8 lerp16(u8 a, u8 b, u16 f) noexcept;

}  // namespace golden
