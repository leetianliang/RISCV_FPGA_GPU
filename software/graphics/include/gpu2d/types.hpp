#pragma once

#include <cstdint>

namespace gpu2d {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

struct Color {
    u8 a = 255, r = 0, g = 0, b = 0;
    static constexpr Color rgba(u8 r, u8 g, u8 b, u8 a = 255) noexcept {
        return Color{a, r, g, b};
    }
    static constexpr Color rgb(u8 r, u8 g, u8 b) noexcept { return rgba(r, g, b, 255); }
};

enum class PixelFormat : u32 {
    RGB565 = 0,
    ARGB8888 = 1,
    XRGB8888 = 2,
    INDEX8 = 3,
};

enum class BlendMode : u32 {
    Copy = 0,
    StraightAlpha = 1,
    PremultAlpha = 2,
    AddSat = 3,
    Multiply = 4,
    Xor = 5,
};

enum class FilterMode : u32 {
    Nearest = 0,
    Bilinear = 1,
};

enum class AddressMode : u32 {
    Clamp = 0,
    Repeat = 1,
};

enum class BackendKind : u32 {
    Immediate = 0,
    Tile32 = 1,
    Tile16 = 2,
    Tile64 = 3,
};

struct TextureId {
    u32 v = 0;
    bool valid() const noexcept { return v != 0; }
};

struct ClipRect {
    i32 xmin = 0;
    i32 ymin = 0;
    i32 xmax = 0x7FFF;
    i32 ymax = 0x7FFF;
    bool enable = false;
};

struct SpriteParams {
    TextureId tex{};
    i32 dst_x = 0;
    i32 dst_y = 0;
    u32 w = 0;
    u32 h = 0;
    u32 src_x = 0;
    u32 src_y = 0;
    u32 src_w = 0;  // 0 → w
    u32 src_h = 0;  // 0 → h
    i32 scale_w = 0;  // 0 → native 1:1
    i32 scale_h = 0;
    BlendMode blend = BlendMode::Copy;
    u8 global_alpha = 255;
    bool color_key = false;
    u32 color_key_rgb = 0;
    bool color_mod = false;
    Color mod = Color::rgb(255, 255, 255);
    FilterMode filter = FilterMode::Nearest;
    AddressMode addr = AddressMode::Clamp;
    bool flip_x = false;
    bool flip_y = false;
    bool premult = false;
    bool dither = false;
    bool palette = false;
};

struct ProfileDesc {
    u32 width = 640;
    u32 height = 360;
    PixelFormat format = PixelFormat::RGB565;
    u32 tile_size = 32;
};

// Backend-neutral read-only telemetry.
struct RendererTelemetry {
    u32 command_count = 0;
    u32 sprite_count = 0;
    u32 workref_count = 0;
    u32 tiles_total = 0;
    u32 tiles_active = 0;
    u32 max_workrefs_per_tile = 0;
    u32 max_overdraw = 0;
    u32 grid_w = 0;
    u32 grid_h = 0;
    u32 tile_size = 0;
    bool has_overdraw = false;
    bool has_workref_map = false;
    const u32* overdraw = nullptr;     // row-major surface
    const u16* tile_workrefs = nullptr;  // grid_w * grid_h
};

}  // namespace gpu2d
