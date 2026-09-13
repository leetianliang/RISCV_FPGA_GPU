#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"
#include "golden/tile_binner.hpp"

#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;
using namespace golden;

#define EXPECT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d\n", __FILE__, __LINE__);                  \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

ExecResult run_tile(GoldenGPU& g, const std::vector<GpuCmd64>& draws, u32 tw, u32 th,
                    u32 tile, u32 stride, PixelFormat fmt, u32 desc_b, u32 hdr_b,
                    u32 work_b) {
    const auto bin = bin_draws(draws, g.memory(), tw, th, tile);
    g.register_resource(RegisteredResource{
        desc_b, static_cast<u32>(draws.size() * 64), 1, 1, "d"});
    g.register_resource(RegisteredResource{
        hdr_b, static_cast<u32>(bin.headers.size() * 16), 1, 1, "h"});
    g.register_resource(RegisteredResource{
        work_b, static_cast<u32>(bin.workrefs.size() * 4 + 4), 1, 1, "w"});
    for (size_t i = 0; i < draws.size(); ++i) {
        const auto b = serialize_cmd_le(draws[i]);
        g.memory().write_block(desc_b + static_cast<u32>(i) * 64, b.data(), 64);
    }
    for (size_t i = 0; i < bin.headers.size(); ++i) {
        const auto hb = serialize_tile_header(bin.headers[i]);
        g.memory().write_block(hdr_b + static_cast<u32>(i) * 16, hb.data(), 16);
    }
    const auto wr = serialize_workrefs(bin.workrefs);
    if (!wr.empty()) {
        g.memory().write_block(work_b, wr.data(), wr.size());
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
    tf.rt_state = tile_rt_state_store(fmt);
    return execute_tile_frame(g, make_tile_frame_cmd(tf));
}

void run_eq(u32 seed, PixelFormat dst_fmt, auto&& make_draws, const char* name) {
    const u32 tw = 32, th = 32, tile = 16;
    const u32 bpp = bytes_per_pixel(dst_fmt);
    const u32 stride = tw * bpp;
    GoldenGPU imm;
    imm.register_surface(SurfaceDesc{0x10000, stride, tw, th, dst_fmt}, "d");
    // shared texture at 0x20000
    imm.register_surface(SurfaceDesc{0x20000, 32, 8, 8, PixelFormat::RGB565}, "s");
    std::vector<u8> init(static_cast<size_t>(stride) * th, 0);
    std::vector<u8> tex(32 * 8);
    for (size_t i = 0; i + 1 < tex.size(); i += 2) {
        const u16 px = static_cast<u16>((i * 11 + seed) & 0xFFFF);
        tex[i] = static_cast<u8>(px & 0xFF);
        tex[i + 1] = static_cast<u8>(px >> 8);
    }
    imm.memory().write_block(0x10000, init.data(), init.size());
    imm.memory().write_block(0x20000, tex.data(), tex.size());
    // shared extension block for BLIT_EXT
    imm.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    {
        BlitCmdDesc ed;
        ed.clip_xmin = 12;
        ed.clip_ymin = 12;
        ed.clip_xmax = 20;
        ed.clip_ymax = 20;
        auto ext = make_draw2d_ext_v1(ed);
        imm.memory().write_block(0x30000, ext.data(), 64);
    }
    const auto draws = make_draws(stride, seed);
    for (const auto& c : draws) {
        const auto st = imm.execute_command(c);
        if (!st.ok) {
            std::printf("imm fail %s seed=%u fault=%u\n", name, seed,
                        static_cast<u32>(st.fault));
            ++g_failures;
            return;
        }
    }
    std::vector<u8> fb_imm;
    imm.memory().read_block(0x10000, init.size(), fb_imm);

    GoldenGPU tileg;
    tileg.register_surface(SurfaceDesc{0x10000, stride, tw, th, dst_fmt}, "d");
    tileg.register_surface(SurfaceDesc{0x20000, 32, 8, 8, PixelFormat::RGB565}, "s");
    tileg.memory().write_block(0x10000, init.data(), init.size());
    tileg.memory().write_block(0x20000, tex.data(), tex.size());
    tileg.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
    {
        BlitCmdDesc ed;
        ed.clip_xmin = 12;
        ed.clip_ymin = 12;
        ed.clip_xmax = 20;
        ed.clip_ymax = 20;
        auto ext = make_draw2d_ext_v1(ed);
        tileg.memory().write_block(0x30000, ext.data(), 64);
    }
    const auto st = run_tile(tileg, draws, tw, th, tile, stride, dst_fmt, 0x31000,
                             0x34000, 0x36000);
    if (!st.ok) {
        std::printf("tile fail %s seed=%u fault=%u\n", name, seed,
                    static_cast<u32>(st.fault));
        ++g_failures;
        return;
    }
    std::vector<u8> fb_tile;
    tileg.memory().read_block(0x10000, init.size(), fb_tile);
    if (fb_imm != fb_tile) {
        std::printf("eq mismatch %s seed=%u\n", name, seed);
        ++g_failures;
    }
}

}  // namespace

int main() {
    // ARGB dest + BLIT RGB565 source
    run_eq(1, PixelFormat::ARGB8888, [](u32 stride, u32 seed) {
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 32;
        d.dst_stride = stride;
        d.src_format = static_cast<u32>(PixelFormat::RGB565);
        d.dst_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.w = 8;
        d.h = 8;
        d.dst_x = 8;
        d.dst_y = 8;
        return std::vector<GpuCmd64>{make_blit_cmd(d)};
    }, "argb_dst_blit");

    // Premult ARGB source → RGB565 dest
    run_eq(2, PixelFormat::RGB565, [](u32 stride, u32) {
        GoldenGPU tmp;
        // write premult ARGB texture
        tmp.register_surface(SurfaceDesc{0x20000, 32, 8, 8, PixelFormat::ARGB8888},
                             "s");
        SurfaceView sv(&tmp.memory(),
                       RegisteredResource{0x20000, 32 * 8, 8, 8, "s"}, 32,
                       PixelFormat::ARGB8888);
        for (u32 y = 0; y < 8; ++y) {
            for (u32 x = 0; x < 8; ++x) {
                sv.write_rgba(static_cast<i32>(x), static_cast<i32>(y),
                              Rgba8888::pack(128, 200, 10, 20), false, x, y);
            }
        }
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 32;
        d.dst_stride = stride;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.dst_format = static_cast<u32>(PixelFormat::RGB565);
        d.w = 8;
        d.h = 8;
        d.dst_x = 4;
        d.dst_y = 4;
        d.blend = static_cast<u32>(BlendMode::PREMULT_ALPHA);
        d.premult = true;
        d.pixel_alpha_en = true;
        return std::vector<GpuCmd64>{make_blit_cmd(d)};
    }, "premult_argb_src");

    // Clip across tile boundary
    run_eq(3, PixelFormat::RGB565, [](u32 stride, u32) {
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 32;
        d.dst_stride = stride;
        d.w = 8;
        d.h = 8;
        d.dst_x = 8;
        d.dst_y = 8;
        d.blit_ext = true;
        d.ext_ptr = 0x30000;
        d.clip_en = true;
        d.clip_xmin = 12;
        d.clip_ymin = 12;
        d.clip_xmax = 20;
        d.clip_ymax = 20;
        d.u0 = 0;
        d.du_dx = 1 << 16;
        d.dv_dy = 1 << 16;
        return std::vector<GpuCmd64>{make_blit_ext_cmd(d)};
    }, "clip_cross_tile");

    // RGB565 dither fill crossing tile
    run_eq(4, PixelFormat::RGB565, [](u32 stride, u32) {
        auto c = make_fill_rect_cmd(0x10000, stride, 14, 14, 8, 8,
                                    Rgba8888::pack(255, 100, 200, 50));
        u32 ds = c[12];
        ds |= (1u << 27);
        c[12] = ds;
        return std::vector<GpuCmd64>{c};
    }, "dither_cross_tile");

    if (g_failures) {
        std::printf("golden_test_tile_extended FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_tile_extended PASS\n");
    return 0;
}
