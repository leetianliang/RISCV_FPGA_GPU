#include "golden/golden_gpu.hpp"

#include "golden/gpu_math.hpp"

#include <algorithm>
#include <array>

namespace golden {

namespace {

i32 apply_addr(i32 coord, i32 size, AddressMode mode) {
    if (size <= 0) {
        return 0;
    }
    if (mode == AddressMode::REPEAT) {
        i32 m = coord % size;
        if (m < 0) {
            m += size;
        }
        return m;
    }
    // CLAMP
    if (coord < 0) {
        return 0;
    }
    if (coord >= size) {
        return size - 1;
    }
    return coord;
}

// UV is absolute texture space; address domain is source rectangle [0,src_w).
i32 map_tex_coord(i32 abs_coord, i32 src_origin, i32 src_size, AddressMode mode) {
    const i32 rel = abs_coord - src_origin;
    const i32 mapped = apply_addr(rel, src_size, mode);
    return src_origin + mapped;
}

}  // namespace

ExecResult GoldenGPU::register_resource(const RegisteredResource& res) {
    if (res.size == 0 || res.width == 0 || res.height == 0) {
        return ExecResult::failure(FaultCode::BAD_RECT, res.base);
    }
    if (resources_.find(res.base) != resources_.end()) {
        return ExecResult::failure(FaultCode::BAD_ALIGNMENT, res.base);
    }
    if (!memory_.has_region(res.base)) {
        const auto st = memory_.register_region(res.name, res.base, res.size);
        if (st.status != MemAccessStatus::OK) {
            return ExecResult::failure(FaultCode::MEMORY_ERROR,
                                       static_cast<u32>(st.status));
        }
    }
    resources_.emplace(res.base, res);
    return ExecResult::success();
}

ExecResult GoldenGPU::register_surface(const SurfaceDesc& desc, const std::string& name) {
    const u32 bpp = bytes_per_pixel(desc.format);
    if (bpp == 0 || desc.format == PixelFormat::INDEX8) {
        return ExecResult::failure(FaultCode::BAD_FORMAT,
                                   static_cast<u32>(desc.format));
    }
    if (desc.stride < desc.width * bpp) {
        return ExecResult::failure(FaultCode::BAD_RECT, desc.stride);
    }
    const u64 total = static_cast<u64>(desc.stride) * desc.height;
    if (total > 0xFFFFFFFFull) {
        return ExecResult::failure(FaultCode::BAD_RECT);
    }
    RegisteredResource r;
    r.base = desc.base;
    r.size = static_cast<u32>(total);
    r.width = desc.width;
    r.height = desc.height;
    r.name = name;
    return register_resource(r);
}

std::optional<RegisteredResource> GoldenGPU::resource(u32 base) const {
    const auto it = resources_.find(base);
    if (it == resources_.end()) {
        return std::nullopt;
    }
    return it->second;
}

ExecResult GoldenGPU::execute_command(const GpuCmd64& cmd) {
    // Load extension if present
    std::array<u8, 64> ext{};
    const u8* ext_ptr = nullptr;
    const u32 flags = cmd[0] & 0xFFu;
    if ((flags & kHExtValid) != 0) {
        const u32 addr = cmd[3];
        std::vector<u8> tmp;
        const auto st = memory_.read_block(addr, 64, tmp);
        if (st.status != MemAccessStatus::OK) {
            return ExecResult::failure(FaultCode::MEMORY_ERROR, addr);
        }
        std::copy(tmp.begin(), tmp.end(), ext.begin());
        ext_ptr = ext.data();
    }
    const DecodedDraw d = decode_draw_2d(cmd, ext_ptr, ext_ptr ? 64u : 0u);
    if (!d.ok) {
        return ExecResult::failure(d.fault, d.fault_detail);
    }
    return execute_draw(d);
}

ExecResult GoldenGPU::execute_stream(const std::vector<GpuCmd64>& cmds) {
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        const auto st = execute_command(cmds[i]);
        if (!st.ok) {
            return ExecResult::failure(st.fault, st.fault_detail,
                                       static_cast<u32>(i));
        }
    }
    return ExecResult::success();
}

ExecResult GoldenGPU::sample_and_blend(const DecodedDraw& d, SurfaceView& src_view,
                                       SurfaceView& dst_view, i32 dx, i32 dy, i32 sx,
                                       i32 sy) {
    const u32 ds = d.state.draw_state;
    const u32 blend = extract_blend_mode(ds);
    const u32 filter = extract_filter_mode(ds);
    const auto am_u = static_cast<AddressMode>(extract_addr_mode_u(ds));
    const auto am_v = static_cast<AddressMode>(extract_addr_mode_v(ds));
    const u32 src_w = d.state.src_w;
    const u32 src_h = d.state.src_h;

    // Map destination local coords via UV already folded into sx,sy for 1:1;
    // For scaled path caller passes UV-derived sample coords.
    Rgba8888 src{};
    if (d.state.op == DrawOp::FILL_RECT) {
        src = d.state.primary_color;
    } else {
        const u32 sfmt = extract_src_format(ds);
        if (sfmt == static_cast<u32>(PixelFormat::INDEX8)) {
            // fetch up to 4 indices for bilinear
            auto fetch_index = [&](i32 x, i32 y) -> Rgba8888 {
                const i32 ax = map_tex_coord(x, static_cast<i32>(d.state.src_x),
                                             static_cast<i32>(src_w), am_u);
                const i32 ay = map_tex_coord(y, static_cast<i32>(d.state.src_y),
                                             static_cast<i32>(src_h), am_v);
                u8 idx = 0;
                src_view.read_raw(ax, ay, &idx, 1);
                u32 entry = 0;
                memory_.read32(d.state.palette_addr + idx * 4u, entry);
                return Rgba8888::from_u32(entry);
            };
            if (filter == static_cast<u32>(FilterMode::BILINEAR)) {
                const i32 x0 = floor_q16_16(sx);
                const i32 y0 = floor_q16_16(sy);
                const u16 fx = frac_q16_16(sx);
                const u16 fy = frac_q16_16(sy);
                const Rgba8888 c00 = fetch_index(x0, y0);
                const Rgba8888 c10 = fetch_index(x0 + 1, y0);
                const Rgba8888 c01 = fetch_index(x0, y0 + 1);
                const Rgba8888 c11 = fetch_index(x0 + 1, y0 + 1);
                src = Rgba8888::pack(
                    lerp16(lerp16(c00.a(), c10.a(), fx), lerp16(c01.a(), c11.a(), fx), fy),
                    lerp16(lerp16(c00.r(), c10.r(), fx), lerp16(c01.r(), c11.r(), fx), fy),
                    lerp16(lerp16(c00.g(), c10.g(), fx), lerp16(c01.g(), c11.g(), fx), fy),
                    lerp16(lerp16(c00.b(), c10.b(), fx), lerp16(c01.b(), c11.b(), fx), fy));
            } else {
                src = fetch_index(nearest_index_q16(sx), nearest_index_q16(sy));
            }
        } else if (filter == static_cast<u32>(FilterMode::BILINEAR)) {
            const i32 x0 = floor_q16_16(sx);
            const i32 y0 = floor_q16_16(sy);
            const u16 fx = frac_q16_16(sx);
            const u16 fy = frac_q16_16(sy);
            auto samp = [&](i32 x, i32 y) -> Rgba8888 {
                const i32 ax = map_tex_coord(x, static_cast<i32>(d.state.src_x),
                                             static_cast<i32>(src_w), am_u);
                const i32 ay = map_tex_coord(y, static_cast<i32>(d.state.src_y),
                                             static_cast<i32>(src_h), am_v);
                Rgba8888 c{};
                src_view.read_rgba(ax, ay, c);
                return c;
            };
            const Rgba8888 c00 = samp(x0, y0);
            const Rgba8888 c10 = samp(x0 + 1, y0);
            const Rgba8888 c01 = samp(x0, y0 + 1);
            const Rgba8888 c11 = samp(x0 + 1, y0 + 1);
            src = Rgba8888::pack(
                lerp16(lerp16(c00.a(), c10.a(), fx), lerp16(c01.a(), c11.a(), fx), fy),
                lerp16(lerp16(c00.r(), c10.r(), fx), lerp16(c01.r(), c11.r(), fx), fy),
                lerp16(lerp16(c00.g(), c10.g(), fx), lerp16(c01.g(), c11.g(), fx), fy),
                lerp16(lerp16(c00.b(), c10.b(), fx), lerp16(c01.b(), c11.b(), fx), fy));
        } else {
            const i32 nx = nearest_index_q16(sx);
            const i32 ny = nearest_index_q16(sy);
            const i32 ax = map_tex_coord(nx, static_cast<i32>(d.state.src_x),
                                         static_cast<i32>(src_w), am_u);
            const i32 ay = map_tex_coord(ny, static_cast<i32>(d.state.src_y),
                                         static_cast<i32>(src_h), am_v);
            src_view.read_rgba(ax, ay, src);
        }
    }

    // Color Key
    if (extract_color_key_en(ds) && d.state.op != DrawOp::FILL_RECT) {
        const u32 rgb = (static_cast<u32>(src.r()) << 16) | (static_cast<u32>(src.g()) << 8) |
                        src.b();
        if (rgb == (d.state.color_key_rgb & 0x00FFFFFFu)) {
            return ExecResult::success();
        }
    }

    // Color Mod
    src = color_modulate(src, d.state.primary_color, extract_color_mod_en(ds));

    const bool pixel_en = extract_pixel_alpha_en(ds);
    const bool global_en = extract_global_alpha_en(ds);
    const bool mod_en = extract_color_mod_en(ds);
    const u8 aeff = effective_alpha(src.a(), pixel_en, d.state.primary_color.a(), mod_en,
                                    d.state.global_alpha, global_en);

    if (blend == static_cast<u32>(BlendMode::COPY)) {
        return dst_view.write_rgba(dx, dy, src, extract_dither_en(ds),
                                   static_cast<u32>(dx), static_cast<u32>(dy));
    }

    Rgba8888 dstc{};
    const auto rd = dst_view.read_rgba(dx, dy, dstc);
    if (!rd.ok) {
        return rd;
    }

    Rgba8888 out{};
    if (blend == static_cast<u32>(BlendMode::STRAIGHT_ALPHA)) {
        out = Rgba8888::pack(
            source_over_alpha(aeff, dstc.a()),
            blend_straight_chan(src.r(), dstc.r(), aeff),
            blend_straight_chan(src.g(), dstc.g(), aeff),
            blend_straight_chan(src.b(), dstc.b(), aeff));
    } else if (blend == static_cast<u32>(BlendMode::ADD_SAT)) {
        out = Rgba8888::pack(
            add_sat_chan(dstc.a(), aeff),
            add_sat_chan(dstc.r(), mul8_rn(src.r(), aeff)),
            add_sat_chan(dstc.g(), mul8_rn(src.g(), aeff)),
            add_sat_chan(dstc.b(), mul8_rn(src.b(), aeff)));
    } else if (blend == static_cast<u32>(BlendMode::PREMULT_ALPHA)) {
        // Extra opacity E: 255 -> mod alpha -> global -> coverage
        u8 e0 = 255;
        u8 e1 = mul8_rn(e0, mod_en ? d.state.primary_color.a() : 255u);
        u8 e2 = mul8_rn(e1, global_en ? d.state.global_alpha : 255u);
        u8 e3 = mul8_rn(e2, 255u);
        const u8 a_extra = e3;
        const u8 a_total = mul8_rn(src.a(), a_extra);
        const u8 sc = mul8_rn(src.r(), a_extra);  // wait: S' = mod then contrib
        (void)sc;
        // S'_c = MUL8(S_c, M_c); contrib = MUL8(S'_c, Aextra)
        const Rgba8888 sm = color_modulate(src, d.state.primary_color, true);
        out = Rgba8888::pack(
            source_over_alpha(a_total, dstc.a()),
            add_sat_chan(dstc.r(), mul8_rn(sm.r(), a_extra)),
            add_sat_chan(dstc.g(), mul8_rn(sm.g(), a_extra)),
            add_sat_chan(dstc.b(), mul8_rn(sm.b(), a_extra)));
    } else {
        return ExecResult::failure(FaultCode::UNSUPPORTED_FEATURE, blend);
    }
    return dst_view.write_rgba(dx, dy, out, extract_dither_en(ds),
                               static_cast<u32>(dx), static_cast<u32>(dy));
}

ExecResult GoldenGPU::execute_draw(const DecodedDraw& d) {
    const Draw2DState& st = d.state;

    auto src_res = resource(st.src_base);
    auto dst_res = resource(st.dst_base);
    if (!dst_res) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, st.dst_base);
    }
    if (st.op != DrawOp::FILL_RECT && !src_res) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, st.src_base);
    }

    // Command-authoritative views
    SurfaceView dst_view(&memory_, *dst_res, st.dst_stride,
                         static_cast<PixelFormat>(extract_dst_format(st.draw_state)));
    SurfaceView src_view;
    if (st.op != DrawOp::FILL_RECT) {
        src_view = SurfaceView(&memory_, *src_res, st.src_stride,
                               static_cast<PixelFormat>(extract_src_format(st.draw_state)));
    }

    if (st.dst_w == 0 || st.dst_h == 0) {
        return ExecResult::success();
    }

    // Effective clip: dest rect ∩ clip ∩ target bounds
    i32 rx0 = st.dst_x;
    i32 ry0 = st.dst_y;
    i32 rx1 = st.dst_x + static_cast<i32>(st.dst_w);
    i32 ry1 = st.dst_y + static_cast<i32>(st.dst_h);

    if (extract_clip_en(st.draw_state) || st.has_ext) {
        // Clip from extension if loaded
        rx0 = std::max(rx0, st.clip_xmin);
        ry0 = std::max(ry0, st.clip_ymin);
        rx1 = std::min(rx1, st.clip_xmax);
        ry1 = std::min(ry1, st.clip_ymax);
    }
    rx0 = std::max(rx0, 0);
    ry0 = std::max(ry0, 0);
    rx1 = std::min(rx1, static_cast<i32>(dst_res->width));
    ry1 = std::min(ry1, static_cast<i32>(dst_res->height));
    if (rx0 >= rx1 || ry0 >= ry1) {
        return ExecResult::success();  // empty after clip
    }

    // Validate command stride fits resource for at least first pixel of dest rect
    const u32 bpp_d = bytes_per_pixel(dst_view.format());
    if (st.dst_stride < dst_res->width * bpp_d && st.op == DrawOp::FILL_RECT) {
        // FILL uses dest stride; allow non-tight but must cover width
        if (st.dst_stride < bpp_d) {
            return ExecResult::failure(FaultCode::BAD_RECT, st.dst_stride);
        }
    }

    const i64 du_dx = st.du_dx;
    const i64 dv_dx = st.dv_dx;
    const i64 du_dy = st.du_dy;
    const i64 dv_dy = st.dv_dy;

    // UV origin is for first pixel of full dest rect (before clip)
    for (i32 y = ry0; y < ry1; ++y) {
        const i64 ly = y - st.dst_y;
        for (i32 x = rx0; x < rx1; ++x) {
            const i64 lx = x - st.dst_x;
            i32 sxq;
            i32 syq;
            if (st.op == DrawOp::FILL_RECT) {
                sxq = 0;
                syq = 0;
            } else {
                const i64 u = st.u0 + lx * du_dx + ly * du_dy;
                const i64 v = st.v0 + lx * dv_dx + ly * dv_dy;
                if (u > 2147483647ll || u < -2147483648ll ||
                    v > 2147483647ll || v < -2147483648ll) {
                    return ExecResult::failure(FaultCode::BAD_ADDRESS);
                }
                sxq = static_cast<i32>(u);
                syq = static_cast<i32>(v);
            }
            const auto stp = sample_and_blend(d, src_view, dst_view, x, y, sxq, syq);
            if (!stp.ok) {
                return stp;
            }
        }
    }
    return ExecResult::success();
}

void GoldenGPU::reset() {
    memory_.clear();
    resources_.clear();
}

}  // namespace golden
