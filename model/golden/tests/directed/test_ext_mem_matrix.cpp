#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"

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

void test_ext_header_matrix() {
    GoldenGPU gpu;
    gpu.register_surface(SurfaceDesc{0x10000, 32, 2, 2, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 8, 2, 2, PixelFormat::RGB565}, "s");

    auto put_ext = [&](u32 w0) {
        std::vector<u8> ext(64, 0);
        auto putw = [&](int i, u32 w) {
            ext[i * 4 + 0] = static_cast<u8>(w & 0xFF);
            ext[i * 4 + 1] = static_cast<u8>((w >> 8) & 0xFF);
            ext[i * 4 + 2] = static_cast<u8>((w >> 16) & 0xFF);
            ext[i * 4 + 3] = static_cast<u8>((w >> 24) & 0xFF);
        };
        putw(0, w0);
        putw(3, 0);  // U0
        putw(5, 65536);  // DU_DX
        putw(8, 65536);  // DV_DY
        gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
        gpu.memory().write_block(0x30000, ext.data(), 64);
    };

    auto blit_ext_cmd = [&]() {
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 8;
        d.dst_stride = 32;
        d.blit_ext = true;
        d.ext_ptr = 0x30000;
        d.w = 2;
        d.h = 2;
        return make_blit_ext_cmd(d);
    };

    // good header: type=1 ver=1 len=16
    put_ext(0x01u | (0x1u << 8) | (16u << 12));
    EXPECT_TRUE(gpu.execute_command(blit_ext_cmd()).ok);

    // bad type
    gpu.reset();
    gpu.register_surface(SurfaceDesc{0x10000, 32, 2, 2, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 8, 2, 2, PixelFormat::RGB565}, "s");
    put_ext(0x02u | (0x1u << 8) | (16u << 12));
    EXPECT_TRUE(gpu.execute_command(blit_ext_cmd()).fault ==
                FaultCode::BAD_EXT_TYPE);

    // bad version
    gpu.reset();
    gpu.register_surface(SurfaceDesc{0x10000, 32, 2, 2, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 8, 2, 2, PixelFormat::RGB565}, "s");
    put_ext(0x01u | (0x2u << 8) | (16u << 12));
    EXPECT_TRUE(gpu.execute_command(blit_ext_cmd()).fault ==
                FaultCode::BAD_EXT_TYPE);

    // bad length
    gpu.reset();
    gpu.register_surface(SurfaceDesc{0x10000, 32, 2, 2, PixelFormat::RGB565}, "d");
    gpu.register_surface(SurfaceDesc{0x20000, 8, 2, 2, PixelFormat::RGB565}, "s");
    put_ext(0x01u | (0x1u << 8) | (8u << 12));
    EXPECT_TRUE(gpu.execute_command(blit_ext_cmd()).fault ==
                FaultCode::BAD_EXT_TYPE);

    // W13 reserved nonzero: strict vs non-strict
    {
        auto make_with_reserved = [&](bool strict) {
            gpu.reset();
            gpu.register_surface(SurfaceDesc{0x10000, 32, 2, 2, PixelFormat::RGB565},
                                 "d");
            gpu.register_surface(SurfaceDesc{0x20000, 8, 2, 2, PixelFormat::RGB565},
                                 "s");
            std::vector<u8> ext(64, 0);
            auto putw = [&](int i, u32 w) {
                ext[i * 4 + 0] = static_cast<u8>(w & 0xFF);
                ext[i * 4 + 1] = static_cast<u8>((w >> 8) & 0xFF);
                ext[i * 4 + 2] = static_cast<u8>((w >> 16) & 0xFF);
                ext[i * 4 + 3] = static_cast<u8>((w >> 24) & 0xFF);
            };
            putw(0, 0x01u | (0x1u << 8) | (16u << 12));
            putw(5, 65536);
            putw(8, 65536);
            putw(13, 1);  // reserved
            gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
            gpu.memory().write_block(0x30000, ext.data(), 64);
            auto cmd = blit_ext_cmd();
            if (strict) {
                cmd[0] |= kHStrict;
            }
            return gpu.execute_command(cmd);
        };
        EXPECT_TRUE(make_with_reserved(false).ok);
        const auto s1 = make_with_reserved(true);
        EXPECT_TRUE(!s1.ok);
        EXPECT_TRUE(s1.fault == FaultCode::RESERVED_NONZERO);
    }
}

void test_memory_negatives() {
    // bilinear neighbor out of resource
    {
        GoldenGPU gpu;
        gpu.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::RGB565},
                             "d");
        gpu.register_resource(RegisteredResource{0x20000, 2, 1, 1, "s"});  // 1x1 only
        std::vector<u8> t = {0, 0xF8};
        gpu.memory().write_block(0x20000, t.data(), 2);
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 2;
        d.dst_stride = 16;
        d.blit_ext = true;
        d.ext_ptr = 0x30000;
        d.filter = static_cast<u32>(FilterMode::BILINEAR);
        d.w = 1;
        d.h = 1;
        d.u0 = 0;
        d.du_dx = 0;
        d.dv_dy = 0;
        auto ext0 = make_draw2d_ext_v1(d);
        gpu.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
        gpu.memory().write_block(0x30000, ext0.data(), ext0.size());
        auto cmd = make_blit_ext_cmd(d);
        cmd[10] = pack_wh(1, 1);
        cmd[11] = pack_wh(1, 1);
        // src_w=1 CLAMP, bilinear neighbor clamps to same texel — must succeed
        const auto st_ok = gpu.execute_command(cmd);
        if (!st_ok.ok) {
            std::printf("bilinear 1x1 fail fault=%u detail=%u\n",
                        static_cast<u32>(st_ok.fault), st_ok.fault_detail);
        }
        EXPECT_TRUE(st_ok.ok);

        // Resource 1x1, SRC_W=2 without valid neighbor storage: validate_view
        // uses resource width, so stride/size must cover resource. Register 1x1
        // RGB565 stride 2; command SRC_W=2 will validate against resource width 1.
        GoldenGPU gpu2;
        gpu2.register_surface(SurfaceDesc{0x10000, 16, 1, 1, PixelFormat::RGB565},
                              "d");
        gpu2.register_resource(RegisteredResource{0x20000, 2, 1, 1, "s"});
        std::vector<u8> t1 = {0, 0xF8};
        gpu2.memory().write_block(0x20000, t1.data(), 2);
        BlitCmdDesc d2;
        d2.src_base = 0x20000;
        d2.dst_base = 0x10000;
        d2.src_stride = 2;
        d2.dst_stride = 16;
        d2.blit_ext = true;
        d2.ext_ptr = 0x30000;
        d2.filter = static_cast<u32>(FilterMode::BILINEAR);
        d2.w = 1;
        d2.h = 1;
        d2.u0 = 0x8000;
        auto ext2 = make_draw2d_ext_v1(d2);
        gpu2.register_resource(RegisteredResource{0x30000, 64, 1, 1, "e"});
        gpu2.memory().write_block(0x30000, ext2.data(), ext2.size());
        auto cmd2 = make_blit_ext_cmd(d2);
        cmd2[10] = pack_wh(2, 1);  // request 2-wide source rect on 1-wide resource
        const auto st2 = gpu2.execute_command(cmd2);
        // Resource width is 1 so last-byte check uses width 1; bilinear clamps
        // neighbor into source rect [0,2) then map_tex_coord uses src_w=2 on
        // width-1 resource → neighbor x=1 is out of resource → MEMORY_ERROR or
        // BAD_RECT from validate/texture read. Deterministic: must fail.
        EXPECT_TRUE(!st2.ok);
    }

    // palette index*4 near overflow
    {
        GoldenGPU gpu;
        gpu.register_surface(SurfaceDesc{0x10000, 32, 2, 2, PixelFormat::RGB565},
                             "d");
        gpu.register_resource(RegisteredResource{0x20000, 4, 2, 2, "s"});
        std::vector<u8> idx = {0xFF, 0, 0, 0};  // index 255
        gpu.memory().write_block(0x20000, idx.data(), 4);
        gpu.register_resource(
            RegisteredResource{0x40000, 1024, 256, 1, "p"});  // only 256*4
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 2;
        d.dst_stride = 32;
        d.src_format = static_cast<u32>(PixelFormat::INDEX8);
        d.palette_en = true;
        d.palette_addr = 0x40000;
        d.w = 1;
        d.h = 1;
        EXPECT_TRUE(gpu.execute_command(make_blit_cmd(d)).ok);
        // unmapped palette region
        gpu.reset();
        gpu.register_surface(SurfaceDesc{0x10000, 32, 2, 2, PixelFormat::RGB565},
                             "d");
        gpu.register_resource(RegisteredResource{0x20000, 4, 2, 2, "s"});
        gpu.memory().write_block(0x20000, idx.data(), 4);
        d.palette_addr = 0x70000;  // not registered
        const auto st = gpu.execute_command(make_blit_cmd(d));
        EXPECT_TRUE(!st.ok);
        EXPECT_TRUE(st.fault == FaultCode::MEMORY_ERROR);
    }

    // source stride too small for registered width
    {
        GoldenGPU gpu;
        gpu.register_surface(SurfaceDesc{0x10000, 32, 8, 2, PixelFormat::RGB565},
                             "d");
        gpu.register_resource(RegisteredResource{0x20000, 16, 8, 2, "s"});
        BlitCmdDesc d;
        d.src_base = 0x20000;
        d.dst_base = 0x10000;
        d.src_stride = 4;  // < 8*2
        d.dst_stride = 32;
        d.w = 1;
        d.h = 1;
        const auto st = gpu.execute_command(make_blit_cmd(d));
        EXPECT_TRUE(!st.ok);
        EXPECT_TRUE(st.fault == FaultCode::BAD_RECT);
    }

    // destination write: last pixel of last row OK; fully outside is no-op success
    {
        GoldenGPU gpu;
        gpu.register_surface(SurfaceDesc{0x10000, 32, 8, 2, PixelFormat::RGB565},
                             "d");
        EXPECT_TRUE(gpu.execute_command(
                        make_fill_rect_cmd(0x10000, 32, 7, 1, 1, 1,
                                           Rgba8888::pack(255, 255, 0, 0)))
                        .ok);
        // fully outside right: raster empty → success (no write)
        EXPECT_TRUE(gpu.execute_command(
                        make_fill_rect_cmd(0x10000, 32, 8, 1, 1, 1,
                                           Rgba8888::pack(255, 0, 255, 0)))
                        .ok);
        u16 p = 0;
        gpu.memory().read16(0x10000 + 1 * 32 + 7 * 2, p);
        EXPECT_TRUE(p == 0xF800);  // red, not green from OOB fill
    }
}

}  // namespace

int main() {
    test_ext_header_matrix();
    test_memory_negatives();
    if (g_failures) {
        std::printf("golden_test_ext_mem_matrix FAIL %d\n", g_failures);
        return 1;
    }
    std::printf("golden_test_ext_mem_matrix PASS\n");
    return 0;
}
