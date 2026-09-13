#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/tile_binner.hpp"

#include <cstdio>
#include <vector>

using namespace golden;

static std::vector<GpuCmd64> make_workload(int kind, u32 tw, u32 th, u32 stride) {
    std::vector<GpuCmd64> d;
    const u32 dst_base = 0x10000;
    auto fill = [&](u32 x, u32 y, u32 w, u32 h, Rgba8888 c) {
        if (x >= tw) return;
        if (y >= th) return;
        if (x + w > tw) w = tw - x;
        if (y + h > th) h = th - y;
        if (w == 0 || h == 0) return;
        d.push_back(make_fill_rect_cmd(dst_base, stride, static_cast<i32>(x),
                                       static_cast<i32>(y), w, h, c));
    };
    auto blit = [&](u32 x, u32 y, u32 w, u32 h, u32 sx, u32 sy) {
        if (w == 0 || h == 0) return;
        if (sx + w > 8) w = 8 - sx;
        if (sy + h > 8) h = 8 - sy;
        if (x + w > tw) w = tw - x;
        if (y + h > th) h = th - y;
        if (w == 0 || h == 0) return;
        BlitCmdDesc b;
        b.src_base = 0x20000;
        b.dst_base = dst_base;
        b.src_stride = 16;
        b.dst_stride = stride;
        b.src_x = sx;
        b.src_y = sy;
        b.w = w;
        b.h = h;
        b.dst_x = static_cast<i32>(x);
        b.dst_y = static_cast<i32>(y);
        d.push_back(make_blit_cmd(b));
    };
    auto blit_ext_scale = [&](u32 x, u32 y, u32 sw, u32 sh, u32 dw, u32 dh) {
        if (dw == 0 || dh == 0) return;
        if (x + dw > tw) dw = tw - x;
        if (y + dh > th) dh = th - y;
        if (dw == 0 || dh == 0) return;
        BlitCmdDesc b;
        b.src_base = 0x20000;
        b.dst_base = dst_base;
        b.src_stride = 16;
        b.dst_stride = stride;
        b.blit_ext = true;
        b.ext_ptr = 0x30000;
        b.w = dw;
        b.h = dh;
        b.dst_x = static_cast<i32>(x);
        b.dst_y = static_cast<i32>(y);
        compute_axis_aligned_uv(0, sw, dw, b.u0, b.du_dx);
        compute_axis_aligned_uv_v(0, sh, dh, b.v0, b.dv_dy);
        auto cmd = make_blit_ext_cmd(b);
        cmd[10] = pack_wh(sw, sh);
        cmd[11] = pack_wh(dw, dh);
        d.push_back(cmd);
    };

    switch (kind) {
        case 0: {  // W1 sprite grid
            for (u32 gy = 0; gy < th; gy += 8) {
                for (u32 gx = 0; gx < tw; gx += 8) {
                    blit(gx, gy, 6, 6, 1, 1);
                }
            }
            break;
        }
        case 1: {  // W3 alpha storm
            for (int i = 0; i < 80; ++i) {
                auto c = make_fill_rect_cmd(
                    dst_base, stride, static_cast<i32>((i * 5) % tw),
                    static_cast<i32>((i * 7) % th), 8, 8,
                    Rgba8888::pack(128, 255, 0, 0));
                u32 ds = c[12];
                ds = (ds & ~(0xFu << 8)) |
                     (static_cast<u32>(BlendMode::STRAIGHT_ALPHA) << 8);
                ds |= (1u << 20);
                c[12] = ds;
                c[14] = (128u << 24);
                d.push_back(c);
            }
            break;
        }
        case 2: {  // W2 high overdraw stack
            for (int i = 0; i < 64; ++i) {
                fill(8, 8, 16, 16, Rgba8888::pack(255, static_cast<u8>(i),
                                                 static_cast<u8>(i * 2),
                                                 static_cast<u8>(i * 3)));
            }
            break;
        }
        case 3: {  // W4 large scaled sprites
            for (u32 y = 0; y < th; y += 16) {
                for (u32 x = 0; x < tw; x += 16) {
                    blit_ext_scale(x, y, 4, 4, 16, 16);
                }
            }
            break;
        }
        case 4: {  // W5 edge/scatter
            fill(0, 0, 4, 4, Rgba8888::pack(255, 255, 0, 0));
            fill(tw - 3, th - 3, 3, 3, Rgba8888::pack(255, 0, 255, 0));
            fill(tw - 2, 0, 2, 2, Rgba8888::pack(255, 0, 0, 255));
            fill(0, th - 2, 2, 2, Rgba8888::pack(255, 255, 255, 0));
            fill(tw / 2, th / 2, 1, 1, Rgba8888::pack(255, 1, 2, 3));
            for (u32 i = 0; i < 20; ++i) {
                const u32 x = (i * 13) % tw;
                const u32 y = (i * 17) % th;
                fill(x, y, 1 + (i % 3), 1 + (i % 3),
                     Rgba8888::pack(255, static_cast<u8>(i * 4), 64, 32));
            }
            break;
        }
        default: {  // W6 mixed
            fill(0, 0, 8, 8, Rgba8888::pack(255, 10, 20, 30));
            blit(8, 8, 6, 6, 0, 0);
            blit_ext_scale(16, 16, 4, 4, 12, 12);
            auto c = make_fill_rect_cmd(dst_base, stride, 2, 18, 10, 10,
                                        Rgba8888::pack(128, 200, 0, 0));
            u32 ds = c[12];
            ds = (ds & ~(0xFu << 8)) |
                 (static_cast<u32>(BlendMode::STRAIGHT_ALPHA) << 8);
            ds |= (1u << 20);
            c[12] = ds;
            c[14] = (128u << 24);
            d.push_back(c);
            fill(1, 1, 3, 3, Rgba8888::pack(255, 255, 255, 255));
            break;
        }
    }
    return d;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: golden_tile_sweep <workload 0-5> <tile 16|32|64>\n");
        return 2;
    }
    const int kind = std::atoi(argv[1]);
    const u32 tile = static_cast<u32>(std::atoi(argv[2]));
    const u32 tw = 64, th = 64, stride = 128;
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, stride, tw, th, PixelFormat::RGB565},
                         "d");
    std::vector<u8> init(static_cast<size_t>(stride) * th, 0);
    gpu.memory().write_block(0x10000, init.data(), init.size());
    // RGB565 blit source
    std::vector<u8> tex(16 * 8, 0);
    for (size_t i = 0; i + 1 < tex.size(); i += 2) {
        const u16 px = static_cast<u16>((i * 13 + kind) & 0xFFFF);
        tex[i] = static_cast<u8>(px & 0xFF);
        tex[i + 1] = static_cast<u8>(px >> 8);
    }
    gpu.register_surface(SurfaceDesc{0x20000, 16, 8, 8, PixelFormat::RGB565}, "s");
    gpu.memory().write_block(0x20000, tex.data(), tex.size());
    gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    {
        BlitCmdDesc ed;
        auto ext = make_draw2d_ext_v1(ed);
        gpu.memory().write_block(0x30000, ext.data(), 64);
    }
    const auto draws = make_workload(kind, tw, th, stride);
    const auto bin = bin_draws(draws, gpu.memory(), tw, th, tile);
    const u32 desc_b = 0x38000, hdr_b = 0x40000, work_b = 0x50000;
    gpu.register_resource(RegisteredResource{
        desc_b, static_cast<u32>(draws.size() * 64), 1, 1, "d"});
    gpu.register_resource(RegisteredResource{
        hdr_b, static_cast<u32>(bin.headers.size() * 16), 1, 1, "h"});
    gpu.register_resource(RegisteredResource{
        work_b, static_cast<u32>(bin.workrefs.size() * 4), 1, 1, "w"});
    for (size_t i = 0; i < draws.size(); ++i) {
        const auto b = serialize_cmd_le(draws[i]);
        gpu.memory().write_block(desc_b + static_cast<u32>(i) * 64, b.data(), 64);
    }
    for (size_t i = 0; i < bin.headers.size(); ++i) {
        const auto hb = serialize_tile_header(bin.headers[i]);
        gpu.memory().write_block(hdr_b + static_cast<u32>(i) * 16, hb.data(), 16);
    }
    const auto wr = serialize_workrefs(bin.workrefs);
    if (!wr.empty()) {
        gpu.memory().write_block(work_b, wr.data(), wr.size());
    }
    TileFrameCmd tf;
    tf.draw_desc_base = desc_b;
    tf.tile_header_base = hdr_b;
    tf.work_list_base = work_b;
    tf.dst_base = 0x10000;
    tf.dst_stride = stride;
    tf.surface_w = tw;
    tf.surface_h = th;
    tf.grid_w = (tw + tile - 1) / tile;
    tf.grid_h = (th + tile - 1) / tile;
    tf.tile_w = tile;
    tf.tile_h = tile;
    tf.rt_state = tile_rt_state_store(PixelFormat::RGB565);
    const auto st = execute_tile_frame(gpu, make_tile_frame_cmd(tf));
    if (!st.ok) {
        std::fprintf(stderr, "sweep exec fail fault=%u\n", static_cast<u32>(st.fault));
        return 1;
    }
    const auto s = last_tile_stats();
    const char* names[] = {"W1_sprite_grid", "W3_alpha_storm", "W2_high_overdraw",
                           "W4_blit_mix", "W5_edge_scatter", "W6_mixed_scene"};
    std::printf(
        "workload=%s kind=%d tile=%u tiles=%u active=%u refs=%u maxrefs=%u load_px=%u "
        "store_px=%u load_b=%u store_b=%u blend=%llu written=%llu key=%llu samples=%llu "
        "pal=%llu bil=%llu max_od=%u avg_od=%.2f\n",
        names[kind % 6], kind, tile, s.tiles_total, s.tiles_active, s.workref_count,
        s.max_workrefs_per_tile, s.tile_load_pixels, s.tile_store_pixels,
        s.tile_load_bytes, s.tile_store_bytes,
        static_cast<unsigned long long>(s.blend_ops),
        static_cast<unsigned long long>(s.pixels_written),
        static_cast<unsigned long long>(s.key_discards),
        static_cast<unsigned long long>(s.texture_samples),
        static_cast<unsigned long long>(s.palette_reads),
        static_cast<unsigned long long>(s.bilinear_samples), s.max_overdraw,
        s.avg_overdraw_touched);
    return 0;
}
