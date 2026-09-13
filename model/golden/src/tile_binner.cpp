#include "golden/tile_binner.hpp"

#include "golden/gpu_math.hpp"

namespace golden {

namespace {

TileStats g_last_stats;

// Decode one descriptor (with optional extension) for binning bounds.
DecodedDraw decode_desc_for_bin(MemoryImage& mem, const GpuCmd64& cmd) {
    std::array<u8, 64> ext{};
    const u8* ext_ptr = nullptr;
    const DecodedHeader hdr = decode_cmd_header(cmd);
    if (!hdr.ok) {
        DecodedDraw bad;
        bad.ok = false;
        bad.fault = hdr.fault;
        return bad;
    }
    if ((hdr.header.hdr_flags & kHExtValid) != 0) {
        std::vector<u8> tmp;
        if (mem.read_block(hdr.header.ext_ptr, 64, tmp).status ==
            MemAccessStatus::OK) {
            std::copy(tmp.begin(), tmp.end(), ext.begin());
            ext_ptr = ext.data();
        }
    }
    return decode_draw_2d(cmd, ext_ptr, ext_ptr ? 64u : 0u);
}

void effective_raster(const Draw2DState& st, u32 surface_w, u32 surface_h, i32& rx0,
                      i32& ry0, i32& rx1, i32& ry1) {
    rx0 = st.dst_x;
    ry0 = st.dst_y;
    rx1 = st.dst_x + static_cast<i32>(st.dst_w);
    ry1 = st.dst_y + static_cast<i32>(st.dst_h);
    if (extract_clip_en(st.draw_state)) {
        if (rx0 < st.clip_xmin) rx0 = st.clip_xmin;
        if (ry0 < st.clip_ymin) ry0 = st.clip_ymin;
        if (rx1 > st.clip_xmax) rx1 = st.clip_xmax;
        if (ry1 > st.clip_ymax) ry1 = st.clip_ymax;
    }
    if (rx0 < 0) rx0 = 0;
    if (ry0 < 0) ry0 = 0;
    if (rx1 > static_cast<i32>(surface_w)) rx1 = static_cast<i32>(surface_w);
    if (ry1 > static_cast<i32>(surface_h)) ry1 = static_cast<i32>(surface_h);
}

}  // namespace

TileBinResult bin_draws(const std::vector<GpuCmd64>& draws, const MemoryImage& mem_src,
                        u32 surface_w, u32 surface_h, u32 tile_size) {
    MemoryImage mem = mem_src;  // local copy only for const decode reads
    TileBinResult out;
    out.descriptors = draws;
    const TileGridInfo grid = make_grid(surface_w, surface_h, tile_size);
    out.headers.assign(static_cast<size_t>(grid.tiles_x) * grid.tiles_y, TileHeader{});

    std::vector<std::vector<WorkRef>> per_tile(out.headers.size());

    for (size_t di = 0; di < draws.size(); ++di) {
        const DecodedDraw d = decode_desc_for_bin(mem, draws[di]);
        if (!d.ok) {
            continue;  // invalid draw: no workrefs
        }
        if (d.state.dst_w == 0 || d.state.dst_h == 0) {
            continue;
        }
        i32 rx0, ry0, rx1, ry1;
        effective_raster(d.state, surface_w, surface_h, rx0, ry0, rx1, ry1);
        if (rx0 >= rx1 || ry0 >= ry1) {
            continue;
        }
        const u32 tx0 = static_cast<u32>(rx0) / tile_size;
        const u32 ty0 = static_cast<u32>(ry0) / tile_size;
        const u32 tx1 = static_cast<u32>(rx1 - 1) / tile_size;
        const u32 ty1 = static_cast<u32>(ry1 - 1) / tile_size;
        for (u32 ty = ty0; ty <= ty1; ++ty) {
            for (u32 tx = tx0; tx <= tx1; ++tx) {
                const u32 id = tile_id(tx, ty, grid.tiles_x);
                if (id < per_tile.size()) {
                    per_tile[id].push_back(static_cast<WorkRef>(di));
                }
            }
        }
    }

    u32 offset = 0;
    u32 max_refs = 0;
    u32 sum = 0;
    for (size_t i = 0; i < per_tile.size(); ++i) {
        out.headers[i].work_offset = offset;
        out.headers[i].work_count = static_cast<u32>(per_tile[i].size());
        out.headers[i].flags = 0;
        for (WorkRef r : per_tile[i]) {
            out.workrefs.push_back(r);
        }
        offset += static_cast<u32>(per_tile[i].size());
        sum += static_cast<u32>(per_tile[i].size());
        if (per_tile[i].size() > max_refs) {
            max_refs = static_cast<u32>(per_tile[i].size());
        }
    }

    g_last_stats = TileStats{};
    g_last_stats.tiles_total = static_cast<u32>(out.headers.size());
    for (const auto& h : out.headers) {
        if (h.work_count > 0) {
            ++g_last_stats.tiles_active;
        }
    }
    g_last_stats.draw_descriptor_count = static_cast<u32>(draws.size());
    g_last_stats.workref_count = static_cast<u32>(out.workrefs.size());
    g_last_stats.max_workrefs_per_tile = max_refs;
    g_last_stats.sum_workrefs = sum;
    return out;
}

std::vector<u8> serialize_workrefs(const std::vector<WorkRef>& refs) {
    std::vector<u8> out(refs.size() * 4);
    for (size_t i = 0; i < refs.size(); ++i) {
        const u32 w = refs[i];
        out[i * 4 + 0] = static_cast<u8>(w & 0xFF);
        out[i * 4 + 1] = static_cast<u8>((w >> 8) & 0xFF);
        out[i * 4 + 2] = static_cast<u8>((w >> 16) & 0xFF);
        out[i * 4 + 3] = static_cast<u8>((w >> 24) & 0xFF);
    }
    return out;
}

ExecResult execute_tile_frame(GoldenGPU& gpu, const GpuCmd64& tile_cmd) {
    g_last_stats = TileStats{};
    const DecodedHeader hdr = decode_cmd_header(tile_cmd);
    if (!hdr.ok) {
        return ExecResult::failure(hdr.fault, hdr.fault_detail);
    }
    if (hdr.header.opcode != kOpcodeTileFrame) {
        return ExecResult::failure(FaultCode::BAD_OPCODE, hdr.header.opcode);
    }
    const TileFrameCmd tf = decode_tile_frame_cmd(tile_cmd);
    const TileFrameState rt = decode_rt_state(tf.rt_state);
    const auto tile_fmt = static_cast<PixelFormat>(rt.dst_format);
    const u32 bpp = bytes_per_pixel(tile_fmt);
    if (bpp == 0 || tile_fmt == PixelFormat::INDEX8) {
        return ExecResult::failure(FaultCode::BAD_FORMAT, tf.rt_state);
    }
    if (tf.tile_w == 0 || tf.tile_h == 0 || tf.grid_w == 0 || tf.grid_h == 0) {
        return ExecResult::failure(FaultCode::BAD_TILE_CONFIG, tf.rt_state);
    }
    // Grid must exactly cover the surface (V0.1).
    const u32 expect_gw = (tf.surface_w + tf.tile_w - 1) / tf.tile_w;
    const u32 expect_gh = (tf.surface_h + tf.tile_h - 1) / tf.tile_h;
    if (tf.grid_w != expect_gw || tf.grid_h != expect_gh) {
        return ExecResult::failure(FaultCode::BAD_TILE_CONFIG, tf.rt_state);
    }
    if (rt.depth_enable || rt.store_depth) {
        return ExecResult::failure(FaultCode::UNSUPPORTED_FEATURE, tf.rt_state);
    }
    // Reserved RT_STATE bits [31:9] — reject when nonzero (architectural safety).
    if ((tf.rt_state >> 9) != 0) {
        return ExecResult::failure(FaultCode::RESERVED_NONZERO, tf.rt_state);
    }

    // Descriptor bounds from registered resource only (no host side-channel).
    auto dres = gpu.resource(tf.draw_desc_base);
    if (!dres) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, tf.draw_desc_base);
    }
    const u32 desc_capacity = dres->size / 64;
    if (desc_capacity == 0) {
        return ExecResult::failure(FaultCode::DESCRIPTOR_BOUNDS, dres->size);
    }

    // Destination allocation must cover the surface.
    auto fres = gpu.resource(tf.dst_base);
    if (!fres) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, tf.dst_base);
    }
    {
        const u64 need = static_cast<u64>(tf.surface_h) * tf.dst_stride;
        const u64 last = static_cast<u64>(tf.dst_base) + need;
        if (need > fres->size || last > (1ull << 32)) {
            return ExecResult::failure(FaultCode::BAD_RECT, tf.dst_stride);
        }
        if (tf.dst_stride < static_cast<u64>(tf.surface_w) * bpp) {
            return ExecResult::failure(FaultCode::BAD_RECT, tf.dst_stride);
        }
    }

    const u32 tiles = tf.grid_w * tf.grid_h;
    g_last_stats.tiles_total = tiles;
    g_last_stats.draw_descriptor_count = desc_capacity;

    // Internal scratch: separate MemoryImage, no architectural phys addr.
    MemoryImage& tile_mem = gpu.tile_memory();
    const u32 scratch_base = 0;
    const u32 scratch_size = tf.tile_w * tf.tile_h * bpp;
    tile_mem.clear();
    if (tile_mem.register_region("tile_internal", scratch_base, scratch_size).status !=
        MemAccessStatus::OK) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, scratch_size);
    }
    const RegisteredResource scratch_res{scratch_base, scratch_size, tf.tile_w,
                                         tf.tile_h, "tile_internal"};

    // Enable true pixel/sampler profiler for this frame.
    PixelEventSink sink;
    sink.reset_overdraw(tf.surface_w, tf.surface_h);
    PixelEventSink* prev_perf = gpu.perf;
    gpu.perf = &sink;

    for (u32 ty = 0; ty < tf.grid_h; ++ty) {
        for (u32 tx = 0; tx < tf.grid_w; ++tx) {
            const u32 tid = tile_id(tx, ty, tf.grid_w);
            const u32 header_addr = tf.tile_header_base + tid * kTileHeaderBytes;
            std::vector<u8> hbytes;
            if (gpu.memory().read_block(header_addr, kTileHeaderBytes, hbytes).status !=
                MemAccessStatus::OK) {
                return ExecResult::failure(FaultCode::MEMORY_ERROR, header_addr);
            }
            TileHeader th;
            u32 reserved_w3 = 0;
            if (!parse_tile_header(hbytes.data(), th, reserved_w3)) {
                return ExecResult::failure(FaultCode::BAD_TILE_CONFIG, header_addr);
            }
            if (reserved_w3 != 0) {
                return ExecResult::failure(FaultCode::RESERVED_NONZERO, header_addr);
            }
            // TILE_FLAGS reserved [31:4] and depth bits.
            if ((th.flags >> 4) != 0) {
                return ExecResult::failure(FaultCode::RESERVED_NONZERO, th.flags);
            }
            if (th.flags & (kTileLoadDepth | kTileClearDepth)) {
                return ExecResult::failure(FaultCode::UNSUPPORTED_FEATURE, th.flags);
            }
            if (th.work_count == 0 && (th.flags & kTileClearColor) == 0) {
                continue;
            }

            // Pre-read workrefs (and validate strict target match before load).
            std::vector<u8> wraw;
            std::vector<WorkRef> work_idx;
            if (th.work_count > 0) {
                const u64 woff_bytes = static_cast<u64>(tf.work_list_base) +
                                       static_cast<u64>(th.work_offset) * kWorkRefBytes;
                if (woff_bytes > 0xFFFFFFFFull) {
                    return ExecResult::failure(FaultCode::WORKLIST_BOUNDS,
                                               th.work_offset);
                }
                const u64 wbytes = static_cast<u64>(th.work_count) * kWorkRefBytes;
                if (gpu.memory()
                        .read_block(static_cast<u32>(woff_bytes),
                                    static_cast<size_t>(wbytes), wraw)
                        .status != MemAccessStatus::OK) {
                    return ExecResult::failure(FaultCode::WORKLIST_BOUNDS,
                                               static_cast<u32>(woff_bytes));
                }
                work_idx.resize(th.work_count);
                for (u32 wi = 0; wi < th.work_count; ++wi) {
                    work_idx[wi] = static_cast<WorkRef>(wraw[wi * 4 + 0]) |
                                   (static_cast<WorkRef>(wraw[wi * 4 + 1]) << 8) |
                                   (static_cast<WorkRef>(wraw[wi * 4 + 2]) << 16) |
                                   (static_cast<WorkRef>(wraw[wi * 4 + 3]) << 24);
                    if (work_idx[wi] >= desc_capacity) {
                        return ExecResult::failure(FaultCode::DESCRIPTOR_BOUNDS,
                                                   work_idx[wi]);
                    }
                    std::vector<u8> db;
                    const u64 daddr = static_cast<u64>(tf.draw_desc_base) +
                                      static_cast<u64>(work_idx[wi]) * 64ull;
                    if (daddr > 0xFFFFFFFFull ||
                        gpu.memory()
                                .read_block(static_cast<u32>(daddr), 64, db)
                                .status != MemAccessStatus::OK) {
                        return ExecResult::failure(FaultCode::DESCRIPTOR_BOUNDS,
                                                   work_idx[wi]);
                    }
                    GpuCmd64 desc{};
                    deserialize_cmd_le(db.data(), 64, desc);
                    if (rt.strict_target_match) {
                        if (desc[5] != tf.dst_base || desc[7] != tf.dst_stride ||
                            extract_dst_format(desc[12]) != rt.dst_format) {
                            return ExecResult::failure(FaultCode::TILE_TARGET_MISMATCH,
                                                       desc[12]);
                        }
                    }
                }
            }

            u32 x0, y0, vw, vh;
            tile_valid_rect(tx, ty, tf.tile_w, tf.surface_w, tf.surface_h, x0, y0, vw,
                            vh);
            if (vw == 0 || vh == 0) {
                continue;
            }
            ++g_last_stats.tiles_active;
            if (th.work_count > g_last_stats.max_workrefs_per_tile) {
                g_last_stats.max_workrefs_per_tile = th.work_count;
            }

            // LOAD tile from framebuffer into internal scratch (or clear / default).
            const bool dont_load = (th.flags & kTileDontLoadColor) != 0;
            const bool clear = (th.flags & kTileClearColor) != 0;
            auto scratch_off = [&](u32 lx, u32 ly) -> u64 {
                return static_cast<u64>(ly) * tf.tile_w * bpp + static_cast<u64>(lx) * bpp;
            };
            if (clear) {
                SurfaceView tile_sv(&tile_mem, scratch_res, tf.tile_w * bpp, tile_fmt);
                const Rgba8888 cc = Rgba8888::from_u32(tf.clear_color);
                for (u32 ly = 0; ly < vh; ++ly) {
                    for (u32 lx = 0; lx < vw; ++lx) {
                        tile_sv.write_rgba(static_cast<i32>(lx), static_cast<i32>(ly),
                                           cc, false, x0 + lx, y0 + ly);
                    }
                }
                g_last_stats.tile_store_pixels += vw * vh;  // cleared → store
            } else if (dont_load) {
                if (!rt.load_color_default) {
                    return ExecResult::failure(FaultCode::UNSUPPORTED_FEATURE,
                                               th.flags);
                }
                SurfaceView tile_sv(&tile_mem, scratch_res, tf.tile_w * bpp, tile_fmt);
                for (u32 ly = 0; ly < vh; ++ly) {
                    for (u32 lx = 0; lx < vw; ++lx) {
                        tile_sv.write_rgba(static_cast<i32>(lx), static_cast<i32>(ly),
                                           Rgba8888::pack(0, 0, 0, 0), false,
                                           x0 + lx, y0 + ly);
                    }
                }
            } else {
                // LOAD from framebuffer into internal scratch (raw bytes).
                auto fb_res = gpu.resource(tf.dst_base);
                if (!fb_res) {
                    return ExecResult::failure(FaultCode::MEMORY_ERROR, tf.dst_base);
                }
                for (u32 ly = 0; ly < vh; ++ly) {
                    for (u32 lx = 0; lx < vw; ++lx) {
                        const u64 fb_addr =
                            static_cast<u64>(tf.dst_base) +
                            static_cast<u64>(y0 + ly) * tf.dst_stride +
                            static_cast<u64>(x0 + lx) * bpp;
                        if (fb_addr > 0xFFFFFFFFull) {
                            return ExecResult::failure(FaultCode::BAD_ADDRESS);
                        }
                        std::vector<u8> tmp;
                        if (gpu.memory()
                                .read_block(static_cast<u32>(fb_addr), bpp, tmp)
                                .status != MemAccessStatus::OK) {
                            return ExecResult::failure(FaultCode::MEMORY_ERROR,
                                                       static_cast<u32>(fb_addr));
                        }
                        if (tile_mem
                                .write_block(static_cast<u32>(scratch_off(lx, ly)),
                                             tmp.data(), bpp)
                                .status != MemAccessStatus::OK) {
                            return ExecResult::failure(FaultCode::MEMORY_ERROR,
                                                       scratch_off(lx, ly));
                        }
                    }
                }
                g_last_stats.tile_load_bytes += vw * vh * bpp;
                g_last_stats.tile_load_pixels += vw * vh;
            }

            if (th.work_count == 0) {
                if (rt.store_color) {
                    for (u32 ly = 0; ly < vh; ++ly) {
                        for (u32 lx = 0; lx < vw; ++lx) {
                            std::vector<u8> px(bpp);
                            tile_mem.read_block(static_cast<u32>(scratch_off(lx, ly)),
                                                bpp, px);
                            const u64 fb = static_cast<u64>(tf.dst_base) +
                                           static_cast<u64>(y0 + ly) * tf.dst_stride +
                                           static_cast<u64>(x0 + lx) * bpp;
                            gpu.memory().write_block(static_cast<u32>(fb), px.data(),
                                                     bpp);
                        }
                    }
                    g_last_stats.tile_store_bytes += vw * vh * bpp;
                    g_last_stats.tile_store_pixels += vw * vh;
                }
                continue;
            }

            for (u32 wi = 0; wi < th.work_count; ++wi) {
                const WorkRef idx = work_idx[wi];
                if (idx >= desc_capacity) {
                    return ExecResult::failure(FaultCode::DESCRIPTOR_BOUNDS, idx);
                }
                const u64 daddr = static_cast<u64>(tf.draw_desc_base) +
                                  static_cast<u64>(idx) * 64ull;
                if (daddr > 0xFFFFFFFFull) {
                    return ExecResult::failure(FaultCode::DESCRIPTOR_BOUNDS, idx);
                }
                std::vector<u8> db;
                if (gpu.memory().read_block(static_cast<u32>(daddr), 64, db).status !=
                    MemAccessStatus::OK) {
                    return ExecResult::failure(FaultCode::DESCRIPTOR_BOUNDS,
                                               static_cast<u32>(daddr));
                }
                GpuCmd64 desc{};
                if (!deserialize_cmd_le(db.data(), 64, desc)) {
                    return ExecResult::failure(FaultCode::DESCRIPTOR_BOUNDS, idx);
                }
                std::array<u8, 64> ext{};
                const u8* ext_ptr = nullptr;
                const DecodedHeader dh = decode_cmd_header(desc);
                if (!dh.ok) {
                    return ExecResult::failure(dh.fault, dh.fault_detail);
                }
                if ((dh.header.hdr_flags & kHExtValid) != 0) {
                    std::vector<u8> eb;
                    if (gpu.memory().read_block(dh.header.ext_ptr, 64, eb).status !=
                        MemAccessStatus::OK) {
                        return ExecResult::failure(FaultCode::MEMORY_ERROR,
                                                   dh.header.ext_ptr);
                    }
                    std::copy(eb.begin(), eb.end(), ext.begin());
                    ext_ptr = ext.data();
                }
                DecodedDraw dd = decode_draw_2d(desc, ext_ptr, ext_ptr ? 64u : 0u);
                if (!dd.ok) {
                    return ExecResult::failure(dd.fault, dd.fault_detail);
                }

                // TILE_FRAME target is authoritative.
                const u32 desc_dst_fmt =
                    extract_dst_format(dd.state.draw_state);
                if (rt.strict_target_match) {
                    if (dd.state.dst_base != tf.dst_base ||
                        dd.state.dst_stride != tf.dst_stride ||
                        desc_dst_fmt != rt.dst_format) {
                        return ExecResult::failure(FaultCode::TILE_TARGET_MISMATCH,
                                                   desc_dst_fmt);
                    }
                }

                // Translate draw into tile-local coordinates targeting internal scratch.
                DecodedDraw local = dd;
                local.state.dst_base = scratch_base;
                local.state.dst_stride = tf.tile_w * bpp;
                local.state.draw_state =
                    (local.state.draw_state & ~(0xFu << 4)) | (rt.dst_format << 4);
                local.state.dst_x = dd.state.dst_x - static_cast<i32>(x0);
                local.state.dst_y = dd.state.dst_y - static_cast<i32>(y0);
                local.state.dither_ox = static_cast<i32>(x0);
                local.state.dither_oy = static_cast<i32>(y0);
                if (extract_clip_en(dd.state.draw_state)) {
                    local.state.clip_xmin = dd.state.clip_xmin - static_cast<i32>(x0);
                    local.state.clip_xmax = dd.state.clip_xmax - static_cast<i32>(x0);
                    local.state.clip_ymin = dd.state.clip_ymin - static_cast<i32>(y0);
                    local.state.clip_ymax = dd.state.clip_ymax - static_cast<i32>(y0);
                }
                const auto st = gpu.execute_decoded_on(
                    local, &tile_mem, scratch_res, tf.tile_w * bpp, tile_fmt, true, 0,
                    0, static_cast<i32>(vw), static_cast<i32>(vh));
                if (!st.ok) {
                    return st;
                }
                ++g_last_stats.workref_count;
            }
            g_last_stats.sum_workrefs += th.work_count;

            // STORE valid tile region from internal scratch back to framebuffer.
            if (rt.store_color) {
                for (u32 ly = 0; ly < vh; ++ly) {
                    for (u32 lx = 0; lx < vw; ++lx) {
                        std::vector<u8> px(bpp);
                        const u64 tb = static_cast<u64>(ly) * tf.tile_w * bpp +
                                       static_cast<u64>(lx) * bpp;
                        if (tile_mem
                                .read_block(static_cast<u32>(tb), bpp, px)
                                .status != MemAccessStatus::OK) {
                            return ExecResult::failure(FaultCode::MEMORY_ERROR,
                                                       static_cast<u32>(tb));
                        }
                        const u64 fb = static_cast<u64>(tf.dst_base) +
                                       static_cast<u64>(y0 + ly) * tf.dst_stride +
                                       static_cast<u64>(x0 + lx) * bpp;
                        if (fb > 0xFFFFFFFFull) {
                            return ExecResult::failure(FaultCode::BAD_ADDRESS);
                        }
                        if (gpu.memory()
                                .write_block(static_cast<u32>(fb), px.data(), bpp)
                                .status != MemAccessStatus::OK) {
                            return ExecResult::failure(FaultCode::MEMORY_ERROR,
                                                       static_cast<u32>(fb));
                        }
                    }
                }
                g_last_stats.tile_store_bytes += vw * vh * bpp;
                g_last_stats.tile_store_pixels += vw * vh;
            }
        }
    }
    // Copy true pixel/sampler events from sink.
    gpu.perf = prev_perf;
    g_last_stats.blend_ops = sink.blend_ops;
    g_last_stats.pixels_attempted = sink.pixels_attempted;
    g_last_stats.pixels_written = sink.pixels_written;
    g_last_stats.logical_pixel_writes = sink.pixels_written;
    g_last_stats.key_discards = sink.key_discards;
    g_last_stats.texture_samples = sink.texture_samples;
    g_last_stats.palette_reads = sink.palette_reads;
    g_last_stats.bilinear_samples = sink.bilinear_samples;
    g_last_stats.max_overdraw = sink.max_overdraw;
    g_last_stats.avg_overdraw_touched = sink.avg_overdraw_touched();
    g_last_stats.estimated_external_load_bytes = g_last_stats.tile_load_bytes;
    g_last_stats.estimated_external_store_bytes = g_last_stats.tile_store_bytes;
    return ExecResult::success();
}

TileStats last_tile_stats() { return g_last_stats; }

}  // namespace golden
