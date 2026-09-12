#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_failures = 0;

using namespace golden;

struct Rng {
    u32 s;
    explicit Rng(u32 seed) : s(seed) {}
    u32 next() {
        s = s * 1664525u + 1013904223u;
        return s;
    }
    u32 range(u32 n) { return n ? next() % n : 0; }
};

// Independent oracle for Stage-002 subset: FILL/BLIT COPY RGB565 only.
struct SimpleRef {
    std::vector<u8> fb;
    u32 w, h, stride;

    void write_px(u32 x, u32 y, Rgba8888 c) {
        const u16 px = rgb565_encode(c);
        const size_t off = y * stride + x * 2;
        fb[off] = static_cast<u8>(px & 0xFF);
        fb[off + 1] = static_cast<u8>((px >> 8) & 0xFF);
    }
};

void run_case(u32 seed) {
    Rng rng(seed);
    const u32 w = 16, h = 16, stride = 32;
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, stride, w, h, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 16, 8, 8, PixelFormat::RGB565}, "s");
    std::vector<u8> init(stride * h, 0);
    std::vector<u8> tex(16 * 8, 0);
    for (size_t i = 0; i < tex.size(); i += 2) {
        const u16 px = static_cast<u16>((i * 3) & 0xFFFF);
        tex[i] = static_cast<u8>(px & 0xFF);
        tex[i + 1] = static_cast<u8>(px >> 8);
    }
    gpu.memory().write_block(0x10000, init.data(), init.size());
    gpu.memory().write_block(0x20000, tex.data(), tex.size());

    SimpleRef ref{init, w, h, stride};

    std::vector<GpuCmd64> cmds;
    const int n = 4 + static_cast<int>(rng.range(4));
    for (int i = 0; i < n; ++i) {
        if (rng.range(2) == 0) {
            const auto color = Rgba8888::pack(
                255, static_cast<u8>(rng.range(256)), static_cast<u8>(rng.range(256)),
                static_cast<u8>(rng.range(256)));
            const u32 rw = 1 + rng.range(6);
            const u32 rh = 1 + rng.range(6);
            const u32 x = rng.range(w - rw + 1);
            const u32 y = rng.range(h - rh + 1);
            cmds.push_back(make_fill_rect_cmd(0x10000, stride, static_cast<i32>(x),
                                              static_cast<i32>(y), rw, rh, color));
            for (u32 yy = 0; yy < rh; ++yy) {
                for (u32 xx = 0; xx < rw; ++xx) {
                    ref.write_px(x + xx, y + yy, color);
                }
            }
        } else {
            BlitCmdDesc d;
            d.src_base = 0x20000;
            d.dst_base = 0x10000;
            d.src_stride = 16;
            d.dst_stride = stride;
            d.w = 1 + rng.range(4);
            d.h = 1 + rng.range(4);
            d.src_x = rng.range(8 - d.w + 1);
            d.src_y = rng.range(8 - d.h + 1);
            d.dst_x = static_cast<i32>(rng.range(w - d.w + 1));
            d.dst_y = static_cast<i32>(rng.range(h - d.h + 1));
            d.blend = static_cast<u32>(rng.range(2) ? BlendMode::COPY
                                                    : BlendMode::COPY);
            cmds.push_back(make_blit_cmd(d));
            for (u32 yy = 0; yy < d.h; ++yy) {
                for (u32 xx = 0; xx < d.w; ++xx) {
                    const size_t off = (d.src_y + yy) * 16 + (d.src_x + xx) * 2;
                    const u16 px =
                        static_cast<u16>(tex[off] | (tex[off + 1] << 8));
                    ref.write_px(static_cast<u32>(d.dst_x) + xx,
                                 static_cast<u32>(d.dst_y) + yy, rgb565_decode(px));
                }
            }
        }
    }
    const auto st = gpu.execute_stream(cmds);
    if (!st.ok) {
        std::printf("diff fail seed=%u fault=%u\n", seed, static_cast<u32>(st.fault));
        ++g_failures;
        return;
    }
    std::vector<u8> got;
    gpu.memory().read_block(0x10000, stride * h, got);
    if (got != ref.fb) {
        for (size_t i = 0; i < got.size(); ++i) {
            if (got[i] != ref.fb[i]) {
                std::printf("diff seed=%u byte=%zu exp=%02x got=%02x\n", seed, i, ref.fb[i],
                            got[i]);
                break;
            }
        }
        ++g_failures;
    }
}

}  // namespace

int main() {
    const u32 seeds[] = {11u, 22u, 33u, 44u, 55u, 66u, 77u, 88u};
    for (u32 s : seeds) {
        for (int k = 0; k < 10; ++k) {
            run_case(s + static_cast<u32>(k) * 13u);
        }
    }
    if (g_failures) {
        std::printf("golden_test_random_diff FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_random_diff PASS\n");
    return 0;
}
