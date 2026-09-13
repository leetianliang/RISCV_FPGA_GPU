#pragma once

#include "golden/gpu_isa.hpp"
#include "golden/gpu_types.hpp"

#include <array>
#include <vector>

namespace golden {

inline constexpr u32 kTileHeaderBytes = 16;
inline constexpr u32 kWorkRefBytes = 4;
inline constexpr u32 kDefaultTileSize = 32;

struct TileHeader {
    u32 work_offset = 0;  // in WorkRef entries
    u32 work_count = 0;
    u32 flags = 0;
};

inline constexpr u32 kTileDontLoadColor = 1u << 0;
inline constexpr u32 kTileClearColor = 1u << 1;

struct TileFrameState {
    u32 dst_format = 0;
    bool load_color_default = false;
    bool store_color = true;
    bool strict_target_match = false;
};

struct TileFrameCmd {
    u32 draw_desc_base = 0;
    u32 tile_header_base = 0;
    u32 work_list_base = 0;
    u32 dst_base = 0;
    u32 dst_stride = 0;
    u32 surface_w = 0;
    u32 surface_h = 0;
    u32 grid_w = 0;
    u32 grid_h = 0;
    u32 tile_w = 0;
    u32 tile_h = 0;
    u32 rt_state = 0;
    u32 clear_color = 0;
    u32 desc_count = 0;  // not on wire; set by harness for bounds
};

inline GpuCmd64 make_tile_frame_cmd(const TileFrameCmd& t) noexcept {
    GpuCmd64 cmd{};
    cmd[0] = header_word(kClassDraw2D, kOpcodeTileFrame, kCmdEncodingVersion,
                         kCmdLengthDw, 0);
    cmd[4] = t.draw_desc_base;
    cmd[5] = t.tile_header_base;
    cmd[6] = t.work_list_base;
    cmd[7] = t.dst_base;
    cmd[8] = t.dst_stride;
    cmd[9] = pack_wh(t.surface_w, t.surface_h);
    cmd[10] = pack_wh(t.grid_w, t.grid_h);
    cmd[11] = pack_wh(t.tile_w, t.tile_h);
    cmd[12] = t.rt_state;
    cmd[13] = t.clear_color;
    return cmd;
}

inline TileFrameCmd decode_tile_frame_cmd(const GpuCmd64& cmd) noexcept {
    TileFrameCmd t;
    t.draw_desc_base = cmd[4];
    t.tile_header_base = cmd[5];
    t.work_list_base = cmd[6];
    t.dst_base = cmd[7];
    t.dst_stride = cmd[8];
    t.surface_w = unpack_u16_lo(cmd[9]);
    t.surface_h = unpack_u16_hi(cmd[9]);
    t.grid_w = unpack_u16_lo(cmd[10]);
    t.grid_h = unpack_u16_hi(cmd[10]);
    t.tile_w = unpack_u16_lo(cmd[11]);
    t.tile_h = unpack_u16_hi(cmd[11]);
    t.rt_state = cmd[12];
    t.clear_color = cmd[13];
    return t;
}

inline bool parse_tile_header(const u8* bytes, TileHeader& out) noexcept {
    if (bytes == nullptr) {
        return false;
    }
    auto rd = [&](int i) -> u32 {
        return static_cast<u32>(bytes[i * 4 + 0]) |
               (static_cast<u32>(bytes[i * 4 + 1]) << 8) |
               (static_cast<u32>(bytes[i * 4 + 2]) << 16) |
               (static_cast<u32>(bytes[i * 4 + 3]) << 24);
    };
    out.work_offset = rd(0);
    out.work_count = rd(1);
    out.flags = rd(2);
    return true;
}

inline std::array<u8, kTileHeaderBytes> serialize_tile_header(const TileHeader& h) noexcept {
    std::array<u8, kTileHeaderBytes> out{};
    auto put = [&](int i, u32 w) {
        out[i * 4 + 0] = static_cast<u8>(w & 0xFF);
        out[i * 4 + 1] = static_cast<u8>((w >> 8) & 0xFF);
        out[i * 4 + 2] = static_cast<u8>((w >> 16) & 0xFF);
        out[i * 4 + 3] = static_cast<u8>((w >> 24) & 0xFF);
    };
    put(0, h.work_offset);
    put(1, h.work_count);
    put(2, h.flags);
    return out;
}

// WorkRef is a 32-bit descriptor index.
using WorkRef = u32;

struct TileGridInfo {
    u32 tile_size = kDefaultTileSize;
    u32 tiles_x = 0;
    u32 tiles_y = 0;
};

inline TileGridInfo make_grid(u32 surface_w, u32 surface_h, u32 tile_size) {
    TileGridInfo g;
    g.tile_size = tile_size ? tile_size : kDefaultTileSize;
    g.tiles_x = (surface_w + g.tile_size - 1) / g.tile_size;
    g.tiles_y = (surface_h + g.tile_size - 1) / g.tile_size;
    return g;
}

inline u32 tile_id(u32 tx, u32 ty, u32 tiles_x) { return ty * tiles_x + tx; }

inline void tile_valid_rect(u32 tx, u32 ty, u32 tile_size, u32 surface_w,
                            u32 surface_h, u32& x0, u32& y0, u32& w, u32& h) {
    x0 = tx * tile_size;
    y0 = ty * tile_size;
    const u32 x1 = x0 + tile_size < surface_w ? x0 + tile_size : surface_w;
    const u32 y1 = y0 + tile_size < surface_h ? y0 + tile_size : surface_h;
    w = x1 > x0 ? x1 - x0 : 0;
    h = y1 > y0 ? y1 - y0 : 0;
}

struct TileBinResult {
    std::vector<GpuCmd64> descriptors;  // one per submitted draw, in order
    std::vector<TileHeader> headers;    // grid_w * grid_h, row-major
    std::vector<WorkRef> workrefs;
};

}  // namespace golden
