#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_isa.hpp"
#include "golden/gpu_math.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace golden;

namespace {

int usage() {
    std::fprintf(stderr,
                 "Usage:\n"
                 "  golden_cli generate-fill-basic <dir>\n"
                 "  golden_cli generate-stage002-fixtures <frames>\n"
                 "  golden_cli generate-stage003-fixtures <frames>\n"
                 "  golden_cli run-stream --cmd C.bin --initial-fb I.raw [resources] "
                 "--width W --height H --stride S --base B --out O.raw\n");
    return 2;
}

bool write_file(const fs::path& path, const void* data, std::size_t size) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

bool read_file(const fs::path& path, std::vector<u8>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

std::string arg_value(int argc, char** argv, const std::string& key) {
    for (int i = 2; i + 1 < argc; ++i) {
        if (key == argv[i]) {
            return argv[i + 1];
        }
    }
    return {};
}

std::optional<PixelFormat> parse_format(const std::string& s) {
    if (s == "rgb565") return PixelFormat::RGB565;
    if (s == "argb8888") return PixelFormat::ARGB8888;
    if (s == "xrgb8888") return PixelFormat::XRGB8888;
    if (s == "index8") return PixelFormat::INDEX8;
    return std::nullopt;
}

bool run_stream_impl(int argc, char** argv) {
    const std::string cmd_path = arg_value(argc, argv, "--cmd");
    const std::string init_path = arg_value(argc, argv, "--initial-fb");
    const std::string out_path = arg_value(argc, argv, "--out");
    if (cmd_path.empty() || init_path.empty() || out_path.empty()) {
        return false;
    }
    const u32 width = static_cast<u32>(std::stoul(arg_value(argc, argv, "--width")));
    const u32 height = static_cast<u32>(std::stoul(arg_value(argc, argv, "--height")));
    const u32 stride = static_cast<u32>(std::stoul(arg_value(argc, argv, "--stride")));
    const u32 base =
        static_cast<u32>(std::stoul(arg_value(argc, argv, "--base"), nullptr, 0));

    std::vector<u8> cmd_bytes;
    if (!read_file(cmd_path, cmd_bytes) || cmd_bytes.empty() || cmd_bytes.size() % 64) {
        std::fprintf(stderr, "command stream must be N*64\n");
        return false;
    }
    std::vector<GpuCmd64> cmds(cmd_bytes.size() / 64);
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        if (!deserialize_cmd_le(cmd_bytes.data() + i * 64, 64, cmds[i])) {
            return false;
        }
    }

    std::vector<u8> initial;
    if (!read_file(init_path, initial) ||
        initial.size() != static_cast<std::size_t>(stride) * height) {
        std::fprintf(stderr, "initial fb size mismatch\n");
        return false;
    }

    GoldenGPU gpu;
    SurfaceDesc dst{base, stride, width, height, PixelFormat::RGB565};
    // allow override dst format
    if (const auto sfmt = arg_value(argc, argv, "--dst-format"); !sfmt.empty()) {
        const auto f = parse_format(sfmt);
        if (!f) {
            std::fprintf(stderr, "unknown --dst-format %s\n", sfmt.c_str());
            return false;
        }
        dst.format = *f;
    }
    if (!gpu.register_surface(dst, "dst").ok) {
        std::fprintf(stderr, "register dst failed\n");
        return false;
    }
    gpu.memory().write_block(base, initial.data(), initial.size());

    if (const auto tex_path = arg_value(argc, argv, "--textures"); !tex_path.empty()) {
        std::vector<u8> tex;
        if (!read_file(tex_path, tex)) {
            return false;
        }
        const auto tbase =
            static_cast<u32>(std::stoul(arg_value(argc, argv, "--texture-base"), nullptr, 0));
        const auto tw = static_cast<u32>(std::stoul(arg_value(argc, argv, "--texture-width")));
        const auto th = static_cast<u32>(std::stoul(arg_value(argc, argv, "--texture-height")));
        const auto ts = static_cast<u32>(std::stoul(arg_value(argc, argv, "--texture-stride")));
        const auto fmt_s = arg_value(argc, argv, "--texture-format");
        const auto tf = parse_format(fmt_s);
        if (!tf) {
            std::fprintf(stderr, "unknown --texture-format '%s'\n", fmt_s.c_str());
            return false;
        }
        RegisteredResource tr{tbase, static_cast<u32>(tex.size()), tw, th, "tex"};
        if (!gpu.register_resource(tr).ok) {
            std::fprintf(stderr, "register texture failed\n");
            return false;
        }
        gpu.memory().write_block(tbase, tex.data(), tex.size());
        (void)ts;
        (void)tf;
    }

    if (const auto pal_path = arg_value(argc, argv, "--palette"); !pal_path.empty()) {
        std::vector<u8> pal;
        if (!read_file(pal_path, pal) || pal.size() != 1024) {
            std::fprintf(stderr, "palette must be 1024 bytes\n");
            return false;
        }
        const auto pbase = static_cast<u32>(
            std::stoul(arg_value(argc, argv, "--palette-base"), nullptr, 0));
        RegisteredResource pr{pbase, 1024, 256, 1, "pal"};
        gpu.register_resource(pr);
        gpu.memory().write_block(pbase, pal.data(), pal.size());
    }

    if (const auto ext_path = arg_value(argc, argv, "--extensions"); !ext_path.empty()) {
        std::vector<u8> ext;
        if (!read_file(ext_path, ext) || ext.size() < 64) {
            std::fprintf(stderr, "extensions.bin must be >=64\n");
            return false;
        }
        const auto ebase = static_cast<u32>(
            std::stoul(arg_value(argc, argv, "--ext-base"), nullptr, 0));
        RegisteredResource er{ebase, static_cast<u32>(ext.size()), 1, 1, "ext"};
        gpu.register_resource(er);
        gpu.memory().write_block(ebase, ext.data(), ext.size());
    }

    const auto st = gpu.execute_stream(cmds);
    if (!st.ok) {
        std::fprintf(stderr, "execute fail idx=%u fault=%u detail=%u\n", st.fault_index,
                     static_cast<u32>(st.fault), st.fault_detail);
        return false;
    }
    std::vector<u8> out;
    gpu.memory().read_block(base, initial.size(), out);
    write_file(out_path, out.data(), out.size());
    std::printf("Wrote %s (%zu cmds)\n", out_path.c_str(), cmds.size());
    return true;
}

std::vector<u8> pattern(u32 stride, u32 height, u32 salt) {
    std::vector<u8> b(static_cast<std::size_t>(stride) * height);
    for (std::size_t i = 0; i < b.size(); ++i) {
        b[i] = static_cast<u8>((i * 17u + 31u + salt) & 0xFFu);
    }
    return b;
}

bool write_manifest(const fs::path& dir, const std::string& body) {
    return write_file(dir / "manifest.json", body.data(), body.size());
}

bool generate_fill_basic(const fs::path& dir) {
    fs::create_directories(dir);
    const u32 base = 0x10000, stride = 32, w = 16, h = 16;
    auto initial = pattern(stride, h, 0);
    const auto cmd =
        make_fill_rect_cmd(base, stride, 3, 4, 7, 6, Rgba8888::from_u32(0xFF2A55C8u));
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{base, stride, w, h, PixelFormat::RGB565}, "fb");
    gpu.memory().write_block(base, initial.data(), initial.size());
    if (!gpu.execute_command(cmd).ok) {
        return false;
    }
    std::vector<u8> gold;
    gpu.memory().read_block(base, initial.size(), gold);
    const auto bytes = serialize_cmd_le(cmd);
    write_file(dir / "command.bin", bytes.data(), bytes.size());
    write_file(dir / "initial_fb.raw", initial.data(), initial.size());
    write_file(dir / "golden_fb.raw", gold.data(), gold.size());
    write_manifest(dir,
                   "{\n  \"format_version\": 1,\n  \"isa_version\": 1,\n"
                   "  \"pixel_arith_version\": 1,\n  \"width\": 16,\n  \"height\": 16,\n"
                   "  \"stride\": 32,\n  \"framebuffer_format\": \"RGB565\",\n"
                   "  \"command_count\": 1,\n  \"base\": 65536,\n"
                   "  \"description\": \"Stage-001 FILL_RECT interior rectangle 3,4 7x6\"\n}\n");
    return true;
}

struct TexBuild {
    std::vector<u8> bytes;
    u32 base = 0x20000;
    u32 stride = 16;
    u32 w = 8;
    u32 h = 8;
    PixelFormat fmt = PixelFormat::RGB565;
};

TexBuild make_rgb565_tex(u8 r, u8 g, u8 b) {
    TexBuild t;
    t.bytes.assign(t.stride * t.h, 0);
    GoldenGPU g0;
    RegisteredResource res{t.base, static_cast<u32>(t.bytes.size()), t.w, t.h, "t"};
    g0.register_resource(res);
    SurfaceView sv(&g0.memory(), res, t.stride, PixelFormat::RGB565);
    for (u32 y = 0; y < t.h; ++y) {
        for (u32 x = 0; x < t.w; ++x) {
            sv.write_rgba(static_cast<i32>(x), static_cast<i32>(y),
                          Rgba8888::pack(255, r, g, b), false, x, y);
        }
    }
    g0.memory().read_block(t.base, t.bytes.size(), t.bytes);
    return t;
}

bool save_fixture(const fs::path& dir, const std::vector<GpuCmd64>& cmds,
                  const std::vector<u8>& initial, const std::vector<u8>& tex,
                  const std::vector<u8>& ext, const std::vector<u8>& pal,
                  const TexBuild* tb, const std::string& desc,
                  PixelFormat dstf = PixelFormat::RGB565,
                  const std::string& extra_manifest = "") {
    fs::create_directories(dir);
    const u32 base = 0x10000, stride = 32, w = 16, h = 16;
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{base, stride, w, h, dstf}, "dst");
    gpu.memory().write_block(base, initial.data(), initial.size());
    if (tb && !tex.empty()) {
        RegisteredResource tr{tb->base, static_cast<u32>(tex.size()), tb->w, tb->h, "tex"};
        gpu.register_resource(tr);
        gpu.memory().write_block(tb->base, tex.data(), tex.size());
        write_file(dir / "textures.bin", tex.data(), tex.size());
        char meta[64];
        std::snprintf(meta, sizeof(meta), "%u %s", tb->stride,
                      tb->fmt == PixelFormat::INDEX8
                          ? "index8"
                          : (tb->fmt == PixelFormat::ARGB8888 ? "argb8888" : "rgb565"));
        write_file(dir / "texture_meta.txt", meta, std::strlen(meta));
        if (tb->fmt == PixelFormat::INDEX8) {
            write_file(dir / "texture_index8.raw", tex.data(), tex.size());
        }
    }
    if (!ext.empty()) {
        RegisteredResource er{0x30000, static_cast<u32>(ext.size()), 1, 1, "ext"};
        gpu.register_resource(er);
        gpu.memory().write_block(0x30000, ext.data(), ext.size());
        write_file(dir / "extensions.bin", ext.data(), ext.size());
    }
    if (!pal.empty()) {
        RegisteredResource pr{0x40000, 1024, 256, 1, "pal"};
        gpu.register_resource(pr);
        gpu.memory().write_block(0x40000, pal.data(), pal.size());
        write_file(dir / "palette.bin", pal.data(), pal.size());
    }
    const auto st = gpu.execute_stream(cmds);
    if (!st.ok) {
        std::fprintf(stderr, "generate exec fail %s fault=%u\n", dir.filename().string().c_str(),
                     static_cast<u32>(st.fault));
        return false;
    }
    std::vector<u8> gold;
    gpu.memory().read_block(base, initial.size(), gold);
    std::vector<u8> cb;
    for (const auto& c : cmds) {
        const auto b = serialize_cmd_le(c);
        cb.insert(cb.end(), b.begin(), b.end());
    }
    write_file(dir / "command.bin", cb.data(), cb.size());
    write_file(dir / "initial_fb.raw", initial.data(), initial.size());
    write_file(dir / "golden_fb.raw", gold.data(), gold.size());
    const char* dfmt = dstf == PixelFormat::ARGB8888
                           ? "ARGB8888"
                           : (dstf == PixelFormat::XRGB8888 ? "XRGB8888" : "RGB565");
    char man[1024];
    std::snprintf(man, sizeof(man),
                  "{\n  \"format_version\": 1,\n  \"isa_version\": 1,\n"
                  "  \"pixel_arith_version\": 1,\n  \"width\": 16,\n  \"height\": 16,\n"
                  "  \"stride\": 32,\n  \"framebuffer_format\": \"%s\",\n"
                  "  \"command_count\": %zu,\n  \"base\": 65536,\n"
                  "  \"description\": \"%s\"%s\n}\n",
                  dfmt, cmds.size(), desc.c_str(), extra_manifest.c_str());
    write_manifest(dir, man);
    std::printf("Generated %s\n", dir.filename().string().c_str());
    return true;
}

bool generate_stage002_fixtures(const fs::path& root) {
    const u32 base = 0x10000, stride = 32, w = 16, h = 16;
    // blit_rgb565_basic
    {
        auto init = pattern(stride, h, 1);
        auto tex = make_rgb565_tex(255, 0, 0);
        BlitCmdDesc d;
        d.src_base = tex.base;
        d.dst_base = base;
        d.src_stride = tex.stride;
        d.dst_stride = stride;
        d.src_x = 1;
        d.src_y = 1;
        d.dst_x = 2;
        d.dst_y = 3;
        d.w = 5;
        d.h = 5;
        if (!save_fixture(root / "blit_rgb565_basic", {make_blit_cmd(d)}, init, tex.bytes, {}, {},
                          &tex, "blit_rgb565_basic")) {
            return false;
        }
    }
    // colorkey
    {
        auto init = pattern(stride, h, 2);
        auto tex = make_rgb565_tex(0, 255, 0);
        GoldenGPU g0;
        RegisteredResource res{tex.base, static_cast<u32>(tex.bytes.size()), tex.w, tex.h, "t"};
        g0.register_resource(res);
        SurfaceView sv(&g0.memory(), res, tex.stride, PixelFormat::RGB565);
        sv.write_rgba(2, 2, Rgba8888::pack(255, 255, 0, 0), false, 2, 2);
        sv.write_rgba(3, 3, Rgba8888::pack(255, 0, 0, 255), false, 3, 3);
        g0.memory().read_block(tex.base, tex.bytes.size(), tex.bytes);
        BlitCmdDesc d;
        d.src_base = tex.base;
        d.dst_base = base;
        d.src_stride = tex.stride;
        d.dst_stride = stride;
        d.w = 8;
        d.h = 8;
        d.color_key_en = true;
        d.color_key_rgb = 0x00FF00;
        if (!save_fixture(root / "blit_colorkey", {make_blit_cmd(d)}, init, tex.bytes, {}, {},
                          &tex, "blit_colorkey")) {
            return false;
        }
    }
    // alpha argb
    {
        auto init = pattern(stride, h, 3);
        TexBuild tex;
        tex.stride = 32;
        tex.fmt = PixelFormat::ARGB8888;
        tex.bytes.assign(tex.stride * tex.h, 0);
        GoldenGPU g0;
        RegisteredResource res{tex.base, static_cast<u32>(tex.bytes.size()), tex.w, tex.h, "t"};
        g0.register_resource(res);
        SurfaceView sv(&g0.memory(), res, tex.stride, PixelFormat::ARGB8888);
        for (u32 y = 0; y < tex.h; ++y) {
            for (u32 x = 0; x < tex.w; ++x) {
                const u8 a = static_cast<u8>((x * 32u + y * 16u) & 0xFFu);
                sv.write_rgba(static_cast<i32>(x), static_cast<i32>(y),
                              Rgba8888::pack(a, 255, 255, 0), false, x, y);
            }
        }
        g0.memory().read_block(tex.base, tex.bytes.size(), tex.bytes);
        BlitCmdDesc d;
        d.src_base = tex.base;
        d.dst_base = base;
        d.src_stride = tex.stride;
        d.dst_stride = stride;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.w = 8;
        d.h = 8;
        d.blend = static_cast<u32>(BlendMode::STRAIGHT_ALPHA);
        d.pixel_alpha_en = true;
        if (!save_fixture(root / "blit_alpha_argb8888", {make_blit_cmd(d)}, init, tex.bytes,
                          {}, {}, &tex, "blit_alpha_argb8888")) {
            return false;
        }
    }
    // overdraw
    {
        auto init = pattern(stride, h, 4);
        auto tex = make_rgb565_tex(255, 0, 0);
        std::vector<GpuCmd64> cmds;
        for (int i = 0; i < 3; ++i) {
            BlitCmdDesc d;
            d.src_base = tex.base;
            d.dst_base = base;
            d.src_stride = tex.stride;
            d.dst_stride = stride;
            d.w = 4;
            d.h = 4;
            d.blend = static_cast<u32>(BlendMode::STRAIGHT_ALPHA);
            d.global_alpha_en = true;
            d.global_alpha = 128;
            cmds.push_back(make_blit_cmd(d));
        }
        if (!save_fixture(root / "blit_overdraw_rgb565", cmds, init, tex.bytes, {}, {}, &tex,
                          "blit_overdraw_rgb565")) {
            return false;
        }
    }
    // additive
    {
        auto init = pattern(stride, h, 5);
        auto tex = make_rgb565_tex(80, 40, 20);
        BlitCmdDesc d;
        d.src_base = tex.base;
        d.dst_base = base;
        d.src_stride = tex.stride;
        d.dst_stride = stride;
        d.w = 8;
        d.h = 8;
        d.blend = static_cast<u32>(BlendMode::ADD_SAT);
        if (!save_fixture(root / "blit_additive", {make_blit_cmd(d)}, init, tex.bytes, {}, {},
                          &tex, "blit_additive")) {
            return false;
        }
    }
    return true;
}

bool generate_stage003_fixtures(const fs::path& root) {
    const u32 base = 0x10000, stride = 32, w = 16, h = 16;
    auto tex = make_rgb565_tex(255, 0, 0);
    // paint gradient-ish texels
    {
        GoldenGPU g0;
        RegisteredResource res{tex.base, static_cast<u32>(tex.bytes.size()), tex.w, tex.h, "t"};
        g0.register_resource(res);
        SurfaceView sv(&g0.memory(), res, tex.stride, PixelFormat::RGB565);
        for (u32 y = 0; y < tex.h; ++y) {
            for (u32 x = 0; x < tex.w; ++x) {
                sv.write_rgba(static_cast<i32>(x), static_cast<i32>(y),
                              Rgba8888::pack(255, static_cast<u8>(x * 30),
                                             static_cast<u8>(y * 30), 64),
                              false, x, y);
            }
        }
        g0.memory().read_block(tex.base, tex.bytes.size(), tex.bytes);
    }

    auto make_ext = [&](BlitCmdDesc& d) {
        d.ext_ptr = 0x30000;
        d.clip_en = true;
        auto e = make_draw2d_ext_v1(d);
        return std::vector<u8>(e.begin(), e.end());
    };

    // scale nearest
    {
        auto init = pattern(stride, h, 10);
        BlitCmdDesc d;
        d.src_base = tex.base;
        d.dst_base = base;
        d.src_stride = tex.stride;
        d.dst_stride = stride;
        d.w = 8;
        d.h = 8;
        d.blit_ext = true;
        d.filter = static_cast<u32>(FilterMode::NEAREST);
        compute_axis_aligned_uv(0, 8, 16, d.u0, d.du_dx);
        compute_axis_aligned_uv_v(0, 8, 16, d.v0, d.dv_dy);
        // DST size 16x16; SRC_WH patched after build
        d.w = 16;
        d.h = 16;
        // src rect still 8x8 in SRC_WH
        // rebuild cmd with SRC_WH=8x8 DST_WH=16x16
        auto cmd = make_blit_ext_cmd(d);
        cmd[10] = pack_wh(8, 8);
        cmd[11] = pack_wh(16, 16);
        // fix UV for 8->16
        compute_axis_aligned_uv(0, 8, 16, d.u0, d.du_dx);
        compute_axis_aligned_uv_v(0, 8, 16, d.v0, d.dv_dy);
        auto ext = make_draw2d_ext_v1(d);
        // manually set ext UV
        auto putw = [&](std::vector<u8>& b, int i, u32 wv) {
            b[i * 4 + 0] = static_cast<u8>(wv & 0xFF);
            b[i * 4 + 1] = static_cast<u8>((wv >> 8) & 0xFF);
            b[i * 4 + 2] = static_cast<u8>((wv >> 16) & 0xFF);
            b[i * 4 + 3] = static_cast<u8>((wv >> 24) & 0xFF);
        };
        std::vector<u8> extv(ext.begin(), ext.end());
        putw(extv, 3, static_cast<u32>(d.u0));
        putw(extv, 4, static_cast<u32>(d.v0));
        putw(extv, 5, static_cast<u32>(d.du_dx));
        putw(extv, 8, static_cast<u32>(d.dv_dy));
        if (!save_fixture(root / "blit_ext_scale_nearest", {cmd}, init, tex.bytes, extv, {},
                          &tex, "blit_ext_scale_nearest",
                          PixelFormat::RGB565,
                          ",\n  \"ext_base\": 196608")) {
            return false;
        }
    }
    // bilinear scale
    {
        auto init = pattern(stride, h, 11);
        BlitCmdDesc d;
        d.src_base = tex.base;
        d.dst_base = base;
        d.src_stride = tex.stride;
        d.dst_stride = stride;
        d.blit_ext = true;
        d.filter = static_cast<u32>(FilterMode::BILINEAR);
        d.w = 16;
        d.h = 16;
        compute_axis_aligned_uv(0, 8, 16, d.u0, d.du_dx);
        compute_axis_aligned_uv_v(0, 8, 16, d.v0, d.dv_dy);
        auto cmd = make_blit_ext_cmd(d);
        cmd[10] = pack_wh(8, 8);
        cmd[11] = pack_wh(16, 16);
        auto ext = make_draw2d_ext_v1(d);
        std::vector<u8> extv(ext.begin(), ext.end());
        auto putw = [&](std::vector<u8>& b, int i, u32 wv) {
            b[i * 4 + 0] = static_cast<u8>(wv & 0xFF);
            b[i * 4 + 1] = static_cast<u8>((wv >> 8) & 0xFF);
            b[i * 4 + 2] = static_cast<u8>((wv >> 16) & 0xFF);
            b[i * 4 + 3] = static_cast<u8>((wv >> 24) & 0xFF);
        };
        putw(extv, 3, static_cast<u32>(d.u0));
        putw(extv, 4, static_cast<u32>(d.v0));
        putw(extv, 5, static_cast<u32>(d.du_dx));
        putw(extv, 8, static_cast<u32>(d.dv_dy));
        if (!save_fixture(root / "blit_ext_scale_bilinear", {cmd}, init, tex.bytes, extv, {},
                          &tex, "blit_ext_scale_bilinear", PixelFormat::RGB565,
                          ",\n  \"ext_base\": 196608")) {
            return false;
        }
    }
    // flip xy 1:1
    {
        auto init = pattern(stride, h, 12);
        BlitCmdDesc d;
        d.src_base = tex.base;
        d.dst_base = base;
        d.src_stride = tex.stride;
        d.dst_stride = stride;
        d.w = 8;
        d.h = 8;
        d.flip_x = true;
        d.flip_y = true;
        if (!save_fixture(root / "blit_flip_xy", {make_blit_cmd(d)}, init, tex.bytes, {}, {},
                          &tex, "blit_flip_xy")) {
            return false;
        }
    }
    // clip
    {
        auto init = pattern(stride, h, 13);
        BlitCmdDesc d;
        d.src_base = tex.base;
        d.dst_base = base;
        d.src_stride = tex.stride;
        d.dst_stride = stride;
        d.w = 8;
        d.h = 8;
        d.blit_ext = true;
        d.clip_en = true;
        d.clip_xmin = 2;
        d.clip_ymin = 2;
        d.clip_xmax = 6;
        d.clip_ymax = 6;
        auto cmd = make_blit_ext_cmd(d);
        auto ext = make_draw2d_ext_v1(d);
        std::vector<u8> extv(ext.begin(), ext.end());
        if (!save_fixture(root / "blit_clip", {cmd}, init, tex.bytes, extv, {}, &tex,
                          "blit_clip", PixelFormat::RGB565, ",\n  \"ext_base\": 196608")) {
            return false;
        }
    }
    // indexed8 + bilinear
    {
        auto init = pattern(stride, h, 14);
        TexBuild itex;
        itex.stride = 8;
        itex.fmt = PixelFormat::INDEX8;
        itex.bytes.resize(64);
        for (u32 i = 0; i < 64; ++i) {
            itex.bytes[i] = static_cast<u8>(i & 0xFFu);
        }
        std::vector<u8> pal(1024);
        for (u32 i = 0; i < 256; ++i) {
            const u32 c = 0xFF000000u | (i * 0x010203u);
            pal[i * 4 + 0] = static_cast<u8>(c & 0xFF);
            pal[i * 4 + 1] = static_cast<u8>((c >> 8) & 0xFF);
            pal[i * 4 + 2] = static_cast<u8>((c >> 16) & 0xFF);
            pal[i * 4 + 3] = static_cast<u8>((c >> 24) & 0xFF);
        }
        BlitCmdDesc d;
        d.src_base = itex.base;
        d.dst_base = base;
        d.src_stride = itex.stride;
        d.dst_stride = stride;
        d.src_format = static_cast<u32>(PixelFormat::INDEX8);
        d.palette_en = true;
        d.palette_addr = 0x40000;
        d.w = 8;
        d.h = 8;
        if (!save_fixture(root / "indexed8_palette", {make_blit_cmd(d)}, init, itex.bytes, {},
                          pal, &itex, "indexed8_palette")) {
            return false;
        }
        BlitCmdDesc db = d;
        db.blit_ext = true;
        db.filter = static_cast<u32>(FilterMode::BILINEAR);
        db.w = 16;
        db.h = 16;
        compute_axis_aligned_uv(0, 8, 16, db.u0, db.du_dx);
        compute_axis_aligned_uv_v(0, 8, 16, db.v0, db.dv_dy);
        auto cmd = make_blit_ext_cmd(db);
        cmd[10] = pack_wh(8, 8);
        cmd[11] = pack_wh(16, 16);
        auto ext = make_draw2d_ext_v1(db);
        std::vector<u8> extv(ext.begin(), ext.end());
        auto putw = [&](std::vector<u8>& b, int i, u32 wv) {
            b[i * 4 + 0] = static_cast<u8>(wv & 0xFF);
            b[i * 4 + 1] = static_cast<u8>((wv >> 8) & 0xFF);
            b[i * 4 + 2] = static_cast<u8>((wv >> 16) & 0xFF);
            b[i * 4 + 3] = static_cast<u8>((wv >> 24) & 0xFF);
        };
        putw(extv, 3, static_cast<u32>(db.u0));
        putw(extv, 4, static_cast<u32>(db.v0));
        putw(extv, 5, static_cast<u32>(db.du_dx));
        putw(extv, 8, static_cast<u32>(db.dv_dy));
        if (!save_fixture(root / "indexed8_bilinear", {cmd}, init, itex.bytes, extv, pal,
                          &itex, "indexed8_bilinear", PixelFormat::RGB565,
                          ",\n  \"ext_base\": 196608")) {
            return false;
        }
    }
    // color_mod
    {
        auto init = pattern(stride, h, 15);
        BlitCmdDesc d;
        d.src_base = tex.base;
        d.dst_base = base;
        d.src_stride = tex.stride;
        d.dst_stride = stride;
        d.w = 8;
        d.h = 8;
        d.color_mod_en = true;
        d.primary_color = Rgba8888::pack(128, 200, 100, 50);
        if (!save_fixture(root / "color_mod", {make_blit_cmd(d)}, init, tex.bytes, {}, {},
                          &tex, "color_mod")) {
            return false;
        }
    }
    // premult
    {
        auto init = pattern(stride, h, 16);
        TexBuild at;
        at.stride = 32;
        at.fmt = PixelFormat::ARGB8888;
        at.bytes.assign(at.stride * at.h, 0);
        GoldenGPU g0;
        RegisteredResource res{at.base, static_cast<u32>(at.bytes.size()), at.w, at.h, "t"};
        g0.register_resource(res);
        SurfaceView sv(&g0.memory(), res, at.stride, PixelFormat::ARGB8888);
        for (u32 y = 0; y < at.h; ++y) {
            for (u32 x = 0; x < at.w; ++x) {
                const u8 a = 128;
                const u8 r = mul8_rn(255, a);
                const u8 g = mul8_rn(0, a);
                const u8 b = mul8_rn(0, a);
                sv.write_rgba(static_cast<i32>(x), static_cast<i32>(y),
                              Rgba8888::pack(a, r, g, b), false, x, y);
            }
        }
        g0.memory().read_block(at.base, at.bytes.size(), at.bytes);
        BlitCmdDesc d;
        d.src_base = at.base;
        d.dst_base = base;
        d.src_stride = at.stride;
        d.dst_stride = stride;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.w = 8;
        d.h = 8;
        d.blend = static_cast<u32>(BlendMode::PREMULT_ALPHA);
        d.premult = true;
        d.pixel_alpha_en = true;
        if (!save_fixture(root / "premult_alpha", {make_blit_cmd(d)}, init, at.bytes, {}, {},
                          &at, "premult_alpha")) {
            return false;
        }
    }
    // dither fill
    {
        auto init = pattern(stride, h, 17);
        auto cmd = make_fill_rect_cmd(base, stride, 0, 0, 8, 8,
                                      Rgba8888::pack(255, 100, 200, 50));
        cmd[12] |= (1u << 27);  // DITHER_EN
        if (!save_fixture(root / "rgb565_dither", {cmd}, init, {}, {}, {}, nullptr,
                          "rgb565_dither")) {
            return false;
        }
    }
    // argb target fill — ARGB needs stride >= width*4 = 64
    {
        const u32 abase = 0x10000, astride = 64, aw = 16, ah = 16;
        std::vector<u8> ainit(static_cast<size_t>(astride) * ah, 0x11);
        for (size_t i = 0; i < ainit.size(); i += 4) {
            ainit[i + 3] = 0xFF;
        }
        auto cmd = make_fill_rect_cmd(abase, astride, 2, 2, 4, 4,
                                      Rgba8888::pack(0xAA, 0x11, 0x22, 0x33));
        cmd[12] = pack_draw_state(static_cast<u32>(PixelFormat::RGB565),
                                  static_cast<u32>(PixelFormat::ARGB8888),
                                  static_cast<u32>(BlendMode::COPY));
        GoldenGPU gpu;
        if (!gpu.register_surface(SurfaceDesc{abase, astride, aw, ah, PixelFormat::ARGB8888},
                                  "a")
                 .ok) {
            std::fprintf(stderr, "argb register fail\n");
            return false;
        }
        gpu.memory().write_block(abase, ainit.data(), ainit.size());
        if (!gpu.execute_command(cmd).ok) {
            std::fprintf(stderr, "argb exec fail\n");
            return false;
        }
        std::vector<u8> gold;
        gpu.memory().read_block(abase, ainit.size(), gold);
        const auto bytes = serialize_cmd_le(cmd);
        const fs::path dir = root / "argb8888_target";
        fs::create_directories(dir);
        write_file(dir / "command.bin", bytes.data(), bytes.size());
        write_file(dir / "initial_fb.raw", ainit.data(), ainit.size());
        write_file(dir / "golden_fb.raw", gold.data(), gold.size());
        write_manifest(dir,
                       "{\n  \"format_version\": 1,\n  \"isa_version\": 1,\n"
                       "  \"pixel_arith_version\": 1,\n  \"width\": 16,\n  \"height\": 16,\n"
                       "  \"stride\": 64,\n  \"framebuffer_format\": \"ARGB8888\",\n"
                       "  \"command_count\": 1,\n  \"base\": 65536,\n"
                       "  \"description\": \"argb8888_target\"\n}\n");
        std::printf("Generated argb8888_target\n");
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return usage();
    }
    const std::string mode = argv[1];
    if (mode == "generate-fill-basic") {
        return generate_fill_basic(argv[2]) ? 0 : 1;
    }
    if (mode == "generate-stage002-fixtures") {
        return generate_stage002_fixtures(argv[2]) ? 0 : 1;
    }
    if (mode == "generate-stage003-fixtures") {
        return generate_stage003_fixtures(argv[2]) ? 0 : 1;
    }
    if (mode == "run-stream" || mode == "run-fill") {
        return run_stream_impl(argc, argv) ? 0 : 1;
    }
    return usage();
}
