#pragma once

#include "golden/gpu_isa.hpp"
#include "golden/gpu_types.hpp"

namespace golden {

enum class DrawOp : u32 { FILL_RECT = 0, BLIT = 1, BLIT_EXT = 2 };

enum class EnumClass : u32 {
    DEFINED_SUPPORTED = 0,
    DEFINED_UNSUPPORTED = 1,
    RESERVED = 2,
};

EnumClass classify_format(u32 f) noexcept;
EnumClass classify_filter(u32 f) noexcept;
EnumClass classify_blend(u32 b) noexcept;
EnumClass classify_addr_mode(u32 m) noexcept;

struct Draw2DState {
    DrawOp op = DrawOp::FILL_RECT;
    u32 src_base = 0;
    u32 dst_base = 0;
    u32 src_stride = 0;
    u32 dst_stride = 0;
    u32 src_x = 0;  // uint16 architectural
    u32 src_y = 0;
    i32 dst_x = 0;
    i32 dst_y = 0;
    u32 src_w = 0;
    u32 src_h = 0;
    u32 dst_w = 0;
    u32 dst_h = 0;
    u32 draw_state = 0;
    Rgba8888 primary_color{};
    u8 global_alpha = 255;
    u32 color_key_rgb = 0;
    u32 palette_addr = 0;
    u32 ext_ptr = 0;
    bool strict = false;

    // Extension (BLIT_EXT / Clip)
    bool has_ext = false;
    i32 clip_xmin = 0;
    i32 clip_ymin = 0;
    i32 clip_xmax = 0x7FFF;
    i32 clip_ymax = 0x7FFF;
    i32 u0 = 0;
    i32 v0 = 0;
    i32 du_dx = 1 << 16;
    i32 dv_dx = 0;
    i32 du_dy = 0;
    i32 dv_dy = 1 << 16;
};

struct DecodedHeader {
    CmdHeader header{};
    bool ok = true;
    FaultCode fault = FaultCode::NONE;
    u32 fault_detail = 0;
};

struct DecodedDraw {
    Draw2DState state{};
    CmdHeader header{};
    bool ok = true;
    FaultCode fault = FaultCode::NONE;
    u32 fault_detail = 0;
};

DecodedHeader decode_cmd_header(const GpuCmd64& cmd) noexcept;
DecodedDraw decode_draw_2d(const GpuCmd64& cmd, const u8* ext_bytes = nullptr,
                           u32 ext_size = 0) noexcept;

GpuCmd64 make_fill_rect_cmd(u32 dst_base, u32 dst_stride, i32 dst_x, i32 dst_y,
                            u32 dst_w, u32 dst_h, Rgba8888 color) noexcept;

struct BlitCmdDesc {
    u32 src_base = 0;
    u32 dst_base = 0;
    u32 src_stride = 0;
    u32 dst_stride = 0;
    u32 src_x = 0;
    u32 src_y = 0;
    i32 dst_x = 0;
    i32 dst_y = 0;
    u32 w = 0;
    u32 h = 0;
    u32 src_format = static_cast<u32>(PixelFormat::RGB565);
    u32 dst_format = static_cast<u32>(PixelFormat::RGB565);
    u32 blend = static_cast<u32>(BlendMode::COPY);
    u32 filter = static_cast<u32>(FilterMode::NEAREST);
    u32 addr_u = static_cast<u32>(AddressMode::CLAMP);
    u32 addr_v = static_cast<u32>(AddressMode::CLAMP);
    bool color_key_en = false;
    u32 color_key_rgb = 0;
    bool global_alpha_en = false;
    u8 global_alpha = 255;
    bool pixel_alpha_en = false;
    bool flip_x = false;
    bool flip_y = false;
    bool palette_en = false;
    bool premult = false;
    bool clip_en = false;
    bool color_mod_en = false;
    bool dither_en = false;
    u32 palette_addr = 0;
    u32 ext_ptr = 0;
    u32 hdr_flags = 0;
    // BLIT_EXT UV (Q16.16); if use_ext_uv, write extension separately
    bool blit_ext = false;
    i32 u0 = 0;
    i32 v0 = 0;
    i32 du_dx = 1 << 16;
    i32 dv_dx = 0;
    i32 du_dy = 0;
    i32 dv_dy = 1 << 16;
    i32 clip_xmin = 0;
    i32 clip_ymin = 0;
    i32 clip_xmax = 0x7FFF;
    i32 clip_ymax = 0x7FFF;
    Rgba8888 primary_color{};
};

GpuCmd64 make_blit_cmd(const BlitCmdDesc& d) noexcept;
GpuCmd64 make_blit_ext_cmd(const BlitCmdDesc& d) noexcept;
std::array<u8, 64> make_draw2d_ext_v1(const BlitCmdDesc& d) noexcept;

}  // namespace golden
