#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"
#include "golden/gpu_isa.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace golden;

namespace {

int usage() {
    std::fprintf(stderr,
                 "Usage:\n"
                 "  golden_cli generate-fill-basic <outdir>\n"
                 "  golden_cli generate-stage002-fixtures <frames_root>\n"
                 "  golden_cli run-stream --cmd C.bin --initial-fb I.raw\n"
                 "      [--textures T.bin --texture-base HEX --texture-width W\n"
                 "       --texture-height H --texture-stride S --texture-format rgb565|argb8888|xrgb8888]\n"
                 "      --width W --height H --stride S --base B --out O.raw\n");
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

PixelFormat parse_format(const std::string& s) {
    if (s == "argb8888") {
        return PixelFormat::ARGB8888;
    }
    if (s == "xrgb8888") {
        return PixelFormat::XRGB8888;
    }
    if (s == "rgb565") {
        return PixelFormat::RGB565;
    }
    return PixelFormat::RGB565;
}

bool write_manifest(const fs::path& path, const std::string& body) {
    return write_file(path, body.data(), body.size());
}

struct FbConfig {
    u32 base = 0x10000;
    u32 width = 16;
    u32 height = 16;
    u32 stride = 32;
};

std::vector<u8> make_pattern(u32 stride, u32 height, u32 salt) {
    std::vector<u8> buf(static_cast<std::size_t>(stride) * height);
    for (std::size_t i = 0; i < buf.size(); ++i) {
        buf[i] = static_cast<u8>((i * 17u + 31u + salt) & 0xFFu);
    }
    return buf;
}

bool run_stream_impl(int argc, char** argv) {
    const std::string cmd_path = arg_value(argc, argv, "--cmd");
    const std::string init_path = arg_value(argc, argv, "--initial-fb");
    const std::string tex_path = arg_value(argc, argv, "--textures");
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
    if (!read_file(cmd_path, cmd_bytes) || cmd_bytes.empty() ||
        (cmd_bytes.size() % 64) != 0) {
        std::fprintf(stderr, "command stream size must be N*64\n");
        return false;
    }
    std::vector<GpuCmd64> cmds(cmd_bytes.size() / 64);
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        if (!deserialize_cmd_le(cmd_bytes.data() + i * 64, 64, cmds[i])) {
            std::fprintf(stderr, "LE deserialize failed at %zu\n", i);
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
    SurfaceDesc dst;
    dst.base = base;
    dst.stride = stride;
    dst.width = width;
    dst.height = height;
    dst.format = PixelFormat::RGB565;
    if (!gpu.register_surface(dst, "dst").ok) {
        std::fprintf(stderr, "register dst failed\n");
        return false;
    }
    gpu.memory().write_block(base, initial.data(), initial.size());

    if (!tex_path.empty()) {
        std::vector<u8> tex;
        if (!read_file(tex_path, tex)) {
            std::fprintf(stderr, "read textures failed\n");
            return false;
        }
        SurfaceDesc ts;
        ts.base = static_cast<u32>(
            std::stoul(arg_value(argc, argv, "--texture-base"), nullptr, 0));
        ts.width = static_cast<u32>(std::stoul(arg_value(argc, argv, "--texture-width")));
        ts.height =
            static_cast<u32>(std::stoul(arg_value(argc, argv, "--texture-height")));
        ts.stride =
            static_cast<u32>(std::stoul(arg_value(argc, argv, "--texture-stride")));
        ts.format = parse_format(arg_value(argc, argv, "--texture-format"));
        if (!gpu.register_surface(ts, "tex").ok) {
            std::fprintf(stderr, "register texture failed\n");
            return false;
        }
        if (tex.size() != static_cast<std::size_t>(ts.stride) * ts.height) {
            std::fprintf(stderr, "texture size mismatch %zu\n", tex.size());
            return false;
        }
        gpu.memory().write_block(ts.base, tex.data(), tex.size());
    }

    const auto st = gpu.execute_stream(cmds);
    if (!st.ok) {
        std::fprintf(stderr, "execute failed idx=%u fault=%u detail=%u\n", st.fault_index,
                     static_cast<u32>(st.fault), st.fault_detail);
        return false;
    }

    std::vector<u8> out;
    if (gpu.memory().read_block(base, initial.size(), out).status !=
        MemAccessStatus::OK) {
        std::fprintf(stderr, "read fb failed\n");
        return false;
    }
    if (!write_file(out_path, out.data(), out.size())) {
        std::fprintf(stderr, "write out failed\n");
        return false;
    }
    std::printf("Wrote %s (%u cmds)\n", out_path.c_str(),
                static_cast<unsigned>(cmds.size()));
    return true;
}

bool generate_fill_basic(const fs::path& outdir) {
    fs::create_directories(outdir);
    const FbConfig fb{};
    auto initial = make_pattern(fb.stride, fb.height, 0);
    const auto color = Rgba8888::from_u32(0xFF2A55C8u);
    const auto cmd = make_fill_rect_cmd(fb.base, fb.stride, 3, 4, 7, 6, color);

    GoldenGPU gpu;
    SurfaceDesc desc;
    desc.base = fb.base;
    desc.stride = fb.stride;
    desc.width = fb.width;
    desc.height = fb.height;
    desc.format = PixelFormat::RGB565;
    if (!gpu.register_surface(desc, "fill_basic_fb").ok) {
        return false;
    }
    gpu.memory().write_block(fb.base, initial.data(), initial.size());
    if (!gpu.execute_command(cmd).ok) {
        return false;
    }
    std::vector<u8> golden_fb;
    gpu.memory().read_block(fb.base, initial.size(), golden_fb);

    const auto bytes = serialize_cmd_le(cmd);
    if (!write_file(outdir / "command.bin", bytes.data(), bytes.size())) {
        return false;
    }
    write_file(outdir / "initial_fb.raw", initial.data(), initial.size());
    write_file(outdir / "golden_fb.raw", golden_fb.data(), golden_fb.size());
    const std::string manifest =
        "{\n"
        "  \"format_version\": 1,\n"
        "  \"isa_version\": 1,\n"
        "  \"pixel_arith_version\": 1,\n"
        "  \"width\": 16,\n"
        "  \"height\": 16,\n"
        "  \"stride\": 32,\n"
        "  \"framebuffer_format\": \"RGB565\",\n"
        "  \"command_count\": 1,\n"
        "  \"base\": 65536,\n"
        "  \"description\": \"Stage-001 FILL_RECT interior rectangle 3,4 7x6 color 0xFF2A55C8 COPY RGB565\"\n"
        "}\n";
    write_manifest(outdir / "manifest.json", manifest);
    std::printf("Generated fill_basic in %s\n", outdir.string().c_str());
    return true;
}

bool generate_one_blit_fixture(const fs::path& dir, const std::string& kind) {
    fs::create_directories(dir);
    const FbConfig fb{};
    auto initial = make_pattern(fb.stride, fb.height, kind[0]);

    // texture 8x8
    const u32 tex_base = 0x20000;
    const u32 tex_stride = 16;
    const u32 tex_w = 8;
    const u32 tex_h = 8;
    PixelFormat tex_fmt = PixelFormat::RGB565;
    const u32 bpp = 4;  // fill with ARGB pattern when needed

    std::vector<u8> tex;
    std::vector<GpuCmd64> cmds;

    if (kind == "blit_rgb565_basic") {
        tex.assign(tex_stride * tex_h, 0);
        SurfaceDesc td{tex_base, tex_stride, tex_w, tex_h, PixelFormat::RGB565};
        GoldenGPU g0;
        g0.register_surface(td, "t");
        Surface ts(&g0.memory(), td);
        for (u32 y = 0; y < tex_h; ++y) {
            for (u32 x = 0; x < tex_w; ++x) {
                ts.write_pixel(x, y, Rgba8888::pack(255, 255, 0, 0));
            }
        }
        std::vector<u8> dump;
        g0.memory().read_block(tex_base, tex.size(), dump);
        tex = dump;
        BlitCmdDesc d;
        d.src_base = tex_base;
        d.dst_base = fb.base;
        d.src_stride = tex_stride;
        d.dst_stride = fb.stride;
        d.src_x = 1;
        d.src_y = 1;
        d.dst_x = 2;
        d.dst_y = 3;
        d.w = 5;
        d.h = 5;
        cmds.push_back(make_blit_cmd(d));
    } else if (kind == "blit_colorkey") {
        tex.assign(tex_stride * tex_h, 0);
        SurfaceDesc td{tex_base, tex_stride, tex_w, tex_h, PixelFormat::RGB565};
        GoldenGPU g0;
        g0.register_surface(td, "t");
        Surface ts(&g0.memory(), td);
        for (u32 y = 0; y < tex_h; ++y) {
            for (u32 x = 0; x < tex_w; ++x) {
                ts.write_pixel(x, y, Rgba8888::pack(255, 0, 255, 0));
            }
        }
        ts.write_pixel(2, 2, Rgba8888::pack(255, 255, 0, 0));
        ts.write_pixel(3, 3, Rgba8888::pack(255, 0, 0, 255));
        std::vector<u8> dump;
        g0.memory().read_block(tex_base, tex.size(), dump);
        tex = dump;
        BlitCmdDesc d;
        d.src_base = tex_base;
        d.dst_base = fb.base;
        d.src_stride = tex_stride;
        d.dst_stride = fb.stride;
        d.w = 8;
        d.h = 8;
        d.color_key_en = true;
        d.color_key_rgb = 0x00FF00;
        cmds.push_back(make_blit_cmd(d));
    } else if (kind == "blit_alpha_argb8888") {
        tex_fmt = PixelFormat::ARGB8888;
        const u32 tstride = 32;  // 8*4
        tex.assign(tstride * tex_h, 0);
        SurfaceDesc td{tex_base, tstride, tex_w, tex_h, PixelFormat::ARGB8888};
        GoldenGPU g0;
        g0.register_surface(td, "t");
        Surface ts(&g0.memory(), td);
        for (u32 y = 0; y < tex_h; ++y) {
            for (u32 x = 0; x < tex_w; ++x) {
                const u8 a = static_cast<u8>((x * 32u + y * 16u) & 0xFFu);
                ts.write_pixel(x, y, Rgba8888::pack(a, 255, 255, 0));
            }
        }
        std::vector<u8> dump;
        g0.memory().read_block(tex_base, tex.size(), dump);
        tex = dump;
        BlitCmdDesc d;
        d.src_base = tex_base;
        d.dst_base = fb.base;
        d.src_stride = tstride;
        d.dst_stride = fb.stride;
        d.src_format = static_cast<u32>(PixelFormat::ARGB8888);
        d.w = 8;
        d.h = 8;
        d.blend = static_cast<u32>(BlendMode::STRAIGHT_ALPHA);
        d.pixel_alpha_en = true;
        cmds.push_back(make_blit_cmd(d));
    } else if (kind == "blit_overdraw_rgb565") {
        tex.assign(tex_stride * tex_h, 0);
        SurfaceDesc td{tex_base, tex_stride, tex_w, tex_h, PixelFormat::RGB565};
        GoldenGPU g0;
        g0.register_surface(td, "t");
        Surface ts(&g0.memory(), td);
        for (u32 y = 0; y < tex_h; ++y) {
            for (u32 x = 0; x < tex_w; ++x) {
                ts.write_pixel(x, y, Rgba8888::pack(255, 255, 0, 0));
            }
        }
        std::vector<u8> dump;
        g0.memory().read_block(tex_base, tex.size(), dump);
        tex = dump;
        for (int i = 0; i < 3; ++i) {
            BlitCmdDesc d;
            d.src_base = tex_base;
            d.dst_base = fb.base;
            d.src_stride = tex_stride;
            d.dst_stride = fb.stride;
            d.w = 4;
            d.h = 4;
            d.blend = static_cast<u32>(BlendMode::STRAIGHT_ALPHA);
            d.global_alpha_en = true;
            d.global_alpha = 128;
            cmds.push_back(make_blit_cmd(d));
        }
    } else if (kind == "blit_additive") {
        tex.assign(tex_stride * tex_h, 0);
        SurfaceDesc td{tex_base, tex_stride, tex_w, tex_h, PixelFormat::RGB565};
        GoldenGPU g0;
        g0.register_surface(td, "t");
        Surface ts(&g0.memory(), td);
        for (u32 y = 0; y < tex_h; ++y) {
            for (u32 x = 0; x < tex_w; ++x) {
                ts.write_pixel(x, y, Rgba8888::pack(255, 80, 40, 20));
            }
        }
        std::vector<u8> dump;
        g0.memory().read_block(tex_base, tex.size(), dump);
        tex = dump;
        BlitCmdDesc d;
        d.src_base = tex_base;
        d.dst_base = fb.base;
        d.src_stride = tex_stride;
        d.dst_stride = fb.stride;
        d.w = 8;
        d.h = 8;
        d.blend = static_cast<u32>(BlendMode::ADD_SAT);
        cmds.push_back(make_blit_cmd(d));
    } else {
        std::fprintf(stderr, "unknown fixture kind %s\n", kind.c_str());
        return false;
    }

    // Execute for golden_fb
    GoldenGPU gpu;
    SurfaceDesc dst{fb.base, fb.stride, fb.width, fb.height, PixelFormat::RGB565};
    SurfaceDesc src{tex_base, (kind == "blit_alpha_argb8888") ? 32u : tex_stride, tex_w,
                    tex_h, tex_fmt};
    gpu.register_surface(dst, "dst");
    gpu.register_surface(src, "src");
    gpu.memory().write_block(fb.base, initial.data(), initial.size());
    gpu.memory().write_block(tex_base, tex.data(), tex.size());
    const auto st = gpu.execute_stream(cmds);
    if (!st.ok) {
        std::fprintf(stderr, "fixture exec fail %s fault=%u\n", kind.c_str(),
                     static_cast<u32>(st.fault));
        return false;
    }
    std::vector<u8> golden_fb;
    gpu.memory().read_block(fb.base, initial.size(), golden_fb);

    // Serialize command stream
    std::vector<u8> cmd_bytes;
    for (const auto& c : cmds) {
        const auto b = serialize_cmd_le(c);
        cmd_bytes.insert(cmd_bytes.end(), b.begin(), b.end());
    }
    write_file(dir / "command.bin", cmd_bytes.data(), cmd_bytes.size());
    write_file(dir / "initial_fb.raw", initial.data(), initial.size());
    write_file(dir / "textures.bin", tex.data(), tex.size());
    write_file(dir / "golden_fb.raw", golden_fb.data(), golden_fb.size());

    char manifest[512];
    std::snprintf(manifest, sizeof(manifest),
                  "{\n"
                  "  \"format_version\": 1,\n"
                  "  \"isa_version\": 1,\n"
                  "  \"pixel_arith_version\": 1,\n"
                  "  \"width\": 16,\n"
                  "  \"height\": 16,\n"
                  "  \"stride\": 32,\n"
                  "  \"framebuffer_format\": \"RGB565\",\n"
                  "  \"texture_base\": 131072,\n"
                  "  \"texture_format\": \"%s\",\n"
                  "  \"command_count\": %zu,\n"
                  "  \"base\": 65536,\n"
                  "  \"description\": \"%s\"\n"
                  "}\n",
                  (kind == "blit_alpha_argb8888") ? "ARGB8888" : "RGB565", cmds.size(),
                  kind.c_str());
    write_manifest(dir / "manifest.json", manifest);
    std::printf("Generated %s\n", kind.c_str());
    (void)bpp;
    return true;
}

bool generate_stage002_fixtures(const fs::path& root) {
    const char* kinds[] = {"blit_rgb565_basic", "blit_colorkey", "blit_alpha_argb8888",
                           "blit_overdraw_rgb565", "blit_additive"};
    for (const char* k : kinds) {
        if (!generate_one_blit_fixture(root / k, k)) {
            return false;
        }
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
    if (mode == "run-stream" || mode == "run-fill") {
        return run_stream_impl(argc, argv) ? 0 : 1;
    }
    return usage();
}
