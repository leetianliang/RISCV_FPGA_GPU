#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int usage() {
    std::fprintf(stderr,
                 "Usage:\n"
                 "  golden_cli generate-fill-basic <outdir>\n"
                 "  golden_cli run-fill --cmd C.bin --initial-fb I.raw\n"
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

bool read_file(const fs::path& path, std::vector<golden::u8>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

int generate_fill_basic(const fs::path& outdir) {
    fs::create_directories(outdir);

    constexpr golden::u32 kWidth = 16;
    constexpr golden::u32 kHeight = 16;
    constexpr golden::u32 kStride = 32;
    constexpr golden::u32 kBase = 0x10000;

    // initial framebuffer: deterministic pattern
    std::vector<golden::u8> initial(static_cast<std::size_t>(kStride) * kHeight);
    for (std::size_t i = 0; i < initial.size(); ++i) {
        initial[i] = static_cast<golden::u8>((i * 17u + 31u) & 0xFFu);
    }

    // Interior rectangle fill, nontrivial color 0xFF2A55C8
    const golden::Rgba8888 color = golden::Rgba8888::from_u32(0xFF2A55C8u);
    const golden::GpuCmd64 cmd = golden::make_fill_rect_cmd(
        kBase, kStride, 3, 4, 7, 6, color);

    golden::GoldenGPU gpu;
    golden::SurfaceDesc desc;
    desc.base = kBase;
    desc.stride = kStride;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.format = golden::PixelFormat::RGB565;
    if (!gpu.register_surface(desc, "fill_basic_fb").ok) {
        std::fprintf(stderr, "register_surface failed\n");
        return 1;
    }
    if (gpu.memory().write_block(kBase, initial.data(), initial.size()).status !=
        golden::MemAccessStatus::OK) {
        std::fprintf(stderr, "load initial fb failed\n");
        return 1;
    }
    if (!gpu.execute_command(cmd).ok) {
        std::fprintf(stderr, "execute FILL failed\n");
        return 1;
    }
    std::vector<golden::u8> golden_fb;
    if (gpu.memory().read_block(kBase, initial.size(), golden_fb).status !=
        golden::MemAccessStatus::OK) {
        std::fprintf(stderr, "read golden fb failed\n");
        return 1;
    }

    if (!write_file(outdir / "command.bin", cmd.data(), sizeof(cmd))) {
        std::fprintf(stderr, "write command.bin failed\n");
        return 1;
    }
    if (!write_file(outdir / "initial_fb.raw", initial.data(), initial.size())) {
        std::fprintf(stderr, "write initial_fb.raw failed\n");
        return 1;
    }
    if (!write_file(outdir / "golden_fb.raw", golden_fb.data(), golden_fb.size())) {
        std::fprintf(stderr, "write golden_fb.raw failed\n");
        return 1;
    }

    const char* manifest =
        "{\n"
        "  \"format_version\": 1,\n"
        "  \"isa_version\": 1,\n"
        "  \"pixel_arith_version\": 1,\n"
        "  \"width\": 16,\n"
        "  \"height\": 16,\n"
        "  \"stride\": 32,\n"
        "  \"framebuffer_format\": \"RGB565\",\n"
        "  \"command_count\": 1,\n"
        "  \"base\": 40960,\n"
        "  \"description\": \"Stage-001 FILL_RECT interior rectangle 3,4 7x6 color 0xFF2A55C8 COPY RGB565\"\n"
        "}\n";
    if (!write_file(outdir / "manifest.json", manifest, std::strlen(manifest))) {
        std::fprintf(stderr, "write manifest.json failed\n");
        return 1;
    }
    std::printf("Generated fill_basic fixture in %s\n", outdir.string().c_str());
    return 0;
}

std::string arg_value(int argc, char** argv, const std::string& key) {
    for (int i = 2; i + 1 < argc; ++i) {
        if (key == argv[i]) {
            return argv[i + 1];
        }
    }
    return {};
}

int run_fill(int argc, char** argv) {
    const std::string cmd_path = arg_value(argc, argv, "--cmd");
    const std::string init_path = arg_value(argc, argv, "--initial-fb");
    const std::string width_s = arg_value(argc, argv, "--width");
    const std::string height_s = arg_value(argc, argv, "--height");
    const std::string stride_s = arg_value(argc, argv, "--stride");
    const std::string base_s = arg_value(argc, argv, "--base");
    const std::string out_path = arg_value(argc, argv, "--out");
    if (cmd_path.empty() || init_path.empty() || out_path.empty() || width_s.empty() ||
        height_s.empty() || stride_s.empty() || base_s.empty()) {
        return usage();
    }

    std::vector<golden::u8> cmd_bytes;
    if (!read_file(cmd_path, cmd_bytes) || cmd_bytes.size() != 64) {
        std::fprintf(stderr, "command.bin must be 64 bytes\n");
        return 1;
    }
    golden::GpuCmd64 cmd{};
    std::memcpy(cmd.data(), cmd_bytes.data(), 64);

    std::vector<golden::u8> initial;
    if (!read_file(init_path, initial)) {
        std::fprintf(stderr, "failed to read initial fb\n");
        return 1;
    }

    const auto width = static_cast<golden::u32>(std::stoul(width_s));
    const auto height = static_cast<golden::u32>(std::stoul(height_s));
    const auto stride = static_cast<golden::u32>(std::stoul(stride_s));
    const auto base = static_cast<golden::u32>(std::stoul(base_s, nullptr, 0));
    const std::size_t expected = static_cast<std::size_t>(stride) * height;
    if (initial.size() != expected) {
        std::fprintf(stderr, "initial fb size mismatch: %zu vs %zu\n", initial.size(),
                     expected);
        return 1;
    }

    golden::GoldenGPU gpu;
    golden::SurfaceDesc desc;
    desc.base = base;
    desc.stride = stride;
    desc.width = width;
    desc.height = height;
    desc.format = golden::PixelFormat::RGB565;
    if (!gpu.register_surface(desc, "run_fb").ok) {
        std::fprintf(stderr, "register_surface failed\n");
        return 1;
    }
    gpu.memory().write_block(base, initial.data(), initial.size());
    const auto st = gpu.execute_command(cmd);
    if (!st.ok) {
        std::fprintf(stderr, "execute failed fault=%u detail=%u\n",
                     static_cast<unsigned>(st.fault), st.fault_detail);
        return 1;
    }
    std::vector<golden::u8> out;
    if (gpu.memory().read_block(base, expected, out).status !=
        golden::MemAccessStatus::OK) {
        std::fprintf(stderr, "read fb failed\n");
        return 1;
    }
    if (!write_file(out_path, out.data(), out.size())) {
        std::fprintf(stderr, "write out failed\n");
        return 1;
    }
    std::printf("Wrote %s\n", out_path.c_str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return usage();
    }
    const std::string mode = argv[1];
    if (mode == "generate-fill-basic") {
        if (argc < 3) {
            return usage();
        }
        return generate_fill_basic(argv[2]);
    }
    if (mode == "run-fill") {
        return run_fill(argc, argv);
    }
    return usage();
}
