#pragma once

#include "golden/gpu_isa.hpp"
#include "golden/gpu_types.hpp"

namespace golden {

enum class DrawOp : u32 {
    FILL_RECT = 0,
    BLIT = 1,
};

struct Draw2DState {
    DrawOp op = DrawOp::FILL_RECT;
    u32 src_base = 0;
    u32 dst_base = 0;
    u32 src_stride = 0;
    u32 dst_stride = 0;
    i32 src_x = 0;
    i32 src_y = 0;
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
    bool strict = false;
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

// FILL + BLIT decode for Stage-002.
DecodedDraw decode_draw_2d(const GpuCmd64& cmd) noexcept;

GpuCmd64 make_fill_rect_cmd(u32 dst_base, u32 dst_stride, i32 dst_x, i32 dst_y,
                            u32 dst_w, u32 dst_h, Rgba8888 color) noexcept;

// Build BLIT command with optional feature bits.
struct BlitCmdDesc {
    u32 src_base = 0;
    u32 dst_base = 0;
    u32 src_stride = 0;
    u32 dst_stride = 0;
    i32 src_x = 0;
    i32 src_y = 0;
    i32 dst_x = 0;
    i32 dst_y = 0;
    u32 w = 0;
    u32 h = 0;
    u32 src_format = static_cast<u32>(PixelFormat::RGB565);
    u32 dst_format = static_cast<u32>(PixelFormat::RGB565);
    u32 blend = static_cast<u32>(BlendMode::COPY);
    bool color_key_en = false;
    u32 color_key_rgb = 0;
    bool global_alpha_en = false;
    u8 global_alpha = 255;
    bool pixel_alpha_en = false;
    u32 hdr_flags = 0;
};

GpuCmd64 make_blit_cmd(const BlitCmdDesc& d) noexcept;

}  // namespace golden
