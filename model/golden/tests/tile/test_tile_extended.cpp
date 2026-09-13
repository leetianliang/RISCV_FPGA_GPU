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

struct AssetEnv {
    GoldenGPU imm;
    GoldenGPU tileg;
    u32 tw = 32, th = 32, tile = 16, stride = 64;
    PixelFormat dst_fmt = PixelFormat::RGB565;
    std::vector<u8> init;
};

// Install identical fb/texture/palette/ext into both GPUs.
void setup_pair(AssetEnv& e, PixelFormat dst_fmt, u32 tw, u32 th, u32 stride,
                const std::vector<u8>& tex, PixelFormat tex_fmt, u32 tex_stride,
                const std::vector<u8>& pal, const std::vector<u8>& ext) {
    e.dst_fmt = dst_fmt;
    e.tw = tw;
    e.th = th;
    e.stride = stride;
    e.init.assign(static_cast<size_t>(stride) * th, 0);
    for (size_t i = 0; i + 1 < e.init.size(); i += 2) {
        const u16 px = static_cast<u16>((i * 9 + 3) & 0xFFFF);
        e.init[i] = static_cast<u8>(px & 0xFF);
        e.init[i + 1] = static_cast<u8>(px >> 8);
    }
    auto setup = [&](GoldenGPU& g) {
        g.register_surface(SurfaceDesc{0x10000, stride, tw, th, dst_fmt}, "d");
        g.memory().write_block(0x10000, e.init.data(), e.init.size());
        if (!tex.empty()) {
            g.register_surface(SurfaceDesc{0x20000, tex_stride, 8, 8, tex_fmt}, "s");
            g.memory().write_block(0x20000, tex.data(), tex.size());
        }
        // always reserve ext slot so callers can write UV later
        g.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
        if (!ext.empty()) {
            g.memory().write_block(0x30000, ext.data(), ext.size());
        }
        if (!pal.empty()) {
            g.register_resource(RegisteredResource{0x40000, 1024, 256, 1, "p"});
            g.memory().write_block(0x40000, pal.data(), pal.size());
        }
    };
    setup(e.imm);
    setup(e.tileg);
}

bool run_eq(AssetEnv& e, const std::vector<GpuCmd64>& draws, const char* name) {
    for (const auto& c : draws) {
        const auto st = e.imm.execute_command(c);
        if (!st.ok) {
            std::printf("imm fail %s fault=%u\n", name, static_cast<u32>(st.fault));
            ++g_failures;
            return false;
        }
    }
    std::vector<u8> fb_imm;
    e.imm.memory().read_block(0x10000, e.init.size(), fb_imm);

    const auto bin = bin_draws(draws, e.tileg.memory(), e.tw, e.th, e.tile);
    const u32 desc_b = 0x38000, hdr_b = 0x3A000, work_b = 0x3C000;
    e.tileg.register_resource(RegisteredResource{
        desc_b, static_cast<u32>(draws.size() * 64), 1, 1, "d"});
    e.tileg.register_resource(RegisteredResource{
        hdr_b, static_cast<u32>(bin.headers.size() * 16), 1, 1, "h"});
    e.tileg.register_resource(RegisteredResource{
        work_b, static_cast<u32>(bin.workrefs.size() * 4 + 4), 1, 1, "w"});
    for (size_t i = 0; i < draws.size(); ++i) {
        const auto b = serialize_cmd_le(draws[i]);
        e.tileg.memory().write_block(desc_b + static_cast<u32>(i) * 64, b.data(), 64);
    }
    for (size_t i = 0; i < bin.headers.size(); ++i) {
        const auto hb = serialize_tile_header(bin.headers[i]);
        e.tileg.memory().write_block(hdr_b + static_cast<u32>(i) * 16, hb.data(), 16);
    }
    const auto wr = serialize_workrefs(bin.workrefs);
    if (!wr.empty()) {
        e.tileg.memory().write_block(work_b, wr.data(), wr.size());
    }
    TileFrameCmd tf;
    tf.draw_desc_base = desc_b;
    tf.tile_header_base = hdr_b;
    tf.work_list_base = work_b;
    tf.dst_base = 0x10000;
    tf.dst_stride = e.stride;
    tf.surface_w = e.tw;
    tf.surface_h = e.th;
    tf.grid_w = (e.tw + e.tile - 1) / e.tile;
    tf.grid_h = (e.th + e.tile - 1) / e.tile;
    tf.tile_w = e.tile;
    tf.tile_h = e.tile;
    tf.rt_state = tile_rt_state_store(e.dst_fmt);
    const auto st = execute_tile_frame(e.tileg, make_tile_frame_cmd(tf));
    if (!st.ok) {
        std::printf("tile fail %s fault=%u\n", name, static_cast<u32>(st.fault));
        ++g_failures;
        return false;
    }
    std::vector<u8> fb_tile;
    e.tileg.memory().read_block(0x10000, e.init.size(), fb_tile);
    if (fb_imm != fb_tile) {
        std::printf("eq mismatch %s\n", name);
        ++g_failures;
        return false;
    }
    return true;
}

std::vector<u8> rgb565_tex() {
    std::vector<u8> t(16 * 8);
    for (size_t i = 0; i + 1 < t.size(); i += 2) {
        const u16 px = static_cast<u16>((i * 11) & 0xFFFF);
        t[i] = static_cast<u8>(px & 0xFF);
        t[i + 1] = static_cast<u8>(px >> 8);
    }
    return t;
}

std::vector<u8> argb_tex() {
    std::vector<u8> t(32 * 8);
    for (size_t i = 0; i + 4 <= t.size(); i += 4) {
        t[i] = static_cast<u8>(i & 0xFF);
        t[i + 1] = static_cast<u8>((i * 2) & 0xFF);
        t[i + 2] = static_cast<u8>((i * 3) & 0xFF);
        t[i + 3] = static_cast<u8>(64 + (i & 0x7F));
    }
    return t;
}

std::vector<u8> xrgb_tex() {
    return argb_tex();  // same layout as ARGB LE; A forced 255 on decode
}

std::vector<u8> make_pal() {
    std::vector<u8> p(1024);
    for (u32 i = 0; i < 256; ++i) {
        p[i * 4 + 0] = static_cast<u8>(i);
        p[i * 4 + 1] = static_cast<u8>(255 - i);
        p[i * 4 + 2] = static_cast<u8>(i / 2);
        p[i * 4 + 3] = 255;
    }
    return p;
}

}  // namespace

int main() {
    // XRGB8888 source → RGB565 dest
    {
        AssetEnv e;
        setup_pair(e, PixelFormat::RGB565, 32, 32, 64, xrgb_tex(),
                   PixelFormat::XRGB8888, 32, {}, {});
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 32;
        d.dst_stride = 64;
        d.src_format = static_cast<u32>(PixelFormat::XRGB8888);
        d.dst_format = static_cast<u32>(PixelFormat::RGB565);
        d.w = 8;
        d.h = 8;
        d.dst_x = 8;
        d.dst_y = 8;
        run_eq(e, {make_blit_cmd(d)}, "xrgb_src");
    }
    // RGB565 source → XRGB8888 dest
    {
        AssetEnv e;
        setup_pair(e, PixelFormat::XRGB8888, 16, 16, 64, rgb565_tex(),
                   PixelFormat::RGB565, 16, {}, {});
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 16;
        d.dst_stride = 64;
        d.src_format = static_cast<u32>(PixelFormat::RGB565);
        d.dst_format = static_cast<u32>(PixelFormat::XRGB8888);
        d.w = 8;
        d.h = 8;
        d.dst_x = 4;
        d.dst_y = 4;
        run_eq(e, {make_blit_cmd(d)}, "xrgb_dst");
    }
    // Color Mod
    {
        AssetEnv e;
        setup_pair(e, PixelFormat::RGB565, 32, 32, 64, rgb565_tex(),
                   PixelFormat::RGB565, 16, {}, {});
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 16;
        d.dst_stride = 64;
        d.w = 8;
        d.h = 8;
        d.dst_x = 2;
        d.dst_y = 2;
        d.color_mod_en = true;
        d.primary_color = Rgba8888::pack(128, 200, 100, 50);
        run_eq(e, {make_blit_cmd(d)}, "color_mod");
    }
    // Global Alpha
    {
        AssetEnv e;
        setup_pair(e, PixelFormat::RGB565, 32, 32, 64, rgb565_tex(),
                   PixelFormat::RGB565, 16, {}, {});
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 16;
        d.dst_stride = 64;
        d.w = 8;
        d.h = 8;
        d.dst_x = 0;
        d.dst_y = 0;
        d.blend = static_cast<u32>(BlendMode::STRAIGHT_ALPHA);
        d.global_alpha_en = true;
        d.global_alpha = 128;
        run_eq(e, {make_blit_cmd(d)}, "global_alpha");
    }
    // Per-Pixel Alpha (ARGB source)
    {
        AssetEnv e;
        setup_pair(e, PixelFormat::RGB565, 32, 32, 64, argb_tex(),
                   PixelFormat::ARGB8888, 32, {}, {});
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 32;
        d.dst_stride = 64;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.w = 8;
        d.h = 8;
        d.dst_x = 1;
        d.dst_y = 1;
        d.blend = static_cast<u32>(BlendMode::STRAIGHT_ALPHA);
        d.pixel_alpha_en = true;
        run_eq(e, {make_blit_cmd(d)}, "pixel_alpha");
    }
    // Premult — assets installed in actual Imm/Tile GPUs
    {
        AssetEnv e;
        // Premultiplied ARGB: A=128, RGB already scaled
        std::vector<u8> ptex(32 * 8, 0);
        for (size_t i = 0; i + 4 <= ptex.size(); i += 4) {
            ptex[i] = 10;
            ptex[i + 1] = 20;
            ptex[i + 2] = 30;
            ptex[i + 3] = 128;
        }
        setup_pair(e, PixelFormat::RGB565, 32, 32, 64, ptex, PixelFormat::ARGB8888, 32,
                   {}, {});
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 32;
        d.dst_stride = 64;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.w = 8;
        d.h = 8;
        d.dst_x = 8;
        d.dst_y = 8;
        d.blend = static_cast<u32>(BlendMode::PREMULT_ALPHA);
        d.premult = true;
        d.pixel_alpha_en = true;
        run_eq(e, {make_blit_cmd(d)}, "premult_asset");
    }
    // Nearest scaling
    {
        AssetEnv e;
        setup_pair(e, PixelFormat::RGB565, 32, 32, 64, rgb565_tex(),
                   PixelFormat::RGB565, 16, {}, {});
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 16;
        d.dst_stride = 64;
        d.blit_ext = true;
        d.ext_ptr = 0x30000;
        d.w = 8;
        d.h = 8;
        d.dst_x = 8;
        d.dst_y = 8;
        compute_axis_aligned_uv(0, 4, 8, d.u0, d.du_dx);
        compute_axis_aligned_uv_v(0, 4, 8, d.v0, d.dv_dy);
        auto cmd = make_blit_ext_cmd(d);
        cmd[10] = pack_wh(4, 4);
        cmd[11] = pack_wh(8, 8);
        BlitCmdDesc ed;
        auto ext = make_draw2d_ext_v1(ed);
        std::vector<u8> extv(ext.begin(), ext.end());
        // write UV into ext after make
        auto putw = [&](int i, u32 w) {
            extv[i * 4 + 0] = static_cast<u8>(w & 0xFF);
            extv[i * 4 + 1] = static_cast<u8>((w >> 8) & 0xFF);
            extv[i * 4 + 2] = static_cast<u8>((w >> 16) & 0xFF);
            extv[i * 4 + 3] = static_cast<u8>((w >> 24) & 0xFF);
        };
        putw(3, static_cast<u32>(d.u0));
        putw(4, static_cast<u32>(d.v0));
        putw(5, static_cast<u32>(d.du_dx));
        putw(8, static_cast<u32>(d.dv_dy));
        e.imm.memory().write_block(0x30000, extv.data(), 64);
        e.tileg.memory().write_block(0x30000, extv.data(), 64);
        run_eq(e, {cmd}, "nearest_scale");
    }
    // Bilinear scaling
    {
        AssetEnv e;
        setup_pair(e, PixelFormat::RGB565, 32, 32, 64, rgb565_tex(),
                   PixelFormat::RGB565, 16, {}, {});
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 16;
        d.dst_stride = 64;
        d.blit_ext = true;
        d.ext_ptr = 0x30000;
        d.filter = static_cast<u32>(FilterMode::BILINEAR);
        d.w = 6;
        d.h = 6;
        d.dst_x = 4;
        d.dst_y = 4;
        compute_axis_aligned_uv(0, 4, 6, d.u0, d.du_dx);
        compute_axis_aligned_uv_v(0, 4, 6, d.v0, d.dv_dy);
        auto cmd = make_blit_ext_cmd(d);
        cmd[10] = pack_wh(4, 4);
        cmd[11] = pack_wh(6, 6);
        BlitCmdDesc ed;
        auto ext = make_draw2d_ext_v1(ed);
        std::vector<u8> extv(ext.begin(), ext.end());
        auto putw = [&](int i, u32 w) {
            extv[i * 4 + 0] = static_cast<u8>(w & 0xFF);
            extv[i * 4 + 1] = static_cast<u8>((w >> 8) & 0xFF);
            extv[i * 4 + 2] = static_cast<u8>((w >> 16) & 0xFF);
            extv[i * 4 + 3] = static_cast<u8>((w >> 24) & 0xFF);
        };
        putw(3, static_cast<u32>(d.u0));
        putw(4, static_cast<u32>(d.v0));
        putw(5, static_cast<u32>(d.du_dx));
        putw(8, static_cast<u32>(d.dv_dy));
        e.imm.memory().write_block(0x30000, extv.data(), 64);
        e.tileg.memory().write_block(0x30000, extv.data(), 64);
        run_eq(e, {cmd}, "bilinear_scale");
    }
    // Clamp / Repeat via BLIT_EXT UV beyond source
    {
        AssetEnv e;
        setup_pair(e, PixelFormat::RGB565, 32, 32, 64, rgb565_tex(),
                   PixelFormat::RGB565, 16, {}, {});
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 16;
        d.dst_stride = 64;
        d.blit_ext = true;
        d.ext_ptr = 0x30000;
        d.addr_u = static_cast<u32>(AddressMode::REPEAT);
        d.w = 8;
        d.h = 8;
        d.dst_x = 0;
        d.dst_y = 0;
        d.u0 = -65536;  // -1.0 → repeat to last
        d.du_dx = 65536;
        d.dv_dy = 65536;
        auto cmd = make_blit_ext_cmd(d);
        cmd[10] = pack_wh(4, 4);
        cmd[11] = pack_wh(8, 8);
        BlitCmdDesc ed;
        auto ext = make_draw2d_ext_v1(ed);
        std::vector<u8> extv(ext.begin(), ext.end());
        auto putw = [&](int i, u32 w) {
            extv[i * 4 + 0] = static_cast<u8>(w & 0xFF);
            extv[i * 4 + 1] = static_cast<u8>((w >> 8) & 0xFF);
            extv[i * 4 + 2] = static_cast<u8>((w >> 16) & 0xFF);
            extv[i * 4 + 3] = static_cast<u8>((w >> 24) & 0xFF);
        };
        putw(3, static_cast<u32>(d.u0));
        putw(5, static_cast<u32>(d.du_dx));
        putw(8, static_cast<u32>(d.dv_dy));
        e.imm.memory().write_block(0x30000, extv.data(), 64);
        e.tileg.memory().write_block(0x30000, extv.data(), 64);
        run_eq(e, {cmd}, "repeat_addr");
    }
    // Indexed8 + Palette
    {
        AssetEnv e;
        std::vector<u8> i8(64);
        for (size_t i = 0; i < i8.size(); ++i) {
            i8[i] = static_cast<u8>(i & 0xFF);
        }
        // register INDEX8 texture manually after pair
        setup_pair(e, PixelFormat::RGB565, 32, 32, 64, {}, PixelFormat::RGB565, 16,
                   make_pal(), {});
        e.imm.register_resource(RegisteredResource{0x48000, 64, 8, 8, "i8"});
        e.tileg.register_resource(RegisteredResource{0x48000, 64, 8, 8, "i8"});
        e.imm.memory().write_block(0x48000, i8.data(), i8.size());
        e.tileg.memory().write_block(0x48000, i8.data(), i8.size());
        BlitCmdDesc d;
        d.src_base = 0x48000;
        d.dst_base = 0x10000;
        d.src_stride = 8;
        d.dst_stride = 64;
        d.src_format = static_cast<u32>(PixelFormat::INDEX8);
        d.palette_en = true;
        d.palette_addr = 0x40000;
        d.w = 4;
        d.h = 4;
        d.dst_x = 0;
        d.dst_y = 0;
        run_eq(e, {make_blit_cmd(d)}, "indexed8_palette");
    }

    if (g_failures) {
        std::printf("golden_test_tile_extended FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_tile_extended PASS\n");
    return 0;
}
