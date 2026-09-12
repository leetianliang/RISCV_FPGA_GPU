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

void fail_draw(DecodedDraw& d, FaultCode f, u32 detail = 0) {
    d.ok = false;
    d.fault = f;
    d.fault_detail = detail;
}
}  // namespace

EnumClass classify_format(u32 f) noexcept {
    if (f <= static_cast<u32>(PixelFormat::INDEX8)) {
        return EnumClass::DEFINED_SUPPORTED;
    }
    return EnumClass::RESERVED;
}

EnumClass classify_filter(u32 f) noexcept {
    if (f <= static_cast<u32>(FilterMode::BILINEAR)) {
        return EnumClass::DEFINED_SUPPORTED;
    }
    return EnumClass::RESERVED;
}

EnumClass classify_blend(u32 b) noexcept {
    if (b <= static_cast<u32>(BlendMode::XOR)) {
        return EnumClass::DEFINED_SUPPORTED;
    }
    return EnumClass::RESERVED;
}

EnumClass classify_addr_mode(u32 m) noexcept {
    if (m <= static_cast<u32>(AddressMode::REPEAT)) {
        return EnumClass::DEFINED_SUPPORTED;
    }
    return EnumClass::RESERVED;
}

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
    if (out.header.length_dw != kCmdLengthDw) {
        return make_header_fault(FaultCode::BAD_LENGTH, out.header.length_dw);
    }
    if (strict && (out.header.hdr_flags & kHHdrReservedMask) != 0) {
        return make_header_fault(FaultCode::RESERVED_NONZERO, out.header.hdr_flags);
    }
    if (strict && (out.header.hdr_flags & kHExtValid) == 0 && out.header.ext_ptr != 0) {
        return make_header_fault(FaultCode::BAD_EXT_PTR, out.header.ext_ptr);
    }
    if ((out.header.hdr_flags & kHExtValid) != 0) {
        if (out.header.ext_ptr == 0 || (out.header.ext_ptr & 0x3Fu) != 0) {
            return make_header_fault(FaultCode::BAD_EXT_PTR, out.header.ext_ptr);
        }
    }
    if (out.header.cmd_class != kClassDraw2D) {
        return make_header_fault(FaultCode::BAD_CMD_CLASS, out.header.cmd_class);
    }
    switch (out.header.opcode) {
        case kOpcodeFillRect:
        case kOpcodeBlit:
        case kOpcodeBlitExt:
            break;
        case kOpcodeTileFrame:
            return make_header_fault(FaultCode::UNSUPPORTED_FEATURE, out.header.opcode);
        default:
            return make_header_fault(FaultCode::BAD_OPCODE, out.header.opcode);
    }
    out.ok = true;
    return out;
}

namespace {

bool decode_common(const GpuCmd64& cmd, const DecodedHeader& hdr, DrawOp op,
                   DecodedDraw& out) {
    out.header = hdr.header;
    if (!hdr.ok) {
        fail_draw(out, hdr.fault, hdr.fault_detail);
        return false;
    }
    out.state.op = op;
    out.state.strict = (hdr.header.hdr_flags & kHStrict) != 0;
    out.state.src_base = cmd[4];
    out.state.dst_base = cmd[5];
    out.state.src_stride = cmd[6];
    out.state.dst_stride = cmd[7];
    out.state.src_x = unpack_u16_lo(cmd[8]);
    out.state.src_y = unpack_u16_hi(cmd[8]);
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
    out.state.palette_addr = cmd[15];
    out.state.ext_ptr = hdr.header.ext_ptr;
    out.state.has_ext = (hdr.header.hdr_flags & kHExtValid) != 0;

    const u32 ds = out.state.draw_state;
    if (out.state.strict && extract_draw_state_reserved(ds) != 0) {
        fail_draw(out, FaultCode::RESERVED_NONZERO, ds);
        return false;
    }

    const u32 src_fmt = extract_src_format(ds);
    const u32 dst_fmt = extract_dst_format(ds);
    const u32 blend = extract_blend_mode(ds);
    const u32 filter = extract_filter_mode(ds);
    const u32 au = extract_addr_mode_u(ds);
    const u32 av = extract_addr_mode_v(ds);

    if (classify_format(src_fmt) == EnumClass::RESERVED) {
        fail_draw(out, FaultCode::BAD_FORMAT, src_fmt);
        return false;
    }
    if (classify_format(dst_fmt) == EnumClass::RESERVED) {
        fail_draw(out, FaultCode::BAD_FORMAT, dst_fmt);
        return false;
    }
    if (classify_blend(blend) == EnumClass::RESERVED) {
        fail_draw(out, FaultCode::BAD_BLEND, blend);
        return false;
    }
    if (classify_filter(filter) == EnumClass::RESERVED) {
        fail_draw(out, FaultCode::BAD_FILTER, filter);
        return false;
    }
    if (classify_addr_mode(au) == EnumClass::RESERVED ||
        classify_addr_mode(av) == EnumClass::RESERVED) {
        fail_draw(out, FaultCode::BAD_FILTER, ds);
        return false;
    }

    // Stage support checks (defined-but-unsupported → UNSUPPORTED_FEATURE)
    if (dst_fmt == static_cast<u32>(PixelFormat::INDEX8)) {
        fail_draw(out, FaultCode::UNSUPPORTED_FEATURE, dst_fmt);
        return false;
    }
    if (blend == static_cast<u32>(BlendMode::MULTIPLY) ||
        blend == static_cast<u32>(BlendMode::XOR)) {
        fail_draw(out, FaultCode::UNSUPPORTED_FEATURE, blend);
        return false;
    }
    if (out.state.dst_x < 0 || out.state.dst_y < 0) {
        // Clip may allow later; for non-clip reject. Clip handled after ext load.
        if (!extract_clip_en(ds)) {
            fail_draw(out, FaultCode::BAD_RECT, cmd[9]);
            return false;
        }
    }
    return true;
}

bool load_extension(const Draw2DState& st, const u8* ext_bytes, u32 ext_size,
                    DecodedDraw& out) {
    if (!st.has_ext) {
        return true;
    }
    if (ext_bytes == nullptr || ext_size < 64) {
        fail_draw(out, FaultCode::MEMORY_ERROR, st.ext_ptr);
        return false;
    }
    auto rdw = [&](int i) -> u32 {
        const u8* p = ext_bytes + i * 4;
        return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) |
               (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24);
    };
    const u32 w0 = rdw(0);
    const u32 ext_type = w0 & 0xFFu;
    const u32 ext_ver = (w0 >> 8) & 0xFu;
    const u32 ext_len = (w0 >> 12) & 0xFFu;
    const u32 ext_flags = w0 >> 20;
    if (ext_type != 0x01 || ext_ver != 0x1 || ext_len != 16 || ext_flags != 0) {
        fail_draw(out, FaultCode::BAD_EXT_TYPE, w0);
        return false;
    }
    out.state.clip_xmin = unpack_s16_lo(rdw(1));
    out.state.clip_ymin = unpack_s16_hi(rdw(1));
    out.state.clip_xmax = unpack_s16_lo(rdw(2));
    out.state.clip_ymax = unpack_s16_hi(rdw(2));
    out.state.u0 = static_cast<i32>(rdw(3));
    out.state.v0 = static_cast<i32>(rdw(4));
    out.state.du_dx = static_cast<i32>(rdw(5));
    out.state.dv_dx = static_cast<i32>(rdw(6));
    out.state.du_dy = static_cast<i32>(rdw(7));
    out.state.dv_dy = static_cast<i32>(rdw(8));
    for (int i = 12; i < 16; ++i) {
        if (rdw(i) != 0 && out.state.strict) {
            fail_draw(out, FaultCode::RESERVED_NONZERO, static_cast<u32>(i));
            return false;
        }
    }
    return true;
}

}  // namespace

DecodedDraw decode_draw_2d(const GpuCmd64& cmd, const u8* ext_bytes, u32 ext_size) noexcept {
    DecodedDraw out;
    DecodedHeader hdr = decode_cmd_header(cmd);
    if (!hdr.ok) {
        fail_draw(out, hdr.fault, hdr.fault_detail);
        out.header = hdr.header;
        return out;
    }
    const DrawOp op = static_cast<DrawOp>(hdr.header.opcode);
    if (!decode_common(cmd, hdr, op, out)) {
        return out;
    }

    if (op == DrawOp::FILL_RECT) {
        out.state.src_base = 0;
        out.state.src_stride = 0;
        out.state.src_x = 0;
        out.state.src_y = 0;
        out.state.src_w = out.state.dst_w;
        out.state.src_h = out.state.dst_h;
        // FILL may use Draw2D extension for Clip when H_EXT_VALID=1.
        if (out.state.has_ext) {
            if (!load_extension(out.state, ext_bytes, ext_size, out)) {
                return out;
            }
        }
        return out;
    }

    // BLIT / BLIT_EXT
    if (!load_extension(out.state, ext_bytes, ext_size, out)) {
        return out;
    }

    if (op == DrawOp::BLIT) {
        if (out.state.has_ext) {
            // BLIT may use clip-only ext; scale not via BLIT
            // Allow clip-only; reject non-identity UV if present is complex — BLIT uses 1:1
        }
        if (out.state.src_w != out.state.dst_w || out.state.src_h != out.state.dst_h) {
            fail_draw(out, FaultCode::UNSUPPORTED_FEATURE, cmd[11]);
            return out;
        }
        if (extract_filter_mode(out.state.draw_state) !=
            static_cast<u32>(FilterMode::NEAREST)) {
            fail_draw(out, FaultCode::UNSUPPORTED_FEATURE, out.state.draw_state);
            return out;
        }
        // Default 1:1 UV for BLIT
        out.state.u0 = static_cast<i32>(out.state.src_x) << 16;
        out.state.v0 = static_cast<i32>(out.state.src_y) << 16;
        out.state.du_dx = 1 << 16;
        out.state.dv_dx = 0;
        out.state.du_dy = 0;
        out.state.dv_dy = 1 << 16;
        if (extract_flip_x(out.state.draw_state)) {
            // Last integer texel index: SRC_X + SRC_W - 1
            out.state.u0 =
                static_cast<i32>(out.state.src_x + out.state.src_w - 1u) << 16;
            out.state.du_dx = -65536;
        }
        if (extract_flip_y(out.state.draw_state)) {
            out.state.v0 =
                static_cast<i32>(out.state.src_y + out.state.src_h - 1u) << 16;
            out.state.dv_dy = -65536;
        }
    } else {
        // BLIT_EXT
        if (!out.state.has_ext) {
            fail_draw(out, FaultCode::BAD_EXT_PTR, 0);
            return out;
        }
        if (!load_extension(out.state, ext_bytes, ext_size, out)) {
            return out;
        }
        if (out.state.dv_dx != 0 || out.state.du_dy != 0) {
            fail_draw(out, FaultCode::UNSUPPORTED_FEATURE, out.state.draw_state);
            return out;
        }
        // Scale path: SRC_W/H describe source rectangle size for address modes
    }

    if (extract_palette_en(out.state.draw_state)) {
        if (extract_src_format(out.state.draw_state) !=
            static_cast<u32>(PixelFormat::INDEX8)) {
            fail_draw(out, FaultCode::UNSUPPORTED_FEATURE, out.state.draw_state);
            return out;
        }
        if (out.state.palette_addr == 0) {
            fail_draw(out, FaultCode::BAD_ADDRESS, 0);
            return out;
        }
    }
    if (extract_src_format(out.state.draw_state) ==
        static_cast<u32>(PixelFormat::INDEX8)) {
        if (!extract_palette_en(out.state.draw_state)) {
            fail_draw(out, FaultCode::UNSUPPORTED_FEATURE, out.state.draw_state);
            return out;
        }
    }

    // premult strict requirements
    if (extract_premult_src(out.state.draw_state)) {
        if (extract_blend_mode(out.state.draw_state) !=
            static_cast<u32>(BlendMode::PREMULT_ALPHA)) {
            fail_draw(out, FaultCode::UNSUPPORTED_FEATURE, out.state.draw_state);
            return out;
        }
        if (!extract_pixel_alpha_en(out.state.draw_state) && out.state.strict) {
            fail_draw(out, FaultCode::BAD_BLEND_STATE, out.state.draw_state);
            return out;
        }
    }
    if (extract_blend_mode(out.state.draw_state) ==
        static_cast<u32>(BlendMode::PREMULT_ALPHA)) {
        if (!extract_premult_src(out.state.draw_state) && out.state.strict) {
            fail_draw(out, FaultCode::BAD_BLEND_STATE, out.state.draw_state);
            return out;
        }
    }
    if (extract_dither_en(out.state.draw_state) &&
        extract_dst_format(out.state.draw_state) !=
            static_cast<u32>(PixelFormat::RGB565)) {
        if (out.state.strict) {
            fail_draw(out, FaultCode::BAD_FORMAT, out.state.draw_state);
            return out;
        }
        // non-strict: ignore dither — clear bit for execution
        out.state.draw_state &= ~(1u << 27);
    }

    out.ok = true;
    out.fault = FaultCode::NONE;
    return out;
}

GpuCmd64 make_fill_rect_cmd(u32 dst_base, u32 dst_stride, i32 dst_x, i32 dst_y,
                            u32 dst_w, u32 dst_h, Rgba8888 color) noexcept {
    GpuCmd64 cmd{};
    cmd[0] = header_word(kClassDraw2D, kOpcodeFillRect, kCmdEncodingVersion, kCmdLengthDw, 0);
    cmd[5] = dst_base;
    cmd[7] = dst_stride;
    cmd[9] = pack_xy_i16(dst_x, dst_y);
    cmd[11] = pack_wh(dst_w, dst_h);
    cmd[12] = pack_draw_state(static_cast<u32>(PixelFormat::RGB565),
                              static_cast<u32>(PixelFormat::RGB565),
                              static_cast<u32>(BlendMode::COPY));
    cmd[13] = color.value;
    cmd[14] = 0;
    return cmd;
}

namespace {
u32 pack_xy_src(u32 x, u32 y) noexcept {
    return ((y & 0xFFFFu) << 16) | (x & 0xFFFFu);
}
}  // namespace

GpuCmd64 make_blit_cmd(const BlitCmdDesc& d) noexcept {
    GpuCmd64 cmd{};
    const u32 op = d.blit_ext ? kOpcodeBlitExt : kOpcodeBlit;
    u32 flags = d.hdr_flags;
    if (d.blit_ext || d.clip_en) {
        flags |= kHExtValid;
    }
    cmd[0] = header_word(kClassDraw2D, op, kCmdEncodingVersion, kCmdLengthDw, flags);
    cmd[3] = d.ext_ptr;
    cmd[4] = d.src_base;
    cmd[5] = d.dst_base;
    cmd[6] = d.src_stride;
    cmd[7] = d.dst_stride;
    cmd[8] = pack_xy_src(d.src_x, d.src_y);
    cmd[9] = pack_xy_i16(d.dst_x, d.dst_y);
    cmd[10] = pack_wh(d.w, d.h);
    cmd[11] = pack_wh(d.w, d.h);
    u32 ds = pack_draw_state(d.src_format, d.dst_format, d.blend, d.filter);
    ds |= (d.addr_u & 3u) << 14;
    ds |= (d.addr_v & 3u) << 16;
    if (d.color_key_en) ds |= 1u << 18;
    if (d.global_alpha_en) ds |= 1u << 19;
    if (d.pixel_alpha_en) ds |= 1u << 20;
    if (d.flip_x) ds |= 1u << 21;
    if (d.flip_y) ds |= 1u << 22;
    if (d.palette_en) ds |= 1u << 23;
    if (d.premult) ds |= 1u << 24;
    if (d.clip_en) ds |= 1u << 25;
    if (d.color_mod_en) ds |= 1u << 26;
    if (d.dither_en) ds |= 1u << 27;
    cmd[12] = ds;
    cmd[13] = d.primary_color.value;
    cmd[14] = (static_cast<u32>(d.global_alpha) << 24) | (d.color_key_rgb & 0x00FFFFFFu);
    cmd[15] = d.palette_addr;
    return cmd;
}

GpuCmd64 make_blit_ext_cmd(const BlitCmdDesc& d) noexcept {
    BlitCmdDesc c = d;
    c.blit_ext = true;
    // Do not force Clip; preserve caller intent.
    c.ext_ptr = d.ext_ptr ? d.ext_ptr : 0x30000;
    return make_blit_cmd(c);
}

std::array<u8, 64> make_draw2d_ext_v1(const BlitCmdDesc& d) noexcept {
    std::array<u8, 64> out{};
    auto putw = [&](int i, u32 w) {
        out[i * 4 + 0] = static_cast<u8>(w & 0xFF);
        out[i * 4 + 1] = static_cast<u8>((w >> 8) & 0xFF);
        out[i * 4 + 2] = static_cast<u8>((w >> 16) & 0xFF);
        out[i * 4 + 3] = static_cast<u8>((w >> 24) & 0xFF);
    };
    putw(0, 0x01u | (0x1u << 8) | (16u << 12));  // type, ver, len
    putw(1, pack_xy_i16(d.clip_xmin, d.clip_ymin));
    putw(2, pack_xy_i16(d.clip_xmax, d.clip_ymax));
    putw(3, static_cast<u32>(d.u0));
    putw(4, static_cast<u32>(d.v0));
    putw(5, static_cast<u32>(d.du_dx));
    putw(6, static_cast<u32>(d.dv_dx));
    putw(7, static_cast<u32>(d.du_dy));
    putw(8, static_cast<u32>(d.dv_dy));
    return out;
}

}  // namespace golden
