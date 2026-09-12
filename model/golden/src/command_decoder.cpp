#include "golden/command_decoder.hpp"

namespace golden {

namespace {

DecodedHeader make_header_fault(FaultCode fault, u32 detail = 0) {
    DecodedHeader out;
    out.ok = false;
    out.fault = fault;
    out.fault_detail = detail;
    return out;
}

}  // namespace

DecodedHeader decode_cmd_header(const GpuCmd64& cmd) noexcept {
    DecodedHeader out;
    const u32 w0 = cmd[0];
    out.header.cmd_class = (w0 >> 28) & 0xFu;
    out.header.opcode = (w0 >> 20) & 0xFFu;
    out.header.version = (w0 >> 16) & 0xFu;
    out.header.length_dw = (w0 >> 8) & 0xFFu;
    out.header.hdr_flags = w0 & 0xFFu;
    out.header.sequence_id = cmd[1];
    out.header.user_tag = cmd[2];
    out.header.ext_ptr = cmd[3];

    if (out.header.version != kCmdEncodingVersion) {
        return make_header_fault(FaultCode::BAD_VERSION, out.header.version);
    }
    if (out.header.length_dw != kCmdLengthDw) {
        return make_header_fault(FaultCode::BAD_LENGTH, out.header.length_dw);
    }
    if ((out.header.hdr_flags & kHHdrReservedMask) != 0) {
        return make_header_fault(FaultCode::RESERVED_NONZERO, out.header.hdr_flags);
    }
    if ((out.header.hdr_flags & kHExtValid) == 0 && out.header.ext_ptr != 0) {
        return make_header_fault(FaultCode::BAD_EXT_PTR, out.header.ext_ptr);
    }
    if ((out.header.hdr_flags & kHExtValid) != 0 && out.header.ext_ptr == 0) {
        return make_header_fault(FaultCode::BAD_EXT_PTR, 0);
    }
    if (out.header.cmd_class != kClassDraw2D) {
        return make_header_fault(FaultCode::BAD_CMD_CLASS, out.header.cmd_class);
    }
    if (out.header.opcode != kOpcodeFillRect) {
        // DRAW_2D known class, opcode not implemented in Stage-001.
        return make_header_fault(FaultCode::BAD_OPCODE, out.header.opcode);
    }

    out.ok = true;
    out.fault = FaultCode::NONE;
    return out;
}

DecodedFill decode_fill_rect(const GpuCmd64& cmd) noexcept {
    DecodedFill out;
    const DecodedHeader hdr = decode_cmd_header(cmd);
    out.header = hdr.header;
    if (!hdr.ok) {
        out.ok = false;
        out.fault = hdr.fault;
        out.fault_detail = hdr.fault_detail;
        return out;
    }

    // Extension request not supported by Stage-001 capability.
    if ((hdr.header.hdr_flags & kHExtValid) != 0) {
        out.ok = false;
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = hdr.header.ext_ptr;
        return out;
    }

    const u32 w5 = cmd[5];   // DST_BASE
    const u32 w7 = cmd[7];   // DST_STRIDE
    const u32 w9 = cmd[9];   // DST_XY
    const u32 w11 = cmd[11]; // DST_WH
    const u32 w12 = cmd[12]; // DRAW_STATE
    const u32 w13 = cmd[13]; // PRIMARY_COLOR

    out.payload.dst_base = w5;
    out.payload.dst_stride = w7;
    out.payload.dst_x = unpack_s16_lo(w9);
    out.payload.dst_y = unpack_s16_hi(w9);
    out.payload.dst_w = unpack_u16_lo(w11);
    out.payload.dst_h = unpack_u16_hi(w11);
    out.payload.draw_state = w12;
    out.payload.primary_color = Rgba8888::from_u32(w13);

    // Stage-001 execution profile.
    if (extract_dst_format(w12) != static_cast<u32>(PixelFormat::RGB565)) {
        out.ok = false;
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = extract_dst_format(w12);
        return out;
    }
    if (extract_blend_mode(w12) != static_cast<u32>(BlendMode::COPY)) {
        out.ok = false;
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = extract_blend_mode(w12);
        return out;
    }
    if (extract_dither_en(w12)) {
        out.ok = false;
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = w12;
        return out;
    }
    if (extract_clip_en(w12)) {
        out.ok = false;
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = w12;
        return out;
    }
    if (extract_draw_state_reserved(w12) != 0) {
        out.ok = false;
        out.fault = FaultCode::RESERVED_NONZERO;
        out.fault_detail = w12;
        return out;
    }

    // Stage-001 directed-test limitation (not a full ISA redefinition):
    // destination x/y must be non-negative.
    if (out.payload.dst_x < 0 || out.payload.dst_y < 0) {
        out.ok = false;
        out.fault = FaultCode::BAD_RECT;
        out.fault_detail = w9;
        return out;
    }

    out.ok = true;
    out.fault = FaultCode::NONE;
    return out;
}

GpuCmd64 make_fill_rect_cmd(u32 dst_base, u32 dst_stride, i32 dst_x, i32 dst_y,
                            u32 dst_w, u32 dst_h, Rgba8888 color) noexcept {
    GpuCmd64 cmd{};
    cmd.fill(0u);
    cmd[0] = header_word(kClassDraw2D, kOpcodeFillRect, kCmdEncodingVersion,
                         kCmdLengthDw, 0u);
    cmd[1] = 0u;  // SEQUENCE_ID
    cmd[2] = 0u;  // USER_TAG
    cmd[3] = 0u;  // EXT_PTR
    // W4 SRC_BASE ignored by FILL, write 0
    cmd[4] = 0u;
    cmd[5] = dst_base;
    // W6 SRC_STRIDE ignored by FILL
    cmd[6] = 0u;
    cmd[7] = dst_stride;
    // W8 SRC_XY ignored by FILL
    cmd[8] = 0u;
    cmd[9] = pack_xy_i16(dst_x, dst_y);
    // W10 SRC_WH ignored by FILL
    cmd[10] = 0u;
    cmd[11] = pack_wh(dst_w, dst_h);
    cmd[12] = pack_draw_state(static_cast<u32>(PixelFormat::RGB565),
                              static_cast<u32>(BlendMode::COPY));
    cmd[13] = color.value;
    cmd[14] = 0u;  // ALPHA_KEY ignored by FILL
    cmd[15] = 0u;  // PALETTE_ADDR ignored by FILL
    return cmd;
}

}  // namespace golden
