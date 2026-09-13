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
    if (coord < 0) {
        return 0;
    }
    if (coord >= size) {
        return size - 1;
    }
    return coord;
}

// UV absolute texture space; address domain is source rectangle.
i32 map_tex_coord(i32 abs_coord, i32 src_origin, i32 src_size, AddressMode mode) {
    const i32 rel = abs_coord - src_origin;
    return src_origin + apply_addr(rel, src_size, mode);
}

// Linear surface rule: stride covers registered resource width; view must fit allocation.
ExecResult validate_view(const SurfaceView& view, u32 /*logical_w_hint*/) {
    if (!view.valid()) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR);
    }
    const u32 bpp = bytes_per_pixel(view.format());
    if (bpp == 0) {
        return ExecResult::failure(FaultCode::BAD_FORMAT,
                                   static_cast<u32>(view.format()));
    }
    const u32 width = view.resource().width;
    const u32 height = view.resource().height;
    if (width == 0 || height == 0) {
        return ExecResult::failure(FaultCode::BAD_RECT);
    }
    const u64 row_bytes = static_cast<u64>(width) * bpp;
    if (static_cast<u64>(view.stride()) < row_bytes) {
        return ExecResult::failure(FaultCode::BAD_RECT, view.stride());
    }
    // Last byte of last row: base + (h-1)*stride + width*bpp
    const u64 last = static_cast<u64>(view.resource().base) +
                     static_cast<u64>(height - 1) * static_cast<u64>(view.stride()) +
                     row_bytes;
    if (last > (1ull << 32)) {
        return ExecResult::failure(FaultCode::BAD_ADDRESS);
    }
    if (last > static_cast<u64>(view.resource().base) + view.resource().size) {
        return ExecResult::failure(FaultCode::BAD_RECT, view.stride());
    }
    return ExecResult::success();
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
    const DecodedHeader hdr = decode_cmd_header(cmd);
    if (!hdr.ok) {
        return ExecResult::failure(hdr.fault, hdr.fault_detail);
    }
    if (hdr.header.opcode == kOpcodeTileFrame) {
        // Handled by tile renderer helper (shares this GoldenGPU).
        return ExecResult::failure(FaultCode::UNSUPPORTED_FEATURE,
                                   kOpcodeTileFrame);
    }

    std::array<u8, 64> ext{};
    const u8* ext_ptr = nullptr;
    if ((hdr.header.hdr_flags & kHExtValid) != 0) {
        const u32 addr = hdr.header.ext_ptr;
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

GoldenGPU::Sampled GoldenGPU::sample_color(const DecodedDraw& d, SurfaceView& src_view,
                                           i32 sx, i32 sy) {
    Sampled out;
    const u32 ds = d.state.draw_state;
    const u32 filter = extract_filter_mode(ds);
    const auto am_u = static_cast<AddressMode>(extract_addr_mode_u(ds));
    const auto am_v = static_cast<AddressMode>(extract_addr_mode_v(ds));
    const u32 src_w = d.state.src_w;
    const u32 src_h = d.state.src_h;
    const u32 sfmt = extract_src_format(ds);

    auto fetch_rgba = [&](i32 ax, i32 ay) -> Sampled {
        Sampled s;
        if (perf) {
            ++perf->texture_samples;
        }
        const auto st = src_view.read_rgba(ax, ay, s.color);
        s.status = st;
        return s;
    };

    auto fetch_index = [&](i32 ax, i32 ay) -> Sampled {
        Sampled s;
        if (perf) {
            ++perf->texture_samples;
            ++perf->palette_reads;
        }
        u8 idx = 0;
        const auto st = src_view.read_raw(ax, ay, &idx, 1);
        if (!st.ok) {
            s.status = st;
            return s;
        }
        const u64 paddr = static_cast<u64>(d.state.palette_addr) +
                          static_cast<u64>(idx) * 4ull;
        if (paddr > 0xFFFFFFFFull) {
            s.status = ExecResult::failure(FaultCode::BAD_ADDRESS);
            return s;
        }
        u32 entry = 0;
        const auto pst = memory_.read32(static_cast<u32>(paddr), entry);
        if (pst.status != MemAccessStatus::OK) {
            s.status = ExecResult::failure(FaultCode::MEMORY_ERROR,
                                           static_cast<u32>(paddr));
            return s;
        }
        s.color = Rgba8888::from_u32(entry);
        return s;
    };

    auto map_and = [&](i32 x, i32 y) -> std::pair<i32, i32> {
        return {map_tex_coord(x, static_cast<i32>(d.state.src_x),
                              static_cast<i32>(src_w), am_u),
                map_tex_coord(y, static_cast<i32>(d.state.src_y),
                              static_cast<i32>(src_h), am_v)};
    };

    if (sfmt == static_cast<u32>(PixelFormat::INDEX8)) {
        if (filter == static_cast<u32>(FilterMode::BILINEAR)) {
            if (perf) {
                ++perf->bilinear_samples;
            }
            const i32 x0 = floor_q16_16(sx);
            const i32 y0 = floor_q16_16(sy);
            const u16 fx = frac_q16_16(sx);
            const u16 fy = frac_q16_16(sy);
            Sampled c00, c10, c01, c11;
            {
                auto p = map_and(x0, y0);
                c00 = fetch_index(p.first, p.second);
            }
            if (!c00.status.ok) {
                return c00;
            }
            {
                auto p = map_and(x0 + 1, y0);
                c10 = fetch_index(p.first, p.second);
            }
            if (!c10.status.ok) {
                return c10;
            }
            {
                auto p = map_and(x0, y0 + 1);
                c01 = fetch_index(p.first, p.second);
            }
            if (!c01.status.ok) {
                return c01;
            }
            {
                auto p = map_and(x0 + 1, y0 + 1);
                c11 = fetch_index(p.first, p.second);
            }
            if (!c11.status.ok) {
                return c11;
            }
            out.color = Rgba8888::pack(
                lerp16(lerp16(c00.color.a(), c10.color.a(), fx),
                       lerp16(c01.color.a(), c11.color.a(), fx), fy),
                lerp16(lerp16(c00.color.r(), c10.color.r(), fx),
                       lerp16(c01.color.r(), c11.color.r(), fx), fy),
                lerp16(lerp16(c00.color.g(), c10.color.g(), fx),
                       lerp16(c01.color.g(), c11.color.g(), fx), fy),
                lerp16(lerp16(c00.color.b(), c10.color.b(), fx),
                       lerp16(c01.color.b(), c11.color.b(), fx), fy));
            return out;
        }
        auto p = map_and(nearest_index_q16(sx), nearest_index_q16(sy));
        return fetch_index(p.first, p.second);
    }

    if (filter == static_cast<u32>(FilterMode::BILINEAR)) {
        if (perf) {
            ++perf->bilinear_samples;
        }
        const i32 x0 = floor_q16_16(sx);
        const i32 y0 = floor_q16_16(sy);
        const u16 fx = frac_q16_16(sx);
        const u16 fy = frac_q16_16(sy);
        Sampled c00, c10, c01, c11;
        {
            auto p = map_and(x0, y0);
            c00 = fetch_rgba(p.first, p.second);
        }
        if (!c00.status.ok) {
            return c00;
        }
        {
            auto p = map_and(x0 + 1, y0);
            c10 = fetch_rgba(p.first, p.second);
        }
        if (!c10.status.ok) {
            return c10;
        }
        {
            auto p = map_and(x0, y0 + 1);
            c01 = fetch_rgba(p.first, p.second);
        }
        if (!c01.status.ok) {
            return c01;
        }
        {
            auto p = map_and(x0 + 1, y0 + 1);
            c11 = fetch_rgba(p.first, p.second);
        }
        if (!c11.status.ok) {
            return c11;
        }
        out.color = Rgba8888::pack(
            lerp16(lerp16(c00.color.a(), c10.color.a(), fx),
                   lerp16(c01.color.a(), c11.color.a(), fx), fy),
            lerp16(lerp16(c00.color.r(), c10.color.r(), fx),
                   lerp16(c01.color.r(), c11.color.r(), fx), fy),
            lerp16(lerp16(c00.color.g(), c10.color.g(), fx),
                   lerp16(c01.color.g(), c11.color.g(), fx), fy),
            lerp16(lerp16(c00.color.b(), c10.color.b(), fx),
                   lerp16(c01.color.b(), c11.color.b(), fx), fy));
        return out;
    }

    auto p = map_and(nearest_index_q16(sx), nearest_index_q16(sy));
    return fetch_rgba(p.first, p.second);
}

ExecResult GoldenGPU::sample_and_blend(const DecodedDraw& d, SurfaceView& src_view,
                                       SurfaceView& dst_view, i32 dx, i32 dy, i32 sx,
                                       i32 sy) {
    const u32 ds = d.state.draw_state;
    const u32 blend = extract_blend_mode(ds);
    const bool key_en = extract_color_key_en(ds);
    const bool mod_en = extract_color_mod_en(ds);
    const bool pixel_en = extract_pixel_alpha_en(ds);
    const bool global_en = extract_global_alpha_en(ds);
    const bool dither = extract_dither_en(ds);
    const Rgba8888 mod_color = d.state.primary_color;

    Rgba8888 sampled{};
    if (perf) {
        ++perf->pixels_attempted;
    }
    if (d.state.op == DrawOp::FILL_RECT) {
        sampled = d.state.primary_color;
    } else {
        const Sampled s = sample_color(d, src_view, sx, sy);
        if (!s.status.ok) {
            return s.status;
        }
        sampled = s.color;
    }

    // Color Key on decoded RGB before Color Mod / Alpha.
    if (key_en && d.state.op != DrawOp::FILL_RECT) {
        const u32 rgb = (static_cast<u32>(sampled.r()) << 16) |
                        (static_cast<u32>(sampled.g()) << 8) | sampled.b();
        if (rgb == (d.state.color_key_rgb & 0x00FFFFFFu)) {
            if (perf) {
                ++perf->key_discards;
            }
            return ExecResult::success();
        }
    }

    // Color Mod RGB exactly once → S'
    const Rgba8888 sm = color_modulate(sampled, mod_color, mod_en);

    const u8 aeff =
        effective_alpha(sampled.a(), pixel_en, mod_color.a(), mod_en,
                        d.state.global_alpha, global_en);

    if (blend == static_cast<u32>(BlendMode::COPY)) {
        const auto wr = dst_view.write_rgba(dx, dy, sm, dither,
                                       static_cast<u32>(dx + d.state.dither_ox),
                                       static_cast<u32>(dy + d.state.dither_oy));
        if (wr.ok && perf) {
            ++perf->pixels_written;
            perf->note_write(static_cast<u32>(dx), static_cast<u32>(dy));
        }
        return wr;
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
            blend_straight_chan(sm.r(), dstc.r(), aeff),
            blend_straight_chan(sm.g(), dstc.g(), aeff),
            blend_straight_chan(sm.b(), dstc.b(), aeff));
    } else if (blend == static_cast<u32>(BlendMode::ADD_SAT)) {
        out = Rgba8888::pack(
            add_sat_chan(dstc.a(), aeff),
            add_sat_chan(dstc.r(), mul8_rn(sm.r(), aeff)),
            add_sat_chan(dstc.g(), mul8_rn(sm.g(), aeff)),
            add_sat_chan(dstc.b(), mul8_rn(sm.b(), aeff)));
    } else if (blend == static_cast<u32>(BlendMode::PREMULT_ALPHA)) {
        // Aextra: 255 → mod α → global α → coverage(255)
        const u8 e1 = mul8_rn(255u, mod_en ? mod_color.a() : 255u);
        const u8 e2 = mul8_rn(e1, global_en ? d.state.global_alpha : 255u);
        const u8 aextra = mul8_rn(e2, 255u);
        const u8 a_total = mul8_rn(sampled.a(), aextra);
        // Scontrib = MUL8(S', Aextra); S' already color-modulated once
        const u8 cr = mul8_rn(sm.r(), aextra);
        const u8 cg = mul8_rn(sm.g(), aextra);
        const u8 cb = mul8_rn(sm.b(), aextra);
        const u8 dmul = static_cast<u8>(255u - a_total);
        out = Rgba8888::pack(
            source_over_alpha(a_total, dstc.a()),
            add_sat_chan(cr, mul8_rn(dstc.r(), dmul)),
            add_sat_chan(cg, mul8_rn(dstc.g(), dmul)),
            add_sat_chan(cb, mul8_rn(dstc.b(), dmul)));
    } else {
        return ExecResult::failure(FaultCode::UNSUPPORTED_FEATURE, blend);
    }
    if (perf) {
        ++perf->blend_ops;
    }
    const auto wr2 = dst_view.write_rgba(dx, dy, out, dither,
                               static_cast<u32>(dx + d.state.dither_ox),
                               static_cast<u32>(dy + d.state.dither_oy));
    if (wr2.ok && perf) {
        ++perf->pixels_written;
        perf->note_write(static_cast<u32>(dx), static_cast<u32>(dy));
    }
    return wr2;
}

ExecResult GoldenGPU::execute_decoded(const DecodedDraw& d, bool extra_clip, i32 x0,
                                      i32 y0, i32 x1, i32 y1) {
    return execute_draw_clipped(d, extra_clip, x0, y0, x1, y1, nullptr, nullptr, 0,
                                PixelFormat::RGB565);
}

ExecResult GoldenGPU::execute_decoded_on(const DecodedDraw& d, MemoryImage* dst_mem,
                                         const RegisteredResource& dst_res,
                                         u32 dst_stride, PixelFormat dst_fmt,
                                         bool extra_clip, i32 x0, i32 y0, i32 x1,
                                         i32 y1) {
    return execute_draw_clipped(d, extra_clip, x0, y0, x1, y1, dst_mem, &dst_res,
                                dst_stride, dst_fmt);
}

ExecResult GoldenGPU::execute_draw(const DecodedDraw& d) {
    return execute_draw_clipped(d, false, 0, 0, 0, 0, nullptr, nullptr, 0,
                                PixelFormat::RGB565);
}

ExecResult GoldenGPU::execute_draw_clipped(const DecodedDraw& d, bool extra_clip,
                                           i32 ex0, i32 ey0, i32 ex1, i32 ey1,
                                           MemoryImage* dst_mem,
                                           const RegisteredResource* dst_res_opt,
                                           u32 dst_stride_opt, PixelFormat dst_fmt_opt) {
    const Draw2DState& st = d.state;

    auto src_res = resource(st.src_base);
    RegisteredResource dst_res_local{};
    const RegisteredResource* dst_res = nullptr;
    MemoryImage* dst_memory = &memory_;
    u32 dst_stride = st.dst_stride;
    PixelFormat dst_fmt = static_cast<PixelFormat>(extract_dst_format(st.draw_state));

    if (dst_mem != nullptr && dst_res_opt != nullptr) {
        // Tile internal RT path
        dst_memory = dst_mem;
        dst_res = dst_res_opt;
        dst_stride = dst_stride_opt;
        dst_fmt = dst_fmt_opt;
    } else {
        auto lookup = resource(st.dst_base);
        if (!lookup) {
            return ExecResult::failure(FaultCode::MEMORY_ERROR, st.dst_base);
        }
        dst_res_local = *lookup;
        dst_res = &dst_res_local;
        dst_memory = &memory_;
        dst_stride = st.dst_stride;
        dst_fmt = static_cast<PixelFormat>(extract_dst_format(st.draw_state));
    }
    if (st.op != DrawOp::FILL_RECT && !src_res) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, st.src_base);
    }

    SurfaceView dst_view(dst_memory, *dst_res, dst_stride, dst_fmt);
    SurfaceView src_view;
    if (st.op != DrawOp::FILL_RECT) {
        src_view = SurfaceView(&memory_, *src_res, st.src_stride,
                               static_cast<PixelFormat>(extract_src_format(st.draw_state)));
        const auto vst = validate_view(src_view, st.src_w ? st.src_w : src_res->width);
        if (!vst.ok) {
            return vst;
        }
    }
    {
        const auto vst = validate_view(dst_view, dst_res->width);
        if (!vst.ok) {
            return vst;
        }
    }

    if (st.dst_w == 0 || st.dst_h == 0) {
        return ExecResult::success();
    }

    i32 rx0 = st.dst_x;
    i32 ry0 = st.dst_y;
    i32 rx1 = st.dst_x + static_cast<i32>(st.dst_w);
    i32 ry1 = st.dst_y + static_cast<i32>(st.dst_h);

    // Clip applies only when CLIP_EN=1 (not merely because extension exists).
    if (extract_clip_en(st.draw_state)) {
        rx0 = std::max(rx0, st.clip_xmin);
        ry0 = std::max(ry0, st.clip_ymin);
        rx1 = std::min(rx1, st.clip_xmax);
        ry1 = std::min(ry1, st.clip_ymax);
    }
    // Target bounds: always intersect with registered resource (no-clip OOB rule).
    rx0 = std::max(rx0, 0);
    ry0 = std::max(ry0, 0);
    rx1 = std::min(rx1, static_cast<i32>(dst_res->width));
    ry1 = std::min(ry1, static_cast<i32>(dst_res->height));
    if (extra_clip) {
        rx0 = std::max(rx0, ex0);
        ry0 = std::max(ry0, ey0);
        rx1 = std::min(rx1, ex1);
        ry1 = std::min(ry1, ey1);
    }
    if (rx0 >= rx1 || ry0 >= ry1) {
        return ExecResult::success();
    }

    const i64 du_dx = st.du_dx;
    const i64 dv_dx = st.dv_dx;
    const i64 du_dy = st.du_dy;
    const i64 dv_dy = st.dv_dy;

    for (i32 y = ry0; y < ry1; ++y) {
        const i64 ly = y - st.dst_y;
        for (i32 x = rx0; x < rx1; ++x) {
            const i64 lx = x - st.dst_x;
            i32 sxq = 0;
            i32 syq = 0;
            if (st.op != DrawOp::FILL_RECT) {
                const i64 u = st.u0 + lx * du_dx + ly * du_dy;
                const i64 v = st.v0 + lx * dv_dx + ly * dv_dy;
                if (u > 2147483647ll || u < -2147483648ll || v > 2147483647ll ||
                    v < -2147483648ll) {
                    return ExecResult::failure(FaultCode::BAD_ADDRESS);
                }
                sxq = static_cast<i32>(u);
                syq = static_cast<i32>(v);
            }
            const auto stp = sample_and_blend(d, src_view, dst_view, x, y, sxq, syq);
            if (!stp.ok) {
                // Mid-command memory failure: partial write allowed (documented).
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
