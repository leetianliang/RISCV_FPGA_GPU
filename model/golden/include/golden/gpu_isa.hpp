#pragma once

#include "golden/gpu_types.hpp"

#include <array>
#include <cstddef>

namespace golden {

inline constexpr u32 kCommandDwCount = 16;
inline constexpr u32 kCommandByteCount = 64;
inline constexpr u32 kCmdEncodingVersion = 0x1;
inline constexpr u32 kCmdLengthDw = 16;

inline constexpr u32 kClassDraw2D = 0x1;
inline constexpr u32 kOpcodeFillRect = 0x00;
inline constexpr u32 kOpcodeBlit = 0x01;
inline constexpr u32 kOpcodeBlitExt = 0x02;
inline constexpr u32 kOpcodeTileFrame = 0x10;

inline constexpr u32 kHIrqOnRetire = 1u << 0;
inline constexpr u32 kHTrace = 1u << 1;
inline constexpr u32 kHExtValid = 1u << 2;
inline constexpr u32 kHStrict = 1u << 3;
inline constexpr u32 kHHdrReservedMask = 0xF0u;

inline constexpr u32 extract_src_format(u32 draw_state) noexcept {
    return draw_state & 0xFu;
}
inline constexpr u32 extract_dst_format(u32 draw_state) noexcept {
    return (draw_state >> 4) & 0xFu;
}
inline constexpr u32 extract_blend_mode(u32 draw_state) noexcept {
    return (draw_state >> 8) & 0xFu;
}
inline constexpr u32 extract_filter_mode(u32 draw_state) noexcept {
    return (draw_state >> 12) & 0x3u;
}
inline constexpr u32 extract_addr_mode_u(u32 draw_state) noexcept {
    return (draw_state >> 14) & 0x3u;
}
inline constexpr u32 extract_addr_mode_v(u32 draw_state) noexcept {
    return (draw_state >> 16) & 0x3u;
}
inline constexpr bool extract_color_key_en(u32 draw_state) noexcept {
    return ((draw_state >> 18) & 1u) != 0;
}
inline constexpr bool extract_global_alpha_en(u32 draw_state) noexcept {
    return ((draw_state >> 19) & 1u) != 0;
}
inline constexpr bool extract_pixel_alpha_en(u32 draw_state) noexcept {
    return ((draw_state >> 20) & 1u) != 0;
}
inline constexpr bool extract_flip_x(u32 draw_state) noexcept {
    return ((draw_state >> 21) & 1u) != 0;
}
inline constexpr bool extract_flip_y(u32 draw_state) noexcept {
    return ((draw_state >> 22) & 1u) != 0;
}
inline constexpr bool extract_palette_en(u32 draw_state) noexcept {
    return ((draw_state >> 23) & 1u) != 0;
}
inline constexpr bool extract_premult_src(u32 draw_state) noexcept {
    return ((draw_state >> 24) & 1u) != 0;
}
inline constexpr bool extract_clip_en(u32 draw_state) noexcept {
    return ((draw_state >> 25) & 1u) != 0;
}
inline constexpr bool extract_color_mod_en(u32 draw_state) noexcept {
    return ((draw_state >> 26) & 1u) != 0;
}
inline constexpr bool extract_dither_en(u32 draw_state) noexcept {
    return ((draw_state >> 27) & 1u) != 0;
}
inline constexpr bool extract_depth_test_en(u32 draw_state) noexcept {
    return ((draw_state >> 28) & 1u) != 0;
}
inline constexpr u32 extract_draw_state_reserved(u32 draw_state) noexcept {
    return (draw_state >> 29) & 0x7u;
}

inline constexpr u32 pack_draw_state(u32 src_format, u32 dst_format, u32 blend_mode,
                                     u32 filter_mode = 0) noexcept {
    return (src_format & 0xFu) | ((dst_format & 0xFu) << 4) |
           ((blend_mode & 0xFu) << 8) | ((filter_mode & 0x3u) << 12);
}

using GpuCmd64 = std::array<u32, kCommandDwCount>;

static_assert(sizeof(GpuCmd64) == kCommandByteCount);
static_assert(GpuCmd64{}.size() == 16);

struct CmdHeader {
    u32 cmd_class = 0;
    u32 opcode = 0;
    u32 version = 0;
    u32 length_dw = 0;
    u32 hdr_flags = 0;
    u32 sequence_id = 0;
    u32 user_tag = 0;
    u32 ext_ptr = 0;
};

inline constexpr u32 header_word(u32 cmd_class, u32 opcode, u32 version,
                                 u32 length_dw, u32 hdr_flags) noexcept {
    return (cmd_class << 28) | (opcode << 20) | (version << 16) |
           (length_dw << 8) | (hdr_flags & 0xFFu);
}

inline constexpr u32 pack_xy_u16(u32 x, u32 y) noexcept {
    return ((y & 0xFFFFu) << 16) | (x & 0xFFFFu);
}

inline constexpr u32 pack_xy_i16(i32 x, i32 y) noexcept {
    return pack_xy_u16(static_cast<u32>(static_cast<u16>(x)),
                       static_cast<u32>(static_cast<u16>(y)));
}

inline constexpr u32 pack_wh(u32 w, u32 h) noexcept { return pack_xy_u16(w, h); }

inline constexpr i32 unpack_s16_lo(u32 word) noexcept {
    return static_cast<i32>(static_cast<i16>(word & 0xFFFFu));
}
inline constexpr i32 unpack_s16_hi(u32 word) noexcept {
    return static_cast<i32>(static_cast<i16>((word >> 16) & 0xFFFFu));
}
inline constexpr u32 unpack_u16_lo(u32 word) noexcept { return word & 0xFFFFu; }
inline constexpr u32 unpack_u16_hi(u32 word) noexcept { return (word >> 16) & 0xFFFFu; }

// Explicit little-endian 64B serialization (not host object layout).
inline std::array<u8, kCommandByteCount> serialize_cmd_le(const GpuCmd64& cmd) noexcept {
    std::array<u8, kCommandByteCount> out{};
    for (u32 i = 0; i < kCommandDwCount; ++i) {
        const u32 w = cmd[i];
        out[i * 4 + 0] = static_cast<u8>(w & 0xFFu);
        out[i * 4 + 1] = static_cast<u8>((w >> 8) & 0xFFu);
        out[i * 4 + 2] = static_cast<u8>((w >> 16) & 0xFFu);
        out[i * 4 + 3] = static_cast<u8>((w >> 24) & 0xFFu);
    }
    return out;
}

inline bool deserialize_cmd_le(const u8* bytes, std::size_t size, GpuCmd64& out) noexcept {
    if (bytes == nullptr || size != kCommandByteCount) {
        return false;
    }
    for (u32 i = 0; i < kCommandDwCount; ++i) {
        out[i] = static_cast<u32>(bytes[i * 4 + 0]) |
                 (static_cast<u32>(bytes[i * 4 + 1]) << 8) |
                 (static_cast<u32>(bytes[i * 4 + 2]) << 16) |
                 (static_cast<u32>(bytes[i * 4 + 3]) << 24);
    }
    return true;
}

}  // namespace golden
