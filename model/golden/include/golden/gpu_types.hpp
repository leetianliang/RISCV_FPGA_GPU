#pragma once

#include <cstdint>

namespace golden {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

struct Rgba8888 {
    u32 value = 0u;

    static constexpr Rgba8888 from_u32(u32 v) noexcept { return Rgba8888{v}; }

    constexpr u8 a() const noexcept { return static_cast<u8>((value >> 24) & 0xFFu); }
    constexpr u8 r() const noexcept { return static_cast<u8>((value >> 16) & 0xFFu); }
    constexpr u8 g() const noexcept { return static_cast<u8>((value >> 8) & 0xFFu); }
    constexpr u8 b() const noexcept { return static_cast<u8>(value & 0xFFu); }

    static constexpr Rgba8888 pack(u8 a, u8 r, u8 g, u8 b) noexcept {
        return Rgba8888{
            (static_cast<u32>(a) << 24) | (static_cast<u32>(r) << 16) |
            (static_cast<u32>(g) << 8) | static_cast<u32>(b)};
    }
};

enum class PixelFormat : u32 {
    RGB565 = 0x0,
    ARGB8888 = 0x1,
    XRGB8888 = 0x2,
    INDEX8 = 0x3,
};

enum class BlendMode : u32 {
    COPY = 0x0,
    STRAIGHT_ALPHA = 0x1,
    PREMULT_ALPHA = 0x2,
    ADD_SAT = 0x3,
    MULTIPLY = 0x4,
    XOR = 0x5,
};

enum class FilterMode : u32 {
    NEAREST = 0x0,
    BILINEAR = 0x1,
};

enum class AddressMode : u32 {
    CLAMP = 0x0,
    REPEAT = 0x1,
};

// Numeric values frozen by Register Map V0.1 FAULT_CODE.
enum class FaultCode : u32 {
    NONE = 0x0000,
    BAD_CMD_CLASS = 0x0001,
    BAD_OPCODE = 0x0002,
    BAD_VERSION = 0x0003,
    BAD_LENGTH = 0x0004,
    RESERVED_NONZERO = 0x0005,
    UNSUPPORTED_FEATURE = 0x0006,
    BAD_ALIGNMENT = 0x0007,
    BAD_EXT_PTR = 0x0008,
    BAD_EXT_TYPE = 0x0009,
    BAD_FORMAT = 0x000A,
    BAD_BLEND = 0x000B,
    BAD_FILTER = 0x000C,
    BAD_RECT = 0x000D,
    BAD_RING_CONFIG = 0x000E,
    BAD_TILE_CONFIG = 0x000F,
    TILE_TARGET_MISMATCH = 0x0010,
    WORKLIST_BOUNDS = 0x0011,
    DESCRIPTOR_BOUNDS = 0x0012,
    MEMORY_ERROR = 0x0013,
    DISPLAY_UNDERFLOW = 0x0014,
    INTERNAL_TIMEOUT = 0x0015,
    INTERNAL_RT_COORD = 0x0016,
    CONTEXT_MISMATCH = 0x0017,
    TAG_MISMATCH = 0x0018,
    RING_OVERFLOW = 0x0019,
    BAD_BLEND_STATE = 0x001A,
    BAD_ADDRESS = 0x001B,
};

struct ExecResult {
    bool ok = true;
    FaultCode fault = FaultCode::NONE;
    u32 fault_detail = 0;
    u32 fault_index = 0;  // stream command index when applicable

    static constexpr ExecResult success() noexcept { return ExecResult{}; }

    static constexpr ExecResult failure(FaultCode code, u32 detail = 0,
                                        u32 index = 0) noexcept {
        return ExecResult{false, code, detail, index};
    }
};

}  // namespace golden
