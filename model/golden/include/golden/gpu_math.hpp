#pragma once

#include "golden/gpu_types.hpp"

namespace golden {

u8 sat_u8(int x) noexcept;
u8 div255_rn(u32 x) noexcept;
u8 mul8_rn(u8 a, u8 b) noexcept;
u8 expand5(u8 v) noexcept;
u8 expand6(u8 v) noexcept;
Rgba8888 rgb565_decode(u16 px) noexcept;
u16 rgb565_encode(Rgba8888 c) noexcept;
u16 rgb565_encode_dither(Rgba8888 c, u32 x, u32 y) noexcept;
i32 floor_q16_16(i32 q) noexcept;
u16 frac_q16_16(i32 q) noexcept;
i64 round_div_signed(i64 num, i64 den) noexcept;
i32 nearest_index_q16(i32 q) noexcept;
u8 lerp16(u8 a, u8 b, u16 f) noexcept;

u8 effective_alpha(u8 src_a, bool pixel_alpha_en, u8 mod_a, bool mod_en, u8 global_a,
                   bool global_en) noexcept;
u8 blend_straight_chan(u8 s, u8 d, u8 a) noexcept;
u8 source_over_alpha(u8 a_src, u8 a_dst) noexcept;
u8 add_sat_chan(u8 d, u8 src_contrib) noexcept;
u32 bytes_per_pixel(PixelFormat format) noexcept;
Rgba8888 decode_source_pixel(PixelFormat format, const u8* p) noexcept;
Rgba8888 color_modulate(Rgba8888 s, Rgba8888 m, bool en) noexcept;

}  // namespace golden
