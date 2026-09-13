#include "golden_renderer.hpp"
#include "gpu2d/renderer.hpp"
#include "neon/assets.hpp"
#include "neon/sim.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {
int g_fail = 0;
#define CHECK(c)                                                                \
    do {                                                                        \
        if (!(c)) {                                                             \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c);             \
            ++g_fail;                                                           \
        }                                                                       \
    } while (0)

using namespace gpu2d;

u32 hash_fb(const GoldenBackend& g) {
    const u8* p = g.framebuffer();
    const u32 n = g.fb_stride() * g.fb_height();
    u32 h = 2166136261u;
    for (u32 i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

bool make_app(GoldenBackend& g, neon::Assets& a, BackendKind bk, u32 w, u32 h) {
    ProfileDesc p;
    p.width = w;
    p.height = h;
    p.format = PixelFormat::RGB565;
    p.tile_size = 32;
    if (!g.init(p)) {
        return false;
    }
    g.set_backend(bk);
    auto blobs = neon::build_procedural_assets();
    auto up = [&](const neon::SpriteBlob& b) {
        TextureDesc d;
        d.width = b.w;
        d.height = b.h;
        d.stride = b.stride;
        d.format = b.indexed8 ? PixelFormat::INDEX8 : PixelFormat::RGB565;
        d.pixels = b.pixels.data();
        if (b.indexed8) {
            d.palette = b.palette.data();
            d.palette_entries = 256;
        }
        return g.create_texture(d);
    };
    a.player = up(blobs.player);
    a.enemy_n = up(blobs.enemy_n);
    a.enemy_f = up(blobs.enemy_f);
    a.enemy_h = up(blobs.enemy_h);
    a.bullet = up(blobs.bullet);
    a.bullet_e = up(blobs.bullet_e);
    a.particle = up(blobs.particle);
    a.glow = up(blobs.glow);
    a.font = up(blobs.font);
    a.font_pal = up(blobs.font_index8);
    return a.player.valid();
}

}  // namespace

int main() {
    // B13: one-pixel FB mutation must break equality comparator
    {
        GoldenBackend imm, tile;
        neon::Assets ai, at;
        CHECK(make_app(imm, ai, BackendKind::Immediate, 64, 64));
        CHECK(make_app(tile, at, BackendKind::Tile32, 64, 64));
        CommandRecorder rec;
        rec.begin_frame();
        rec.fill_rect(0, 0, 64, 64, Color::rgb(10, 20, 30));
        rec.fill_rect(8, 8, 16, 16, Color::rgb(200, 40, 40));
        rec.present();
        CHECK(imm.execute_frame(rec.commands()));
        CHECK(tile.execute_frame(rec.commands()));
        const u32 n = imm.fb_stride() * imm.fb_height();
        CHECK(std::memcmp(imm.framebuffer(), tile.framebuffer(), n) == 0);
        // harness one-pixel mutation on tile only
        CommandRecorder mut;
        mut.begin_frame();
        mut.fill_rect(20, 20, 1, 1, Color::rgb(255, 255, 0));
        mut.present();
        CHECK(tile.execute_frame(mut.commands()));
        if (std::memcmp(imm.framebuffer(), tile.framebuffer(), n) == 0) {
            std::printf("FAIL mutation did not change FB\n");
            ++g_fail;
        }
        // command mutation: change fill color → different FB
        CommandRecorder rec2;
        rec2.begin_frame();
        rec2.fill_rect(0, 0, 64, 64, Color::rgb(10, 20, 30));
        rec2.fill_rect(8, 8, 16, 16, Color::rgb(200, 80, 40));  // clearly different after RGB565
        rec2.present();
        GoldenBackend imm2;
        neon::Assets a2;
        CHECK(make_app(imm2, a2, BackendKind::Immediate, 64, 64));
        CHECK(imm2.execute_frame(rec2.commands()));
        // re-run original on fresh GPU
        GoldenBackend imm3;
        neon::Assets a3;
        CHECK(make_app(imm3, a3, BackendKind::Immediate, 64, 64));
        CHECK(imm3.execute_frame(rec.commands()));
        if (hash_fb(imm2) == hash_fb(imm3)) {
            std::printf("FAIL command mutation not detected\n");
            ++g_fail;
        }
    }

    if (g_fail) {
        std::printf("gpu2d_test_mutation FAIL %d\n", g_fail);
        return 1;
    }
    std::printf("gpu2d_test_mutation PASS\n");
    return 0;
}
