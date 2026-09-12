#include "golden/golden_gpu.hpp"

#include "golden/gpu_math.hpp"

namespace golden {

namespace {

bool validate_surface_desc(const SurfaceDesc& desc) {
    if (desc.width == 0 || desc.height == 0 || desc.stride == 0) {
        return false;
    }
    if (desc.format == PixelFormat::INDEX8) {
        return false;
    }
    const u32 bpp = bytes_per_pixel(desc.format);
    if (bpp == 0) {
        return false;
    }
    const u64 min_stride = static_cast<u64>(desc.width) * bpp;
    if (desc.stride < min_stride) {
        return false;
    }
    const u64 total = static_cast<u64>(desc.stride) * desc.height;
    if (total == 0 || total > 0xFFFFFFFFull) {
        return false;
    }
    return true;
}

}  // namespace

ExecResult GoldenGPU::register_surface(const SurfaceDesc& desc, const std::string& name) {
    if (!validate_surface_desc(desc)) {
        return ExecResult::failure(FaultCode::BAD_RECT, desc.base);
    }
    if (desc.format != PixelFormat::RGB565 && desc.format != PixelFormat::ARGB8888 &&
        desc.format != PixelFormat::XRGB8888) {
        return ExecResult::failure(FaultCode::BAD_FORMAT,
                                   static_cast<u32>(desc.format));
    }
    if (surfaces_.find(desc.base) != surfaces_.end()) {
        return ExecResult::failure(FaultCode::BAD_ALIGNMENT, desc.base);
    }
    if (!memory_.has_region(desc.base)) {
        const auto st = memory_.register_region(
            name, desc.base, static_cast<u32>(static_cast<u64>(desc.stride) * desc.height));
        if (st.status != MemAccessStatus::OK) {
            return ExecResult::failure(FaultCode::MEMORY_ERROR,
                                       static_cast<u32>(st.status));
        }
    }
    surfaces_.emplace(desc.base, desc);
    return ExecResult::success();
}

std::optional<SurfaceDesc> GoldenGPU::surface_desc(u32 base) const {
    const auto it = surfaces_.find(base);
    if (it == surfaces_.end()) {
        return std::nullopt;
    }
    return it->second;
}

ExecResult GoldenGPU::write_dst_pixel(const Surface& dst, u32 x, u32 y, Rgba8888 src,
                                      const Draw2DState& st) {
    const u32 blend = extract_blend_mode(st.draw_state);
    const bool key_en = extract_color_key_en(st.draw_state);
    const bool global_en = extract_global_alpha_en(st.draw_state);
    const bool pixel_en = extract_pixel_alpha_en(st.draw_state);

    // Color Key on canonical decoded RGB (ignore source alpha).
    if (key_en) {
        const u32 rgb = (static_cast<u32>(src.r()) << 16) |
                        (static_cast<u32>(src.g()) << 8) | static_cast<u32>(src.b());
        if (rgb == (st.color_key_rgb & 0x00FFFFFFu)) {
            return ExecResult::success();  // discard
        }
    }

    const u8 aeff =
        effective_alpha(src.a(), pixel_en, st.global_alpha, global_en);

    if (blend == static_cast<u32>(BlendMode::COPY)) {
        // Alpha flags must not convert COPY into blending.
        return dst.write_pixel(x, y, src);
    }

    Rgba8888 dst_color{};
    const auto rd = dst.read_pixel(x, y, dst_color);
    if (!rd.ok) {
        return rd;
    }

    Rgba8888 out{};
    if (blend == static_cast<u32>(BlendMode::STRAIGHT_ALPHA)) {
        out = Rgba8888::pack(
            source_over_alpha(aeff, dst_color.a()),
            blend_straight_chan(src.r(), dst_color.r(), aeff),
            blend_straight_chan(src.g(), dst_color.g(), aeff),
            blend_straight_chan(src.b(), dst_color.b(), aeff));
    } else if (blend == static_cast<u32>(BlendMode::ADD_SAT)) {
        out = Rgba8888::pack(
            add_sat_chan(dst_color.a(), aeff),
            add_sat_chan(dst_color.r(), mul8_rn(src.r(), aeff)),
            add_sat_chan(dst_color.g(), mul8_rn(src.g(), aeff)),
            add_sat_chan(dst_color.b(), mul8_rn(src.b(), aeff)));
    } else {
        return ExecResult::failure(FaultCode::UNSUPPORTED_FEATURE, blend);
    }
    return dst.write_pixel(x, y, out);
}

ExecResult GoldenGPU::execute_command(const GpuCmd64& cmd) {
    const DecodedDraw decoded = decode_draw_2d(cmd);
    if (!decoded.ok) {
        return ExecResult::failure(decoded.fault, decoded.fault_detail);
    }
    return execute_draw(decoded);
}

ExecResult GoldenGPU::execute_stream(const std::vector<GpuCmd64>& cmds) {
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        const ExecResult st = execute_command(cmds[i]);
        if (!st.ok) {
            return ExecResult::failure(st.fault, st.fault_detail,
                                       static_cast<u32>(i));
        }
    }
    return ExecResult::success();
}

ExecResult GoldenGPU::execute_draw(const DecodedDraw& decoded) {
    const Draw2DState& st = decoded.state;

    // Source surface for BLIT
    Surface src_surface;
    if (st.op == DrawOp::BLIT) {
        const auto src_it = surfaces_.find(st.src_base);
        if (src_it == surfaces_.end()) {
            return ExecResult::failure(FaultCode::MEMORY_ERROR, st.src_base);
        }
        src_surface = Surface(&memory_, src_it->second);
    }

    const auto dst_it = surfaces_.find(st.dst_base);
    if (dst_it == surfaces_.end()) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, st.dst_base);
    }
    const Surface dst(&memory_, dst_it->second);
    const SurfaceDesc& dst_desc = dst_it->second;

    if (st.dst_w == 0 || st.dst_h == 0) {
        return ExecResult::success();
    }

    const i64 x0 = st.dst_x;
    const i64 y0 = st.dst_y;
    const i64 x1 = x0 + static_cast<i64>(st.dst_w);
    const i64 y1 = y0 + static_cast<i64>(st.dst_h);
    if (x0 < 0 || y0 < 0 || x1 > static_cast<i64>(dst_desc.width) ||
        y1 > static_cast<i64>(dst_desc.height)) {
        return ExecResult::failure(FaultCode::BAD_RECT, st.draw_state);
    }

    for (i64 ly = 0; ly < static_cast<i64>(st.dst_h); ++ly) {
        for (i64 lx = 0; lx < static_cast<i64>(st.dst_w); ++lx) {
            const u32 dx = static_cast<u32>(x0 + lx);
            const u32 dy = static_cast<u32>(y0 + ly);
            Rgba8888 src{};
            if (st.op == DrawOp::FILL_RECT) {
                src = st.primary_color;
            } else {
                const u32 sx = static_cast<u32>(st.src_x + lx);
                const u32 sy = static_cast<u32>(st.src_y + ly);
                const auto rds = src_surface.read_pixel(sx, sy, src);
                if (!rds.ok) {
                    return rds;
                }
            }
            const auto wr = write_dst_pixel(dst, dx, dy, src, st);
            if (!wr.ok) {
                return wr;
            }
        }
    }
    return ExecResult::success();
}

void GoldenGPU::reset() {
    memory_.clear();
    surfaces_.clear();
}

}  // namespace golden
