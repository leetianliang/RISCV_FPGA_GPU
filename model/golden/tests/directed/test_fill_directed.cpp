#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/gpu_math.hpp"

#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;

#define EXPECT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++g_failures;                                                     \
        }                                                                     \
    } while (0)

struct Fx {
    golden::GoldenGPU gpu;
    golden::SurfaceDesc desc{};
    std::vector<golden::u8> initial;

    Fx() {
        desc.base = 0x10000;
        desc.stride = 32;
        desc.width = 8;
        desc.height = 8;
        desc.format = golden::PixelFormat::RGB565;
        gpu.register_surface(desc, "fb");
        initial.assign(32 * 8, 0x11);
        gpu.memory().write_block(desc.base, initial.data(), initial.size());
    }

    golden::u16 px(golden::u32 x, golden::u32 y) const {
        golden::u16 v = 0;
        gpu.memory().read16(desc.base + y * desc.stride + x * 2, v);
        return v;
    }
};

void test_fill_cases() {
    Fx f;
    const auto c = golden::Rgba8888::pack(255, 255, 0, 0);
    EXPECT_TRUE(f.gpu.execute_command(
                    golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 1, 1, 2, 2, c))
                    .ok);
    EXPECT_TRUE(f.px(1, 1) == 0xF800);
    EXPECT_TRUE(f.px(2, 2) == 0xF800);
    // zero size no-op
    const auto before = f.px(0, 0);
    EXPECT_TRUE(f.gpu.execute_command(
                    golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 0, 4, c))
                    .ok);
    EXPECT_TRUE(f.px(0, 0) == before);
    // full
    EXPECT_TRUE(f.gpu.execute_command(
                    golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 8, 8, c))
                    .ok);
    for (golden::u32 y = 0; y < 8; ++y) {
        for (golden::u32 x = 0; x < 8; ++x) {
            EXPECT_TRUE(f.px(x, y) == 0xF800);
        }
    }
}

void test_fill_additive() {
    Fx f;
    // fill black then additive red-ish via fill
    f.gpu.execute_command(golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 8, 8,
                                                     golden::Rgba8888::pack(255, 0, 0, 0)));
    auto cmd = golden::make_fill_rect_cmd(f.desc.base, f.desc.stride, 0, 0, 1, 1,
                                          golden::Rgba8888::pack(255, 100, 0, 0));
    // set blend ADD_SAT
    golden::u32 ds = cmd[12];
    ds = (ds & ~(0xFu << 8)) | (static_cast<golden::u32>(golden::BlendMode::ADD_SAT) << 8);
    cmd[12] = ds;
    EXPECT_TRUE(f.gpu.execute_command(cmd).ok);
    // dst was 0 after black encode; add 100
    golden::Rgba8888 got{};
    golden::Surface s(&f.gpu.memory(), f.desc);
    s.read_pixel(0, 0, got);
    EXPECT_TRUE(got.r() > 50);
}

}  // namespace

int main() {
    test_fill_cases();
    test_fill_additive();
    if (g_failures) {
        std::printf("golden_test_fill_directed FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_fill_directed PASS\n");
    return 0;
}
