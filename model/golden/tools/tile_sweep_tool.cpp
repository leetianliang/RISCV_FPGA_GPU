#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/tile_binner.hpp"

#include <cstdio>
#include <vector>

using namespace golden;

static std::vector<GpuCmd64> make_workload(int kind, u32 tw, u32 th, u32 stride) {
    std::vector<GpuCmd64> d;
    const int n = kind == 2 ? 128 : (kind == 1 ? 64 : 80);
    for (int i = 0; i < n; ++i) {
        const u32 x = static_cast<u32>(i * 3) % tw;
        const u32 y = static_cast<u32>(i * 5) % th;
        u32 w = 8, h = 8;
        if (x + w > tw) w = tw - x;
        if (y + h > th) h = th - y;
        auto c = make_fill_rect_cmd(0x10000, stride, static_cast<i32>(x),
                                    static_cast<i32>(y), w ? w : 1, h ? h : 1,
                                    Rgba8888::pack(255, static_cast<u8>(i),
                                                   static_cast<u8>(i * 2),
                                                   static_cast<u8>(i * 3)));
        if (kind == 1) {
            u32 ds = c[12];
            ds = (ds & ~(0xFu << 8)) |
                 (static_cast<u32>(BlendMode::STRAIGHT_ALPHA) << 8);
            ds |= (1u << 20);
            c[12] = ds;
            c[14] = (128u << 24);
        }
        d.push_back(c);
    }
    return d;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: golden_tile_sweep <workload 0|1|2> <tile 16|32|64>\n");
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
    const auto draws = make_workload(kind, tw, th, stride);
    const auto bin = bin_draws(draws, gpu.memory(), tw, th, tile);
    const u32 desc_b = 0x30000, hdr_b = 0x40000, work_b = 0x50000;
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
    std::printf("kind=%d tile=%u tiles=%u active=%u refs=%u maxrefs=%u load_px=%u store_px=%u load_b=%u store_b=%u\n",
                kind, tile, s.tiles_total, s.tiles_active, s.workref_count,
                s.max_workrefs_per_tile, s.tile_load_pixels, s.tile_store_pixels,
                s.tile_load_bytes, s.tile_store_bytes);
    return 0;
}
