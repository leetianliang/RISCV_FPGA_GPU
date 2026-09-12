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

// Canonical logical color is 0xAARRGGBB (Pixel Arithmetic V0.1).
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

// Numeric values frozen by Command ISA V0.1.
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

enum class FaultCode : u32 {
    NONE = 0,
    BAD_CMD_CLASS = 1,
    BAD_OPCODE = 2,
    BAD_VERSION = 3,
    BAD_LENGTH = 4,
    RESERVED_NONZERO = 5,
    UNSUPPORTED_FEATURE = 6,
    BAD_ALIGNMENT = 7,
    BAD_EXT_PTR = 8,
    BAD_EXT_TYPE = 9,
    BAD_FORMAT = 10,
    BAD_BLEND = 11,
    BAD_FILTER = 12,
    BAD_RECT = 13,
    BAD_TILE_CONFIG = 14,
    TILE_TARGET_MISMATCH = 15,
    WORKLIST_BOUNDS = 16,
    DESCRIPTOR_BOUNDS = 17,
    MEMORY = 18,
};

struct ExecResult {
    bool ok = true;
    FaultCode fault = FaultCode::NONE;
    u32 fault_detail = 0;

    static constexpr ExecResult success() noexcept { return ExecResult{}; }

    static constexpr ExecResult failure(FaultCode code, u32 detail = 0) noexcept {
        return ExecResult{false, code, detail};
    }
};

}  // namespace golden
