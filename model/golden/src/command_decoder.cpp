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
    const bool strict = (out.header.hdr_flags & kHStrict) != 0;

    if (out.header.version != kCmdEncodingVersion) {
        return make_header_fault(FaultCode::BAD_VERSION, out.header.version);
    }
    // Fixed 64B Stage-002 path always rejects other lengths.
    if (out.header.length_dw != kCmdLengthDw) {
        return make_header_fault(FaultCode::BAD_LENGTH, out.header.length_dw);
    }
    if (strict && (out.header.hdr_flags & kHHdrReservedMask) != 0) {
        return make_header_fault(FaultCode::RESERVED_NONZERO, out.header.hdr_flags);
    }
    if (strict && (out.header.hdr_flags & kHExtValid) == 0 && out.header.ext_ptr != 0) {
        return make_header_fault(FaultCode::BAD_EXT_PTR, out.header.ext_ptr);
    }
    if ((out.header.hdr_flags & kHExtValid) != 0 && out.header.ext_ptr == 0) {
        return make_header_fault(FaultCode::BAD_EXT_PTR, 0);
    }
    if ((out.header.hdr_flags & kHExtValid) != 0 &&
        (out.header.ext_ptr & 0x3Fu) != 0) {
        return make_header_fault(FaultCode::BAD_EXT_PTR, out.header.ext_ptr);
    }
    if (out.header.cmd_class != kClassDraw2D) {
        return make_header_fault(FaultCode::BAD_CMD_CLASS, out.header.cmd_class);
    }

    switch (out.header.opcode) {
        case kOpcodeFillRect:
        case kOpcodeBlit:
            break;
        case kOpcodeBlitExt:
        case kOpcodeTileFrame:
            return make_header_fault(FaultCode::UNSUPPORTED_FEATURE, out.header.opcode);
        default:
            return make_header_fault(FaultCode::BAD_OPCODE, out.header.opcode);
    }

    out.ok = true;
    out.fault = FaultCode::NONE;
    return out;
}

namespace {

DecodedDraw decode_common_payload(const GpuCmd64& cmd, DecodedHeader hdr,
                                  DrawOp op) {
    DecodedDraw out;
    out.header = hdr.header;
    out.ok = false;
    out.fault = hdr.fault;
    out.fault_detail = hdr.fault_detail;
    if (!hdr.ok) {
        return out;
    }

    out.state.op = op;
    out.state.strict = (hdr.header.hdr_flags & kHStrict) != 0;
    out.state.src_base = cmd[4];
    out.state.dst_base = cmd[5];
    out.state.src_stride = cmd[6];
    out.state.dst_stride = cmd[7];
    out.state.src_x = unpack_s16_lo(cmd[8]);
    out.state.src_y = unpack_s16_hi(cmd[8]);
    out.state.dst_x = unpack_s16_lo(cmd[9]);
    out.state.dst_y = unpack_s16_hi(cmd[9]);
    out.state.src_w = unpack_u16_lo(cmd[10]);
    out.state.src_h = unpack_u16_hi(cmd[10]);
    out.state.dst_w = unpack_u16_lo(cmd[11]);
    out.state.dst_h = unpack_u16_hi(cmd[11]);
    out.state.draw_state = cmd[12];
    out.state.primary_color = Rgba8888::from_u32(cmd[13]);
    out.state.global_alpha = static_cast<u8>((cmd[14] >> 24) & 0xFFu);
    out.state.color_key_rgb = cmd[14] & 0x00FFFFFFu;

    const u32 ds = out.state.draw_state;
    const u32 dst_fmt = extract_dst_format(ds);
    const u32 blend = extract_blend_mode(ds);

    if (out.state.strict && extract_draw_state_reserved(ds) != 0) {
        out.fault = FaultCode::RESERVED_NONZERO;
        out.fault_detail = ds;
        return out;
    }

    if (dst_fmt != static_cast<u32>(PixelFormat::RGB565)) {
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = dst_fmt;
        return out;
    }

    // Deferred / unsupported flags must not silently degrade.
    if (extract_dither_en(ds) || extract_clip_en(ds) || extract_color_mod_en(ds) ||
        extract_palette_en(ds) || extract_flip_x(ds) || extract_flip_y(ds) ||
        extract_premult_src(ds) || extract_depth_test_en(ds)) {
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = ds;
        return out;
    }

    const bool blend_ok =
        blend == static_cast<u32>(BlendMode::COPY) ||
        blend == static_cast<u32>(BlendMode::STRAIGHT_ALPHA) ||
        blend == static_cast<u32>(BlendMode::ADD_SAT);
    if (!blend_ok) {
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = blend;
        return out;
    }

    if (out.state.dst_x < 0 || out.state.dst_y < 0) {
        out.fault = FaultCode::BAD_RECT;
        out.fault_detail = cmd[9];
        return out;
    }

    out.ok = true;
    out.fault = FaultCode::NONE;
    return out;
}

}  // namespace

DecodedDraw decode_draw_2d(const GpuCmd64& cmd) noexcept {
    DecodedHeader hdr = decode_cmd_header(cmd);
    if (!hdr.ok) {
        DecodedDraw out;
        out.header = hdr.header;
        out.ok = false;
        out.fault = hdr.fault;
        out.fault_detail = hdr.fault_detail;
        return out;
    }
    if (hdr.header.opcode == kOpcodeFillRect) {
        DecodedDraw out = decode_common_payload(cmd, hdr, DrawOp::FILL_RECT);
        if (!out.ok) {
            return out;
        }
        // FILL: SRC fields ignored; Stage-002 non-negative DST only.
        out.state.src_base = 0;
        out.state.src_stride = 0;
        out.state.src_x = 0;
        out.state.src_y = 0;
        out.state.src_w = out.state.dst_w;
        out.state.src_h = out.state.dst_h;
        return out;
    }
    // BLIT
    DecodedDraw out = decode_common_payload(cmd, hdr, DrawOp::BLIT);
    if (!out.ok) {
        return out;
    }
    if ((hdr.header.hdr_flags & kHExtValid) != 0) {
        out.ok = false;
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = hdr.header.ext_ptr;
        return out;
    }
    if (out.state.src_w != out.state.dst_w || out.state.src_h != out.state.dst_h) {
        out.ok = false;
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = cmd[11];
        return out;
    }
    if (extract_filter_mode(out.state.draw_state) !=
        static_cast<u32>(FilterMode::NEAREST)) {
        out.ok = false;
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = out.state.draw_state;
        return out;
    }
    if (out.state.src_x < 0 || out.state.src_y < 0) {
        out.ok = false;
        out.fault = FaultCode::BAD_RECT;
        out.fault_detail = cmd[8];
        return out;
    }
    const u32 src_fmt = extract_src_format(out.state.draw_state);
    if (src_fmt == static_cast<u32>(PixelFormat::INDEX8) ||
        src_fmt > static_cast<u32>(PixelFormat::INDEX8)) {
        out.ok = false;
        out.fault = FaultCode::UNSUPPORTED_FEATURE;
        out.fault_detail = src_fmt;
        return out;
    }
    return out;
}

GpuCmd64 make_fill_rect_cmd(u32 dst_base, u32 dst_stride, i32 dst_x, i32 dst_y,
                            u32 dst_w, u32 dst_h, Rgba8888 color) noexcept {
    GpuCmd64 cmd{};
    cmd[0] = header_word(kClassDraw2D, kOpcodeFillRect, kCmdEncodingVersion,
                         kCmdLengthDw, 0u);
    cmd[5] = dst_base;
    cmd[7] = dst_stride;
    cmd[9] = pack_xy_i16(dst_x, dst_y);
    cmd[11] = pack_wh(dst_w, dst_h);
    cmd[12] = pack_draw_state(static_cast<u32>(PixelFormat::RGB565),
                              static_cast<u32>(PixelFormat::RGB565),
                              static_cast<u32>(BlendMode::COPY));
    cmd[13] = color.value;
    cmd[14] = 0u;  // matches Stage-001 fill_basic fixture (G_ALPHA unused for COPY)
    return cmd;
}

GpuCmd64 make_blit_cmd(const BlitCmdDesc& d) noexcept {
    GpuCmd64 cmd{};
    cmd[0] = header_word(kClassDraw2D, kOpcodeBlit, kCmdEncodingVersion, kCmdLengthDw,
                         d.hdr_flags);
    cmd[4] = d.src_base;
    cmd[5] = d.dst_base;
    cmd[6] = d.src_stride;
    cmd[7] = d.dst_stride;
    cmd[8] = pack_xy_i16(d.src_x, d.src_y);
    cmd[9] = pack_xy_i16(d.dst_x, d.dst_y);
    cmd[10] = pack_wh(d.w, d.h);
    cmd[11] = pack_wh(d.w, d.h);
    u32 ds = pack_draw_state(d.src_format, d.dst_format, d.blend,
                             static_cast<u32>(FilterMode::NEAREST));
    if (d.color_key_en) {
        ds |= 1u << 18;
    }
    if (d.global_alpha_en) {
        ds |= 1u << 19;
    }
    if (d.pixel_alpha_en) {
        ds |= 1u << 20;
    }
    cmd[12] = ds;
    cmd[13] = 0xFFFFFFFFu;
    cmd[14] = (static_cast<u32>(d.global_alpha) << 24) | (d.color_key_rgb & 0x00FFFFFFu);
    return cmd;
}

}  // namespace golden
