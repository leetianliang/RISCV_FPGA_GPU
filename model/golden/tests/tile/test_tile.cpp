#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/tile_binner.hpp"
#include "golden/tile_types.hpp"

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

void test_header_roundtrip() {
    TileHeader h;
    h.work_offset = 7;
    h.work_count = 3;
    h.flags = kTileDontLoadColor;
    const auto bytes = serialize_tile_header(h);
    TileHeader o;
    EXPECT_TRUE(parse_tile_header(bytes.data(), o));
    EXPECT_TRUE(o.work_offset == 7 && o.work_count == 3 && o.flags == 1);
}

void test_workref_range() {
    EXPECT_TRUE(tile_id(0, 0, 4) == 0);
    EXPECT_TRUE(tile_id(3, 2, 4) == 11);
    u32 x0, y0, w, h;
    tile_valid_rect(1, 1, 32, 80, 80, x0, y0, w, h);
    EXPECT_TRUE(x0 == 32 && y0 == 32 && w == 32 && h == 32);
    tile_valid_rect(2, 2, 32, 80, 80, x0, y0, w, h);
    EXPECT_TRUE(x0 == 64 && w == 16 && h == 16);
}

void test_binner_order_and_tiles() {
    // Three overlapping fills in one tile + one fill crossing boundary
    std::vector<GpuCmd64> draws;
    draws.push_back(make_fill_rect_cmd(0x10000, 64, 4, 4, 8, 8,
                                       Rgba8888::pack(255, 255, 0, 0)));
    draws.push_back(make_fill_rect_cmd(0x10000, 64, 6, 6, 8, 8,
                                       Rgba8888::pack(128, 0, 255, 0)));
    draws.push_back(make_fill_rect_cmd(0x10000, 64, 8, 8, 8, 8,
                                       Rgba8888::pack(255, 0, 0, 255)));
    draws.push_back(make_fill_rect_cmd(0x10000, 64, 28, 28, 8, 8,
                                       Rgba8888::pack(255, 255, 255, 0)));
    GoldenGPU dummy;
    const auto bin = bin_draws(draws, dummy.memory(), 64, 64, 32);
    EXPECT_TRUE(bin.descriptors.size() == 4);
    // tile (0,0) should have A,B,C in order
    const TileHeader& t00 = bin.headers[0];
    // A,B,C in tile (0,0); D (28,28,8x8) also overlaps tile (0,0)
    EXPECT_TRUE(t00.work_count == 4);
    EXPECT_TRUE(bin.workrefs[t00.work_offset + 0] == 0);
    EXPECT_TRUE(bin.workrefs[t00.work_offset + 1] == 1);
    EXPECT_TRUE(bin.workrefs[t00.work_offset + 2] == 2);
    EXPECT_TRUE(bin.workrefs[t00.work_offset + 3] == 3);
    // cross-boundary draw touches 4 tiles at 28..36
    u32 active = 0;
    for (const auto& h : bin.headers) {
        if (h.work_count) ++active;
    }
    EXPECT_TRUE(active >= 2);
}

void test_binner_outside_noop() {
    std::vector<GpuCmd64> draws;
    draws.push_back(make_fill_rect_cmd(0x10000, 64, 100, 100, 4, 4,
                                       Rgba8888::pack(255, 1, 2, 3)));
    draws.push_back(make_fill_rect_cmd(0x10000, 64, 0, 0, 4, 4,
                                       Rgba8888::pack(255, 4, 5, 6)));
    GoldenGPU dummy;
    const auto bin = bin_draws(draws, dummy.memory(), 64, 64, 32);
    u32 total = 0;
    for (const auto& h : bin.headers) total += h.work_count;
    EXPECT_TRUE(total == 1);
    EXPECT_TRUE(bin.workrefs[0] == 1);  // second draw only
}

void test_tile_frame_exec() {
    // Immediate vs Tile: two overlapping alpha fills
    // 64px RGB565 needs stride >= 128
    const u32 stride = 128;
    auto run_imm = [&](std::vector<u8>& fb) {
        GoldenGPU gpu;
        gpu.register_surface(SurfaceDesc{0x10000, stride, 64, 64, PixelFormat::RGB565},
                             "d");
        std::vector<u8> init(stride * 64, 0);
        gpu.memory().write_block(0x10000, init.data(), init.size());
        auto c1 = make_fill_rect_cmd(0x10000, stride, 10, 10, 20, 20,
                                     Rgba8888::pack(255, 255, 0, 0));
        auto c2 = make_fill_rect_cmd(0x10000, stride, 15, 15, 20, 20,
                                     Rgba8888::pack(128, 0, 0, 255));
        u32 ds = c2[12];
        ds = (ds & ~(0xFu << 8)) | (static_cast<u32>(BlendMode::STRAIGHT_ALPHA) << 8);
        ds |= (1u << 20);  // PIXEL_ALPHA_EN
        c2[12] = ds;
        c2[14] = (128u << 24);
        EXPECT_TRUE(gpu.execute_command(c1).ok);
        const auto e2 = gpu.execute_command(c2);
        if (!e2.ok) {
            std::printf("imm c2 fail fault=%u\n", static_cast<u32>(e2.fault));
        }
        EXPECT_TRUE(e2.ok);
        gpu.memory().read_block(0x10000, stride * 64, fb);
    };
    auto run_tile = [&](std::vector<u8>& fb) {
        GoldenGPU gpu;
        gpu.register_surface(SurfaceDesc{0x10000, stride, 64, 64, PixelFormat::RGB565},
                             "d");
        std::vector<u8> init(stride * 64, 0);
        gpu.memory().write_block(0x10000, init.data(), init.size());
        auto c1 = make_fill_rect_cmd(0x10000, stride, 10, 10, 20, 20,
                                     Rgba8888::pack(255, 255, 0, 0));
        auto c2 = make_fill_rect_cmd(0x10000, stride, 15, 15, 20, 20,
                                     Rgba8888::pack(128, 0, 0, 255));
        u32 ds = c2[12];
        ds = (ds & ~(0xFu << 8)) | (static_cast<u32>(BlendMode::STRAIGHT_ALPHA) << 8);
        ds |= (1u << 20);
        c2[12] = ds;
        c2[14] = (128u << 24);
        std::vector<GpuCmd64> draws = {c1, c2};
        const auto bin = bin_draws(draws, gpu.memory(), 64, 64, 32);
        const u32 desc_base = 0x30000;
        const u32 hdr_base = 0x31000;
        const u32 work_base = 0x32000;
        // Register regions before writing.
        gpu.register_resource(RegisteredResource{desc_base, 128, 1, 1, "d"});
        gpu.register_resource(RegisteredResource{hdr_base, 4 * 16, 1, 1, "h"});
        const u32 wsize = static_cast<u32>(bin.workrefs.size() * 4 + 4);
        gpu.register_resource(RegisteredResource{work_base, wsize, 1, 1, "w"});
        for (size_t i = 0; i < draws.size(); ++i) {
            const auto b = serialize_cmd_le(draws[i]);
            gpu.memory().write_block(desc_base + static_cast<u32>(i) * 64, b.data(), 64);
        }
        for (size_t i = 0; i < bin.headers.size(); ++i) {
            const auto hb = serialize_tile_header(bin.headers[i]);
            gpu.memory().write_block(hdr_base + static_cast<u32>(i) * 16, hb.data(),
                                     16);
        }
        const auto wr = serialize_workrefs(bin.workrefs);
        gpu.memory().write_block(work_base, wr.data(), wr.size());
        TileFrameCmd tf;
        tf.draw_desc_base = desc_base;
        tf.tile_header_base = hdr_base;
        tf.work_list_base = work_base;
        tf.dst_base = 0x10000;
        tf.dst_stride = stride;
        tf.surface_w = 64;
        tf.surface_h = 64;
        tf.grid_w = 2;
        tf.grid_h = 2;
        tf.tile_w = 32;
        tf.tile_h = 32;
        tf.rt_state = static_cast<u32>(PixelFormat::RGB565);
        const auto st = execute_tile_frame(gpu, make_tile_frame_cmd(tf), 2);
        if (!st.ok) {
            std::printf("tile frame fail fault=%u detail=%u\n",
                        static_cast<u32>(st.fault), st.fault_detail);
        }
        EXPECT_TRUE(st.ok);
        gpu.memory().read_block(0x10000, stride * 64, fb);
    };
    std::vector<u8> a, b;
    run_imm(a);
    run_tile(b);
    EXPECT_TRUE(a.size() == b.size());
    u32 mism = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) ++mism;
    }
    if (mism) {
        std::printf("imm vs tile mismatch %u bytes\n", mism);
    }
    EXPECT_TRUE(mism == 0);
}

void test_tile_order_sensitive() {
    // Reversed workrefs must produce different result for alpha over black vs white
    auto paint = [&](bool reverse, std::vector<u8>& fb) {
        GoldenGPU gpu;
        gpu.register_surface(SurfaceDesc{0x10000, 64, 32, 32, PixelFormat::RGB565},
                             "d");
        std::vector<u8> init(64 * 32, 0);
        gpu.memory().write_block(0x10000, init.data(), init.size());
        auto white = make_fill_rect_cmd(0x10000, 64, 0, 0, 32, 32,
                                        Rgba8888::pack(255, 255, 255, 255));
        auto halfred = make_fill_rect_cmd(0x10000, 64, 0, 0, 32, 32,
                                          Rgba8888::pack(128, 255, 0, 0));
        u32 ds = halfred[12];
        ds = (ds & ~(0xFu << 8)) |
             (static_cast<u32>(BlendMode::STRAIGHT_ALPHA) << 8);
        ds |= (1u << 20);  // PIXEL_ALPHA_EN
        halfred[12] = ds;
        halfred[14] = (128u << 24);
        std::vector<GpuCmd64> draws = {white, halfred};
        (void)draws;
        if (reverse) {
            EXPECT_TRUE(gpu.execute_command(halfred).ok);
            EXPECT_TRUE(gpu.execute_command(white).ok);
        } else {
            EXPECT_TRUE(gpu.execute_command(white).ok);
            EXPECT_TRUE(gpu.execute_command(halfred).ok);
        }
        gpu.memory().read_block(0x10000, 64 * 32, fb);
    };
    std::vector<u8> a, b;
    paint(false, a);
    paint(true, b);
    u32 mism = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) ++mism;
    }
    if (mism == 0) {
        std::printf("order-sensitive: frames identical (unexpected)\n");
    }
    EXPECT_TRUE(mism > 0);
}

}  // namespace

int main() {
    test_header_roundtrip();
    test_workref_range();
    test_binner_order_and_tiles();
    test_binner_outside_noop();
    test_tile_frame_exec();
    test_tile_order_sensitive();
    if (g_failures) {
        std::printf("golden_test_tile FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_tile PASS\n");
    return 0;
}
