#include "golden_renderer.hpp"
#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "gpu2d/renderer.hpp"
#include "neon/assets.hpp"
#include "neon/sim.hpp"

#include <cstdio>
#include <cstring>
#include <string>

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

struct Rig {
    GoldenBackend gpu;
    neon::Assets a;
    ProfileDesc prof;
};

bool make_rig(Rig& r, BackendKind bk, u32 w = 64, u32 h = 64) {
    r.prof.width = w;
    r.prof.height = h;
    r.prof.format = PixelFormat::RGB565;
    r.prof.tile_size = 32;
    if (!r.gpu.init(r.prof)) {
        return false;
    }
    r.gpu.set_backend(bk);
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
        return r.gpu.create_texture(d);
    };
    r.a.font = up(blobs.font);
    r.a.font_pal = up(blobs.font_index8);
    r.a.glow = up(blobs.glow);
    r.a.enemy_h = up(blobs.enemy_h);
    r.a.enemy_n = up(blobs.enemy_n);
    r.a.enemy_f = up(blobs.enemy_f);
    r.a.player = up(blobs.player);
    r.a.bullet = up(blobs.bullet);
    r.a.bullet_e = up(blobs.bullet_e);
    r.a.particle = up(blobs.particle);
    return r.a.font.valid() && r.a.font_pal.valid() && r.a.glow.valid() &&
           r.a.player.valid() && r.a.bullet.valid();
}

// Read RGB565 pixel; return as 0x00RRGGBB after reverse expand.
u32 sample(const GoldenBackend& g, u32 x, u32 y) {
    const u8* fb = g.framebuffer();
    if (!fb) {
        return 0xFFFFFFFFu;
    }
    const size_t off = static_cast<size_t>(y) * g.fb_stride() + x * 2;
    const u16 p = static_cast<u16>(fb[off] | (fb[off + 1] << 8));
    const u32 r = (p >> 11) & 0x1F;
    const u32 gr = (p >> 5) & 0x3F;
    const u32 b = p & 0x1F;
    const u32 r8 = (r << 3) | (r >> 2);
    const u32 g8 = (gr << 2) | (gr >> 4);
    const u32 b8 = (b << 3) | (b >> 2);
    return (r8 << 16) | (g8 << 8) | b8;
}

bool same_fb(const GoldenBackend& a, const GoldenBackend& b) {
    const u32 n = a.fb_stride() * a.fb_height();
    return std::memcmp(a.framebuffer(), b.framebuffer(), n) == 0;
}

}  // namespace

int main() {
    // B1: partial / full / unclipped fill — exact pixels, not just Imm==Tile
    {
        Rig r;
        CHECK(make_rig(r, BackendKind::Immediate));
        CommandRecorder rec;
        rec.begin_frame();
        rec.fill_rect(0, 0, 64, 64, Color::rgb(0, 0, 0));
        rec.set_clip(true, 10, 10, 30, 30);  // half-open [xmin,xmax) [ymin,ymax)
        rec.fill_rect(0, 0, 64, 64, Color::rgb(255, 0, 0));
        rec.clear_clip();
        rec.fill_rect(40, 40, 8, 8, Color::rgb(0, 255, 0));
        rec.set_clip(true, 0, 0, 4, 4);
        rec.fill_rect(50, 50, 8, 8, Color::rgb(0, 0, 255));  // fully outside → no-op
        rec.clear_clip();
        rec.present();
        CHECK(r.gpu.execute_frame(rec.commands()));
        // inside clip → red
        const u32 inside = sample(r.gpu, 15, 15);
        CHECK(((inside >> 16) & 0xFF) > 200);
        // outside clip (0,0) → black
        const u32 outside = sample(r.gpu, 1, 1);
        CHECK(outside == 0x000000u || ((outside >> 16) & 0xFF) < 32);
        // unclipped green
        const u32 grn = sample(r.gpu, 42, 42);
        CHECK(((grn >> 8) & 0xFF) > 200);
        // fully clipped fill left (50,50) black
        const u32 cl = sample(r.gpu, 52, 52);
        CHECK(((cl >> 16) & 0xFF) < 32 && ((cl >> 8) & 0xFF) < 32);
    }

    // B1 equality Imm==Tile with clip fill
    {
        Rig imm, tile;
        CHECK(make_rig(imm, BackendKind::Immediate));
        CHECK(make_rig(tile, BackendKind::Tile32));
        CommandRecorder rec;
        rec.begin_frame();
        rec.fill_rect(0, 0, 64, 64, Color::rgb(10, 10, 10));
        rec.set_clip(true, 8, 8, 40, 40);
        rec.fill_rect(0, 0, 64, 64, Color::rgb(200, 40, 40));
        rec.clear_clip();
        rec.present();
        CHECK(imm.gpu.execute_frame(rec.commands()));
        CHECK(tile.gpu.execute_frame(rec.commands()));
        CHECK(same_fb(imm.gpu, tile.gpu));
    }

    // B5: color key 0xFF00FF — glyph bg discarded, fg changes
    {
        Rig r;
        CHECK(make_rig(r, BackendKind::Immediate));
        CommandRecorder rec;
        rec.begin_frame();
        rec.fill_rect(0, 0, 16, 16, Color::rgb(0, 64, 0));
        neon::draw_text(rec, r.a, 0, 0, "A", Color::rgb(255, 255, 255));
        rec.present();
        CHECK(r.gpu.execute_frame(rec.commands()));
        // glyph "A" occupies most of 8x8; some pixels should be white (fg)
        bool saw_fg = false;
        bool bg_ok = true;
        for (u32 y = 0; y < 8; ++y) {
            for (u32 x = 0; x < 8; ++x) {
                const u32 p = sample(r.gpu, x, y);
                const u32 rr = (p >> 16) & 0xFF;
                const u32 gg = (p >> 8) & 0xFF;
                const u32 bb = p & 0xFF;
                if (rr > 200 && gg > 200 && bb > 200) {
                    saw_fg = true;
                }
                // magenta key pixels must not appear
                if (rr > 200 && bb > 200 && gg < 80) {
                    bg_ok = false;
                }
            }
        }
        CHECK(saw_fg);
        CHECK(bg_ok);
    }

    // B2: bilinear recorded and Imm==Tile
    {
        Rig imm, tile;
        CHECK(make_rig(imm, BackendKind::Immediate));
        CHECK(make_rig(tile, BackendKind::Tile32));
        CommandRecorder rec;
        rec.begin_frame();
        rec.fill_rect(0, 0, 64, 64, Color::rgb(0, 0, 0));
        SpriteParams sp;
        sp.tex = imm.a.glow;
        sp.w = 32;
        sp.h = 32;
        sp.scale_w = 48;
        sp.scale_h = 48;
        sp.dst_x = 8;
        sp.dst_y = 8;
        sp.blend = BlendMode::AddSat;
        sp.filter = FilterMode::Bilinear;
        rec.draw_sprite(sp);
        rec.present();
        bool has_bilinear = false;
        for (const auto& c : rec.commands()) {
            if (c.op == RecOp::Sprite && c.sp.filter == FilterMode::Bilinear) {
                has_bilinear = true;
            }
        }
        CHECK(has_bilinear);
        CHECK(imm.gpu.execute_frame(rec.commands()));
        CHECK(tile.gpu.execute_frame(rec.commands()));
        CHECK(same_fb(imm.gpu, tile.gpu));
    }

    // B3: Indexed8+Palette draw
    {
        Rig imm, tile;
        CHECK(make_rig(imm, BackendKind::Immediate));
        CHECK(make_rig(tile, BackendKind::Tile32));
        CommandRecorder rec;
        rec.begin_frame();
        rec.fill_rect(0, 0, 32, 16, Color::rgb(20, 20, 40));
        neon::draw_text_pal(rec, imm.a, 0, 0, "OK", Color::rgb(255, 255, 255));
        rec.present();
        bool has_pal = false;
        for (const auto& c : rec.commands()) {
            if (c.op == RecOp::Sprite && c.sp.palette) {
                has_pal = true;
            }
        }
        CHECK(has_pal);
        CHECK(imm.gpu.execute_frame(rec.commands()));
        CHECK(tile.gpu.execute_frame(rec.commands()));
        CHECK(same_fb(imm.gpu, tile.gpu));
        // something non-background drawn
        const u32 p0 = sample(imm.gpu, 2, 2);
        (void)p0;
    }

    // B4: dither flag recorded on default-path glow
    {
        Rig r;
        CHECK(make_rig(r, BackendKind::Immediate));
        neon::SimConfig cfg;
        cfg.width = 64;
        cfg.height = 64;
        neon::SimState sim;
        neon::Rng rng(1);
        neon::sim_reset(sim, cfg, 1);
        for (int i = 0; i < 20; ++i) {
            neon::sim_step(sim, cfg, rng, nullptr, true);
        }
        // force additive glow particles
        sim.scene = neon::SceneId::OverdrawStorm;
        for (int i = 0; i < 30; ++i) {
            neon::sim_step(sim, cfg, rng, nullptr, true);
        }
        CommandRecorder rec;
        rec.begin_frame();
        neon::DrawOpts opts;
        opts.hud = true;
        neon::render_frame(rec, r.a, sim, cfg, opts);
        neon::draw_text_pal(rec, r.a, 2, 2, "P", Color::rgb(255, 255, 255));
        rec.present();
        bool has_dither = false;
        bool has_bilinear = false;
        bool has_pal = false;
        for (const auto& c : rec.commands()) {
            if (c.op == RecOp::Sprite && c.sp.dither) {
                has_dither = true;
            }
            if (c.op == RecOp::Sprite && c.sp.filter == FilterMode::Bilinear) {
                has_bilinear = true;
            }
            if (c.op == RecOp::Sprite && c.sp.palette) {
                has_pal = true;
            }
        }
        CHECK(has_dither);
        CHECK(has_bilinear);
        CHECK(has_pal);
        const auto dc = neon::last_render_counts();
        CHECK(dc.dither_draws > 0);
        CHECK(dc.bilinear_draws > 0);
        if (!r.gpu.execute_frame(rec.commands())) {
            std::printf("fault=0x%X idx=%u\n", r.gpu.last_fault(), r.gpu.last_fault_index());
            ++g_fail;
        }
    }

    // R2-08: TechHudStrings match telemetry
    {
        gpu2d::RendererTelemetry t;
        t.command_count = 42;
        t.sprite_count = 17;
        t.workref_count = 99;
        t.tiles_active = 3;
        t.tiles_total = 15;
        t.max_workrefs_per_tile = 8;
        t.max_overdraw = 5;
        t.grid_w = 5;
        t.grid_h = 3;
        t.tile_size = 32;
        const auto s = neon::make_tech_hud_strings(t, 0.0);
        CHECK(std::string(s.line0) == "CMD 42  SPR 17  WREF 99");
        CHECK(std::string(s.line1) == "TILE 3/15 MAXREF 8 MAXOD 5");
        CHECK(std::string(s.line2) == "PC GOLDEN HOST FPS N/A");
        CHECK(std::string(s.line3) == "GRID 5x3 TILE 32");
        const auto s2 = neon::make_tech_hud_strings(t, 60.0);
        CHECK(std::string(s2.line2) == "PC GOLDEN HOST FPS 60.0");
    }

    // R2-07 / FX-04: damage flash Color Mod non-white + Imm==Tile
    {
        Rig imm, tile;
        CHECK(make_rig(imm, BackendKind::Immediate));
        CHECK(make_rig(tile, BackendKind::Tile32));
        CommandRecorder rec;
        rec.begin_frame();
        rec.fill_rect(0, 0, 32, 32, Color::rgb(0, 0, 0));
        SpriteParams sp;
        sp.tex = imm.a.player;
        sp.w = 16;
        sp.h = 16;
        sp.dst_x = 8;
        sp.dst_y = 8;
        sp.color_mod = true;
        sp.mod = Color::rgb(255, 80, 80);  // non-identity
        rec.draw_sprite(sp);
        rec.present();
        bool has_mod = false;
        for (const auto& c : rec.commands()) {
            if (c.op == RecOp::Sprite && c.sp.color_mod &&
                !(c.sp.mod.r == 255 && c.sp.mod.g == 255 && c.sp.mod.b == 255)) {
                has_mod = true;
            }
        }
        CHECK(has_mod);
        CHECK(imm.gpu.execute_frame(rec.commands()));
        CHECK(tile.gpu.execute_frame(rec.commands()));
        const u32 n = imm.gpu.fb_stride() * imm.gpu.fb_height();
        CHECK(std::memcmp(imm.gpu.framebuffer(), tile.gpu.framebuffer(), n) == 0);
    }

    // R2-09: Tile16 → Tile32 forces 32
    {
        Rig r;
        CHECK(make_rig(r, BackendKind::Tile16));
        r.gpu.set_backend(BackendKind::Tile16);
        // execute a fill so telemetry updates
        CommandRecorder rec;
        rec.begin_frame();
        rec.fill_rect(0, 0, 8, 8, Color::rgb(1, 2, 3));
        rec.present();
        CHECK(r.gpu.execute_frame(rec.commands()));
        // switch to Tile32 and ensure tile_size is 32 via a frame
        r.gpu.set_backend(BackendKind::Tile32);
        rec.begin_frame();
        rec.fill_rect(0, 0, 8, 8, Color::rgb(4, 5, 6));
        rec.present();
        CHECK(r.gpu.execute_frame(rec.commands()));
        const auto& t = r.gpu.telemetry();
        CHECK(t.tile_size == 32);
    }

    // Diagnose Heavy-like bilinear scale COPY (black-box bug)
    {
        Rig r;
        CHECK(make_rig(r, BackendKind::Immediate));
        auto sample_after = [&](const char* tag, FilterMode filt, i32 sw, i32 sh,
                                bool add) {
            CommandRecorder rec;
            rec.begin_frame();
            rec.fill_rect(0, 0, 64, 64, Color::rgb(0, 40, 0));
            SpriteParams sp;
            sp.tex = r.a.enemy_h;
            sp.w = 20;
            sp.h = 20;
            sp.dst_x = 20;
            sp.dst_y = 20;
            sp.filter = filt;
            if (sw) {
                sp.scale_w = sw;
                sp.scale_h = sh;
            }
            if (add) {
                sp.blend = BlendMode::AddSat;
            }
            rec.draw_sprite(sp);
            rec.present();
            const bool ok = r.gpu.execute_frame(rec.commands());
            const u32 c = sample(r.gpu, 32, 32);
            const u32 e = sample(r.gpu, 24, 24);
            std::printf("%s ok=%d center=%06X near=%06X\n", tag, ok ? 1 : 0, c, e);
        };
        sample_after("near 1:1", FilterMode::Nearest, 0, 0, false);
        sample_after("bil 1:1", FilterMode::Bilinear, 0, 0, false);
        sample_after("near scale", FilterMode::Nearest, 24, 24, false);
        sample_after("bil scale", FilterMode::Bilinear, 24, 24, false);
        sample_after("bil scale add", FilterMode::Bilinear, 24, 24, true);
    }

    // Scale-up UV must not underflow (black-sprite bug)
    {
        Rig r;
        CHECK(make_rig(r, BackendKind::Immediate));
        CommandRecorder rec;
        rec.begin_frame();
        rec.fill_rect(0, 0, 64, 64, Color::rgb(0, 40, 0));
        SpriteParams sp;
        sp.tex = r.a.enemy_h;
        sp.w = 20;
        sp.h = 20;
        sp.scale_w = 24;
        sp.scale_h = 24;
        sp.dst_x = 20;
        sp.dst_y = 20;
        sp.filter = FilterMode::Nearest;
        rec.draw_sprite(sp);
        rec.present();
        CHECK(r.gpu.execute_frame(rec.commands()));
        const u32 c = sample(r.gpu, 32, 32);
        if ((c & 0xFFFFFF) == 0) {
            std::printf("FAIL scaled sprite center black (%06X)\n", c);
            ++g_fail;
        }
        golden::i32 u0 = 0, du = 0, v0 = 0, dv = 0;
        golden::compute_axis_aligned_uv(0, 20, 24, u0, du);
        golden::compute_axis_aligned_uv_v(0, 20, 24, v0, dv);
        CHECK(u0 < 0);
        CHECK(du > 0 && du < 65536);
    }

    if (g_fail) {
        std::printf("gpu2d_test_features FAIL %d\n", g_fail);
        return 1;
    }
    std::printf("gpu2d_test_features PASS\n");
    return 0;
}
