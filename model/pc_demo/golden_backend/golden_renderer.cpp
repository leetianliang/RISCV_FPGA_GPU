#include "golden_renderer.hpp"

#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/tile_binner.hpp"

#include <cstring>

namespace gpu2d {
namespace {

golden::Rgba8888 to_golden(Color c) {
    return golden::Rgba8888::pack(c.a, c.r, c.g, c.b);
}

golden::PixelFormat to_gfmt(PixelFormat f) {
    switch (f) {
        case PixelFormat::ARGB8888:
            return golden::PixelFormat::ARGB8888;
        case PixelFormat::XRGB8888:
            return golden::PixelFormat::XRGB8888;
        case PixelFormat::INDEX8:
            return golden::PixelFormat::INDEX8;
        case PixelFormat::RGB565:
        default:
            return golden::PixelFormat::RGB565;
    }
}

u32 bpp_of(PixelFormat f) {
    return f == PixelFormat::ARGB8888 || f == PixelFormat::XRGB8888 ? 4u
           : f == PixelFormat::INDEX8                                 ? 1u
                                                                      : 2u;
}

u32 packed_stride(u32 w, PixelFormat f) { return w * bpp_of(f); }

// Fixed large capacities registered once at init.
constexpr u32 kMaxDesc = 8192;
constexpr u32 kMaxHeaders = 4096;
constexpr u32 kMaxWork = 65536;
constexpr u32 kMaxTex = 64;

}  // namespace

struct GoldenBackend::Impl {
    std::unique_ptr<golden::GoldenGPU> gpu;
    std::vector<u8> fb_cache;
};

GoldenBackend::GoldenBackend() = default;
GoldenBackend::~GoldenBackend() = default;

bool GoldenBackend::init(const ProfileDesc& profile) {
    profile_ = profile;
    if (profile_.width == 0 || profile_.height == 0) {
        return false;
    }
    if (profile_.format != PixelFormat::RGB565 &&
        profile_.format != PixelFormat::ARGB8888 &&
        profile_.format != PixelFormat::XRGB8888) {
        return false;
    }
    fb_stride_ = packed_stride(profile_.width, profile_.format);
    tile_size_ = profile_.tile_size ? profile_.tile_size : 32;
    texs_.clear();
    texs_.emplace_back();
    tex_arena_cursor_ = tex_arena_base_;
    ext_cursor_ = ext_arena_base_;
    impl_ = std::make_unique<Impl>();
    impl_->gpu = std::make_unique<golden::GoldenGPU>();
    auto& gpu = *impl_->gpu;
    const u32 fb_bytes = fb_stride_ * profile_.height;
    if (!gpu.register_surface(
                golden::SurfaceDesc{fb_base_, fb_stride_, profile_.width, profile_.height,
                                    to_gfmt(profile_.format)},
                "rt")
             .ok) {
        return false;
    }
    std::vector<golden::u8> zeros(fb_bytes, 0);
    gpu.memory().write_block(fb_base_, zeros.data(), zeros.size());

    // Pre-register Tile arrays at fixed bases (capacity for stress frames).
    const u32 desc_bytes = kMaxDesc * 64;
    const u32 hdr_bytes = kMaxHeaders * 16;
    const u32 work_bytes = kMaxWork * 4;
    if (!gpu.register_resource(
                golden::RegisteredResource{desc_base_, desc_bytes, 1, 1, "td"})
             .ok) {
        return false;
    }
    if (!gpu.register_resource(
                golden::RegisteredResource{hdr_base_, hdr_bytes, 1, 1, "th"})
             .ok) {
        return false;
    }
    if (!gpu.register_resource(
                golden::RegisteredResource{work_base_, work_bytes, 1, 1, "tw"})
             .ok) {
        return false;
    }
    if (!gpu.register_resource(
                golden::RegisteredResource{ext_arena_base_, 0x100000, 1, 1, "extarena"})
             .ok) {
        return false;
    }

    inited_ = true;
    last_fault_ = 0;
    backend_ = BackendKind::Immediate;
    return true;
}

void GoldenBackend::shutdown() {
    impl_.reset();
    inited_ = false;
    texs_.clear();
}

TextureId GoldenBackend::create_texture(const TextureDesc& desc) {
    if (!inited_ || desc.width == 0 || desc.height == 0 || !desc.pixels) {
        return TextureId{};
    }
    const u32 stride = desc.stride ? desc.stride : packed_stride(desc.width, desc.format);
    const u32 nbytes = stride * desc.height;
    const u32 pal_bytes = (desc.format == PixelFormat::INDEX8 || desc.palette) ? 1024u : 0u;
    const u32 need = nbytes + pal_bytes + 64;
    if (tex_arena_cursor_ + need > tex_arena_limit_ ||
        texs_.size() > kMaxTex) {
        return TextureId{};
    }
    TexSlot slot;
    slot.base = tex_arena_cursor_;
    slot.width = desc.width;
    slot.height = desc.height;
    slot.stride = stride;
    slot.format = desc.format;
    slot.used = true;
    auto& gpu = *impl_->gpu;
    if (!gpu.register_resource(golden::RegisteredResource{slot.base, nbytes, desc.width,
                                                          desc.height, "tex"})
             .ok) {
        return TextureId{};
    }
    std::vector<golden::u8> bytes(nbytes);
    std::memcpy(bytes.data(), desc.pixels, nbytes);
    gpu.memory().write_block(slot.base, bytes.data(), bytes.size());
    if (pal_bytes) {
        slot.palette_base = slot.base + nbytes;
        if (!gpu.register_resource(
                    golden::RegisteredResource{slot.palette_base, 1024, 1, 1, "pal"})
                 .ok) {
            return TextureId{};
        }
        std::vector<golden::u8> pal4(1024, 0);
        if (desc.palette) {
            std::memcpy(pal4.data(), desc.palette, 1024);
        }
        gpu.memory().write_block(slot.palette_base, pal4.data(), pal4.size());
    }
    tex_arena_cursor_ += need;
    texs_.push_back(slot);
    return TextureId{static_cast<u32>(texs_.size() - 1)};
}

bool GoldenBackend::texture_valid(TextureId id) const {
    return id.valid() && id.v < texs_.size() && texs_[id.v].used;
}

void GoldenBackend::set_backend(BackendKind kind) {
    if (kind == BackendKind::Immediate || kind == BackendKind::Tile32 ||
        kind == BackendKind::Tile16 || kind == BackendKind::Tile64) {
        backend_ = kind;
        // R2-09: enum name is authoritative for tile size.
        if (kind == BackendKind::Tile16) {
            tile_size_ = 16;
        } else if (kind == BackendKind::Tile32) {
            tile_size_ = 32;
        } else if (kind == BackendKind::Tile64) {
            tile_size_ = 64;
        }
        // Immediate: leave tile_size_ as telemetry default
    }
}

const u8* GoldenBackend::framebuffer() const {
    if (!inited_) {
        return nullptr;
    }
    const u32 n = fb_stride_ * profile_.height;
    impl_->fb_cache.resize(n);
    const auto st = impl_->gpu->memory().read_block(fb_base_, n, impl_->fb_cache);
    if (st.status != golden::MemAccessStatus::OK) {
        return nullptr;
    }
    return impl_->fb_cache.data();
}

bool GoldenBackend::execute_frame(const std::vector<RecCommand>& cmds) {
    if (!inited_) {
        return false;
    }
    last_fault_ = 0;
    tel_ = TelemetrySnapshot{};
    tel_.command_count = static_cast<u32>(cmds.size());
    tel_.sprite_count = 0;
    for (const auto& c : cmds) {
        if (c.op == RecOp::Sprite) {
            ++tel_.sprite_count;
        }
    }
    tel_.tile_size = tile_size_;
    if (backend_ == BackendKind::Immediate) {
        return run_immediate(cmds);
    }
    return run_tile(cmds);
}

namespace {

bool build_blit_desc(u32 tex_base, u32 tex_stride, PixelFormat tex_fmt, u32 tex_pal,
                     const SpriteParams& sp, const RecCommand& c, PixelFormat dst_fmt,
                     u32 fb_base, u32 fb_stride, u32 fb_w, u32 fb_h, u32& ext_ptr_base,
                     golden::BlitCmdDesc& d, bool& need_ext) {
    d = golden::BlitCmdDesc{};
    d.src_base = tex_base;
    d.dst_base = fb_base;
    d.src_stride = tex_stride;
    d.dst_stride = fb_stride;
    d.src_x = sp.src_x;
    d.src_y = sp.src_y;
    d.dst_x = sp.dst_x;
    d.dst_y = sp.dst_y;
    const u32 sw = sp.src_w ? sp.src_w : sp.w;
    const u32 sh = sp.src_h ? sp.src_h : sp.h;
    const i32 dw = sp.scale_w ? sp.scale_w : static_cast<i32>(sp.w ? sp.w : sw);
    const i32 dh = sp.scale_h ? sp.scale_h : static_cast<i32>(sp.h ? sp.h : sh);
    d.w = sp.w ? sp.w : (sw ? sw : 1);
    d.h = sp.h ? sp.h : (sh ? sh : 1);
    d.src_format = static_cast<golden::u32>(tex_fmt);
    d.dst_format = static_cast<golden::u32>(dst_fmt);
    d.blend = static_cast<golden::u32>(sp.blend);
    d.filter = static_cast<golden::u32>(sp.filter);
    d.addr_u = static_cast<golden::u32>(sp.addr);
    d.addr_v = static_cast<golden::u32>(sp.addr);
    d.color_key_en = sp.color_key;
    d.color_key_rgb = sp.color_key_rgb;
    d.global_alpha_en = sp.global_alpha != 255;
    d.global_alpha = sp.global_alpha;
    d.pixel_alpha_en = sp.blend != BlendMode::Copy;
    d.flip_x = sp.flip_x;
    d.flip_y = sp.flip_y;
    d.palette_en = sp.palette || tex_fmt == PixelFormat::INDEX8;
    d.palette_addr = tex_pal;
    d.premult = sp.premult;
    d.color_mod_en = sp.color_mod;
    d.primary_color = to_golden(sp.mod);
    d.dither_en = sp.dither;
    // Negative dest requires clip_en (Command decoder contract).
    d.clip_en = c.clip_en || sp.dst_x < 0 || sp.dst_y < 0;
    if (c.clip_en) {
        d.clip_xmin = c.clip.xmin;
        d.clip_ymin = c.clip.ymin;
        d.clip_xmax = c.clip.xmax;
        d.clip_ymax = c.clip.ymax;
    } else {
        d.clip_xmin = 0;
        d.clip_ymin = 0;
        d.clip_xmax = static_cast<i32>(fb_w ? fb_w - 1 : 0x7FFF);
        d.clip_ymax = static_cast<i32>(fb_h ? fb_h - 1 : 0x7FFF);
    }
    const bool scaled =
        (dw != static_cast<i32>(d.w)) || (dh != static_cast<i32>(d.h));
    need_ext = scaled || d.clip_en || sp.filter == FilterMode::Bilinear;
    if (need_ext) {
        d.blit_ext = true;
        d.ext_ptr = ext_ptr_base;
        ext_ptr_base += 64;
        golden::compute_axis_aligned_uv(sp.src_x, d.w, dw ? dw : static_cast<u32>(d.w), d.u0,
                                        d.du_dx);
        golden::compute_axis_aligned_uv_v(sp.src_y, d.h, dh ? dh : static_cast<u32>(d.h),
                                          d.v0, d.dv_dy);
    }
    return true;
}

// Deterministic CPU clip for FILL (Golden FILL has no clip extension).
// Returns false if intersection is empty (emit no draw).
bool clip_fill_rect(const RecCommand& c, u32 fb_w, u32 fb_h, i32& x, i32& y, u32& w, u32& h) {
    if (c.fw == 0 || c.fh == 0) {
        return false;
    }
    i32 x0 = c.fx;
    i32 y0 = c.fy;
    i32 x1 = c.fx + static_cast<i32>(c.fw);
    i32 y1 = c.fy + static_cast<i32>(c.fh);
    // RT bounds
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > static_cast<i32>(fb_w)) {
        x1 = static_cast<i32>(fb_w);
    }
    if (y1 > static_cast<i32>(fb_h)) {
        y1 = static_cast<i32>(fb_h);
    }
    if (c.clip_en) {
        if (x0 < c.clip.xmin) {
            x0 = c.clip.xmin;
        }
        if (y0 < c.clip.ymin) {
            y0 = c.clip.ymin;
        }
        if (x1 > c.clip.xmax) {
            x1 = c.clip.xmax;
        }
        if (y1 > c.clip.ymax) {
            y1 = c.clip.ymax;
        }
    }
    if (x1 <= x0 || y1 <= y0) {
        return false;
    }
    x = x0;
    y = y0;
    w = static_cast<u32>(x1 - x0);
    h = static_cast<u32>(y1 - y0);
    return true;
}

}  // namespace

bool GoldenBackend::run_immediate(const std::vector<RecCommand>& cmds) {
    auto& gpu = *impl_->gpu;
    u32 ext_next = ext_cursor_;
    for (const auto& c : cmds) {
        golden::ExecResult st;
        if (c.op == RecOp::Fill) {
            i32 x = 0, y = 0;
            u32 w = 0, h = 0;
            if (!clip_fill_rect(c, profile_.width, profile_.height, x, y, w, h)) {
                continue;  // fully clipped → no-op
            }
            st = gpu.execute_command(golden::make_fill_rect_cmd(
                fb_base_, fb_stride_, x, y, w, h, to_golden(c.fcolor)));
        } else {
            if (!texture_valid(c.sp.tex)) {
                last_fault_ = 0xFFFF;
                return false;
            }
            const TexSlot& tex = texs_[c.sp.tex.v];
            golden::BlitCmdDesc d;
            bool need_ext = false;
            build_blit_desc(tex.base, tex.stride, tex.format, tex.palette_base, c.sp, c,
                            profile_.format, fb_base_, fb_stride_, profile_.width,
                            profile_.height, ext_next, d, need_ext);
            if (need_ext) {
                const auto ext = golden::make_draw2d_ext_v1(d);
                const auto wst = gpu.memory().write_block(d.ext_ptr, ext.data(), ext.size());
                if (wst.status != golden::MemAccessStatus::OK) {
                    last_fault_ = 0xFFFE;
                    return false;
                }
                auto cmd = golden::make_blit_ext_cmd(d);
                const i32 dw = c.sp.scale_w ? c.sp.scale_w : static_cast<i32>(d.w);
                const i32 dh = c.sp.scale_h ? c.sp.scale_h : static_cast<i32>(d.h);
                if (dw != static_cast<i32>(d.w) || dh != static_cast<i32>(d.h)) {
                    // Match Stage-004: cmd[10]=src, cmd[11]=dest
                    cmd[10] = (static_cast<golden::u32>(d.h) << 16) |
                              static_cast<golden::u32>(d.w);
                    cmd[11] = (static_cast<golden::u32>(dh & 0xFFFF) << 16) |
                              static_cast<golden::u32>(dw & 0xFFFF);
                }
                st = gpu.execute_command(cmd);
            } else {
                st = gpu.execute_command(golden::make_blit_cmd(d));
            }
        }
        if (!st.ok) {
            last_fault_ = static_cast<u32>(st.fault);
            last_fault_index_ = st.fault_index ? st.fault_index : 0;
            // track command ordinal
            last_fault_index_ = static_cast<u32>(&c - cmds.data());
            return false;
        }
    }
    ext_cursor_ = (ext_next > ext_arena_base_) ? ext_next : ext_cursor_;
    if (ext_cursor_ >= ext_arena_base_ + 0xF0000) {
        ext_cursor_ = ext_arena_base_;
    }
    return true;
}

bool GoldenBackend::run_tile(const std::vector<RecCommand>& cmds) {
    auto& gpu = *impl_->gpu;
    std::vector<golden::GpuCmd64> draws;
    draws.reserve(cmds.size());
    u32 ext_next = ext_cursor_;
    for (const auto& c : cmds) {
        if (c.op == RecOp::Fill) {
            i32 x = 0, y = 0;
            u32 w = 0, h = 0;
            if (!clip_fill_rect(c, profile_.width, profile_.height, x, y, w, h)) {
                continue;
            }
            draws.push_back(golden::make_fill_rect_cmd(fb_base_, fb_stride_, x, y, w, h,
                                                       to_golden(c.fcolor)));
            continue;
        }
        if (!texture_valid(c.sp.tex)) {
            last_fault_ = 0xFFFF;
            return false;
        }
        const TexSlot& tex = texs_[c.sp.tex.v];
        golden::BlitCmdDesc d;
        bool need_ext = false;
        build_blit_desc(tex.base, tex.stride, tex.format, tex.palette_base, c.sp, c,
                        profile_.format, fb_base_, fb_stride_, profile_.width,
                        profile_.height, ext_next, d, need_ext);
        if (need_ext) {
            const auto ext = golden::make_draw2d_ext_v1(d);
            gpu.memory().write_block(d.ext_ptr, ext.data(), ext.size());
            auto cmd = golden::make_blit_ext_cmd(d);
            const i32 dw = c.sp.scale_w ? c.sp.scale_w : static_cast<i32>(d.w);
            const i32 dh = c.sp.scale_h ? c.sp.scale_h : static_cast<i32>(d.h);
            if (dw != static_cast<i32>(d.w) || dh != static_cast<i32>(d.h)) {
                cmd[11] = (static_cast<golden::u32>(dh & 0xFFFF) << 16) |
                          static_cast<golden::u32>(dw & 0xFFFF);
            }
            draws.push_back(cmd);
        } else {
            draws.push_back(golden::make_blit_cmd(d));
        }
    }
    ext_cursor_ = ext_next;
    if (ext_cursor_ >= ext_arena_base_ + 0xF0000) {
        ext_cursor_ = ext_arena_base_;
    }

    if (draws.empty()) {
        // Clear-only / empty frame still needs a legal TILE_FRAME with no work.
        // Use a single no-op fill of 1x1 at (0,0) with copy of itself via zero fill
        // is wasteful — instead clear dest via fill and skip tile path.
        return true;
    }
    if (draws.size() > kMaxDesc) {
        last_fault_ = 0xFFFE;
        return false;
    }

    const auto bin = golden::bin_draws(draws, gpu.memory(), profile_.width, profile_.height,
                                       tile_size_);
    if (bin.headers.size() > kMaxHeaders || bin.workrefs.size() > kMaxWork) {
        last_fault_ = 0xFFFD;
        return false;
    }

    const u32 n_desc = static_cast<u32>(draws.size());
    const u32 n_hdr = static_cast<u32>(bin.headers.size());
    for (u32 i = 0; i < n_desc; ++i) {
        const auto b = golden::serialize_cmd_le(draws[i]);
        gpu.memory().write_block(desc_base_ + i * 64, b.data(), b.size());
    }
    for (u32 i = 0; i < n_hdr; ++i) {
        const auto hb = golden::serialize_tile_header(bin.headers[i]);
        gpu.memory().write_block(hdr_base_ + i * 16, hb.data(), hb.size());
    }
    const auto wr = golden::serialize_workrefs(bin.workrefs);
    if (!wr.empty()) {
        gpu.memory().write_block(work_base_, wr.data(), wr.size());
    }

    golden::TileFrameCmd tf;
    tf.draw_desc_base = desc_base_;
    tf.tile_header_base = hdr_base_;
    tf.work_list_base = work_base_;
    tf.dst_base = fb_base_;
    tf.dst_stride = fb_stride_;
    tf.surface_w = profile_.width;
    tf.surface_h = profile_.height;
    tf.grid_w = (profile_.width + tile_size_ - 1) / tile_size_;
    tf.grid_h = (profile_.height + tile_size_ - 1) / tile_size_;
    tf.tile_w = tile_size_;
    tf.tile_h = tile_size_;
    tf.rt_state = golden::tile_rt_state_store(to_gfmt(profile_.format));

    const auto st = golden::execute_tile_frame(gpu, golden::make_tile_frame_cmd(tf));
    if (!st.ok) {
        last_fault_ = static_cast<u32>(st.fault);
        return false;
    }
    const auto ts = golden::last_tile_stats();
    tel_.workref_count = ts.workref_count;
    tel_.tiles_total = ts.tiles_total;
    tel_.tiles_active = ts.tiles_active;
    tel_.max_workrefs_per_tile = ts.max_workrefs_per_tile;
    tel_.max_overdraw = ts.max_overdraw;
    tel_.grid_w = tf.grid_w;
    tel_.grid_h = tf.grid_h;
    if (!ts.overdraw_matrix.empty()) {
        tel_.has_overdraw = true;
        tel_.overdraw = ts.overdraw_matrix;
    }
    workref_map_.assign(static_cast<size_t>(tf.grid_w) * tf.grid_h, 0);
    for (size_t i = 0; i < bin.headers.size() && i < workref_map_.size(); ++i) {
        workref_map_[i] = static_cast<u16>(bin.headers[i].work_count);
    }
    tel_.has_workref_map = true;
    tel_.tile_workrefs = workref_map_;
    return true;
}

}  // namespace gpu2d
