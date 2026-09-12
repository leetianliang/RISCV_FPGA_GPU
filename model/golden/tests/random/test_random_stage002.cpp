#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

#include <cstdio>
#include <vector>

namespace {

// Deterministic LCG
struct Rng {
    golden::u32 s;
    explicit Rng(golden::u32 seed) : s(seed) {}
    golden::u32 next() {
        s = s * 1664525u + 1013904223u;
        return s;
    }
    golden::u32 range(golden::u32 n) { return n ? next() % n : 0; }
};

int g_failures = 0;

void run_case(golden::u32 seed) {
    Rng rng(seed);
    golden::GoldenGPU gpu;
    golden::SurfaceDesc dst{};
    dst.base = 0x10000;
    dst.stride = 64;
    dst.width = 32;
    dst.height = 32;
    dst.format = golden::PixelFormat::RGB565;
    golden::SurfaceDesc src{};
    src.base = 0x20000;
    src.stride = 32;
    src.width = 16;
    src.height = 16;
    src.format = golden::PixelFormat::RGB565;
    if (!gpu.register_surface(dst, "d").ok || !gpu.register_surface(src, "s").ok) {
        ++g_failures;
        return;
    }
    std::vector<golden::u8> z(dst.stride * dst.height, 0);
    gpu.memory().write_block(dst.base, z.data(), z.size());
    std::vector<golden::u8> tex(src.stride * src.height, 0);
    for (auto& b : tex) {
        b = static_cast<golden::u8>(rng.next());
    }
    gpu.memory().write_block(src.base, tex.data(), tex.size());

    std::vector<golden::GpuCmd64> stream;
    const int n = 8 + static_cast<int>(rng.range(8));
    for (int i = 0; i < n; ++i) {
        if (rng.range(2) == 0) {
            const auto color = golden::Rgba8888::pack(
                255, static_cast<golden::u8>(rng.range(256)),
                static_cast<golden::u8>(rng.range(256)),
                static_cast<golden::u8>(rng.range(256)));
            const auto w = 1u + rng.range(8);
            const auto h = 1u + rng.range(8);
            const auto x = rng.range(32u - w + 1u);
            const auto y = rng.range(32u - h + 1u);
            stream.push_back(golden::make_fill_rect_cmd(
                dst.base, dst.stride, static_cast<golden::i32>(x),
                static_cast<golden::i32>(y), w, h, color));
        } else {
            golden::BlitCmdDesc d;
            d.src_base = src.base;
            d.dst_base = dst.base;
            d.src_stride = src.stride;
            d.dst_stride = dst.stride;
            const auto w = 1u + rng.range(8);
            const auto h = 1u + rng.range(8);
            d.src_x = static_cast<golden::i32>(rng.range(8u - w + 1u));
            d.src_y = static_cast<golden::i32>(rng.range(8u - h + 1u));
            d.dst_x = static_cast<golden::i32>(rng.range(16u - w + 1u));
            d.dst_y = static_cast<golden::i32>(rng.range(16u - h + 1u));
            d.w = w;
            d.h = h;
            const golden::u32 pick = rng.range(3);
            if (pick == 0) {
                d.blend = static_cast<golden::u32>(golden::BlendMode::COPY);
            } else if (pick == 1) {
                d.blend = static_cast<golden::u32>(golden::BlendMode::STRAIGHT_ALPHA);
                d.global_alpha_en = true;
                d.global_alpha = static_cast<golden::u8>(rng.range(256));
            } else {
                d.blend = static_cast<golden::u32>(golden::BlendMode::ADD_SAT);
                d.global_alpha_en = true;
                d.global_alpha = static_cast<golden::u8>(rng.range(64));
            }
            stream.push_back(golden::make_blit_cmd(d));
        }
    }
    const auto st = gpu.execute_stream(stream);
    if (!st.ok) {
        std::printf("random seed %u failed stream[%u] fault=%u\n", seed, st.fault_index,
                    static_cast<golden::u32>(st.fault));
        ++g_failures;
    }
}

}  // namespace

int main() {
    // Fixed recorded seeds
    const golden::u32 seeds[] = {1u, 42u, 12345u, 0xC0FFEEu, 7u, 99u, 20260912u};
    for (golden::u32 s : seeds) {
        for (int k = 0; k < 20; ++k) {
            run_case(s + static_cast<golden::u32>(k) * 17u);
        }
    }
    if (g_failures) {
        std::printf("golden_test_random_stage002 FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_random_stage002 PASS\n");
    return 0;
}
