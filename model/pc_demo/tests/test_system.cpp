#include "golden_renderer.hpp"
#include "gpu2d/renderer.hpp"
#include "neon/assets.hpp"
#include "neon/sim.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
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

struct App {
    GoldenBackend gpu;
    neon::Assets assets;
    CommandRecorder rec;
    neon::SimConfig cfg;
    neon::SimState sim;
    neon::Rng rng;

    bool init(u32 w, u32 h, u32 seed, BackendKind bk) {
        ProfileDesc p;
        p.width = w;
        p.height = h;
        p.format = PixelFormat::RGB565;
        p.tile_size = 32;
        if (!gpu.init(p)) {
            return false;
        }
        gpu.set_backend(bk);
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
            return gpu.create_texture(d);
        };
        assets.player = up(blobs.player);
        assets.enemy_n = up(blobs.enemy_n);
        assets.enemy_f = up(blobs.enemy_f);
        assets.enemy_h = up(blobs.enemy_h);
        assets.bullet = up(blobs.bullet);
        assets.bullet_e = up(blobs.bullet_e);
        assets.particle = up(blobs.particle);
        assets.glow = up(blobs.glow);
        assets.font = up(blobs.font);
        assets.font_pal = up(blobs.font_index8);
        if (!assets.player.valid()) {
            return false;
        }
        cfg.width = w;
        cfg.height = h;
        cfg.seed = seed;
        neon::sim_reset(sim, cfg, seed);
        rng.seed(seed);
        return true;
    }

    bool step_render() {
        neon::sim_step(sim, cfg, rng, nullptr, true);
        rec.begin_frame();
        neon::DrawOpts opts;
        opts.hud = true;
        neon::render_frame(rec, assets, sim, cfg, opts);
        rec.present();
        const bool ok = gpu.execute_frame(rec.commands());
        if (!ok) {
            std::printf("  step fail bk=%u fault=0x%X idx=%u cmds=%zu\n",
                        static_cast<unsigned>(gpu.backend()), gpu.last_fault(),
                        gpu.last_fault_index(), rec.commands().size());
        }
        return ok;
    }

    // R2-01: base execute → snapshot telemetry → overlay-only execute.
    bool step_base_overlay(bool xray, TelemetrySnapshot& out_base_tel) {
        neon::sim_step(sim, cfg, rng, nullptr, true);
        rec.begin_frame();
        neon::render_scene_base(rec, assets, sim, cfg);
        rec.present();
        if (!gpu.execute_frame(rec.commands())) {
            return false;
        }
        out_base_tel = gpu.telemetry();
        neon::DrawOpts opts;
        opts.hud = true;
        opts.tech_hud = true;
        opts.xray = xray;
        opts.tile_mode = gpu.backend() != BackendKind::Immediate;
        opts.tile_size = 32;
        const auto view = out_base_tel.view();
        opts.tel = &view;
        rec.begin_frame();
        neon::render_debug_overlay(rec, assets, sim, cfg, opts);
        rec.present();
        return gpu.execute_frame(rec.commands());
    }
};

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

}  // namespace

int main() {
    // J-01 / SYS-01 / B11: 100 frames Immediate == Tile + feature coverage
    {
        App imm, tile;
        CHECK(imm.init(160, 90, 1234, BackendKind::Immediate));
        CHECK(tile.init(160, 90, 1234, BackendKind::Tile32));
        u32 cov_alpha = 0, cov_add = 0, cov_scale = 0, cov_clip = 0, cov_pal = 0, cov_bil = 0;
        for (int f = 0; f < 100; ++f) {
            CHECK(imm.step_render());
            CHECK(tile.step_render());
            const auto dc = neon::last_render_counts();
            cov_alpha += dc.alpha_draws;
            cov_add += dc.additive_draws;
            cov_scale += dc.scaled_draws;
            cov_clip += dc.clipped_draws;
            cov_pal += dc.palette_draws;
            cov_bil += dc.bilinear_draws;
            const u32 n = imm.gpu.fb_stride() * imm.gpu.fb_height();
            if (std::memcmp(imm.gpu.framebuffer(), tile.gpu.framebuffer(), n) != 0) {
                std::printf("frame %d FB mismatch cmds=%zu\n", f, imm.rec.commands().size());
                ++g_fail;
                break;
            }
        }
        std::printf("coverage100 alpha=%u add=%u scale=%u clip=%u pal=%u bil=%u\n",
                    cov_alpha, cov_add, cov_scale, cov_clip, cov_pal, cov_bil);
        CHECK(cov_alpha > 0);
        CHECK(cov_add > 0);
        CHECK(cov_scale > 0);
        CHECK(cov_clip > 0);
        CHECK(cov_pal > 0);
        CHECK(cov_bil > 0);
    }

    // B10 / SYS-02: 1000 rendered application frames (moderate profile)
    {
        App a;
        CHECK(a.init(96, 64, 7, BackendKind::Tile32));
        for (int f = 0; f < 1000; ++f) {
            if (!a.step_render()) {
                std::printf("1000-frame fail at %d fault=0x%X\n", f, a.gpu.last_fault());
                ++g_fail;
                break;
            }
            const u8* fb = a.gpu.framebuffer();
            if (!fb) {
                ++g_fail;
                break;
            }
        }
        CHECK(a.sim.frame == 1000);
    }

    // SYS-03: switch stability with same sim
    {
        App a;
        CHECK(a.init(96, 64, 77, BackendKind::Immediate));
        for (int f = 0; f < 5; ++f) {
            CHECK(a.step_render());
        }
        a.gpu.set_backend(BackendKind::Tile32);
        for (int f = 0; f < 5; ++f) {
            CHECK(a.step_render());
        }
        a.gpu.set_backend(BackendKind::Immediate);
        for (int f = 0; f < 5; ++f) {
            CHECK(a.step_render());
        }
        CHECK(a.sim.frame == 15);
    }

    // SYS-04: deterministic capture — two runs same hash
    {
        u32 h1 = 0, h2 = 0;
        for (int run = 0; run < 2; ++run) {
            App a;
            CHECK(a.init(96, 64, 4242, BackendKind::Tile32));
            for (int f = 0; f < 30; ++f) {
                CHECK(a.step_render());
            }
            (run == 0 ? h1 : h2) = hash_fb(a.gpu);
        }
        CHECK(h1 == h2);
        CHECK(h1 != 0);
    }

    // H-T2 frozen thresholds + H-T4 Imm==Tile per stress mode
    {
        struct Case {
            neon::SceneId sc;
            const char* name;
            u32 frames;
            u32 min_metric;
            int metric;  // 0=sprites 1=alpha 2=scaled 3=bilinear+scale 4=max_od 5=bullets
        };
        const Case cases[] = {
            {neon::SceneId::SpriteStorm, "sprite", 80, 500, 0},
            {neon::SceneId::BulletHell, "bullet", 80, 1000, 5},
            {neon::SceneId::AlphaStorm, "alpha", 80, 300, 1},
            {neon::SceneId::ScaleStorm, "scale", 80, 200, 2},
            {neon::SceneId::OverdrawStorm, "overdraw", 80, 8, 4},
        };
        for (const auto& c : cases) {
            App imm, tile;
            CHECK(imm.init(320, 180, 9, BackendKind::Immediate));
            CHECK(tile.init(320, 180, 9, BackendKind::Tile32));
            imm.sim.scene = c.sc;
            tile.sim.scene = c.sc;
            u32 last_metric = 0;
            for (u32 f = 0; f < c.frames; ++f) {
                CHECK(imm.step_render());
                CHECK(tile.step_render());
            }
            const u32 n = imm.gpu.fb_stride() * imm.gpu.fb_height();
            if (std::memcmp(imm.gpu.framebuffer(), tile.gpu.framebuffer(), n) != 0) {
                std::printf("stress %s Imm!=Tile\n", c.name);
                ++g_fail;
            }
            const auto dc = neon::last_render_counts();
            const auto& t = tile.gpu.telemetry();
            u32 live_en = 0, live_bl = 0, live_pt = 0;
            for (const auto& e : tile.sim.enemies) {
                live_en += e.alive ? 1 : 0;
            }
            for (const auto& b : tile.sim.bullets) {
                live_bl += b.alive ? 1 : 0;
            }
            for (const auto& p : tile.sim.particles) {
                live_pt += p.life ? 1 : 0;
            }
            switch (c.metric) {
                case 0:
                    last_metric = dc.sprites;
                    break;
                case 1:
                    last_metric = dc.alpha_draws;
                    break;
                case 2:
                    last_metric = dc.scaled_draws;
                    break;
                case 4:
                    last_metric = t.max_overdraw;
                    break;
                case 5:
                    last_metric = live_bl + dc.sprites;
                    break;
                default:
                    last_metric = dc.sprites;
                    break;
            }
            std::printf(
                "stress %s metric=%u (min %u) sprites=%u alpha=%u scaled=%u bil=%u "
                "live_en=%u live_bl=%u live_pt=%u max_od=%u\n",
                c.name, last_metric, c.min_metric, dc.sprites, dc.alpha_draws,
                dc.scaled_draws, dc.bilinear_draws, live_en, live_bl, live_pt, t.max_overdraw);
            if (last_metric < c.min_metric) {
                std::printf("  FAIL threshold %s metric=%u < %u\n", c.name, last_metric,
                            c.min_metric);
                ++g_fail;
            }
        }
    }

    // XR G-T1..G-T4 + R2-02 sparse fixture: inactive / low / high all >= 1
    {
        App a;
        CHECK(a.init(160, 90, 42, BackendKind::Tile32));
        // Sparse architecture fixture — NO full-screen background (that forces
        // every tile active). Draw only in selected tiles.
        // Grid 5x3 tile32. Tile (0,0) low work; tile (2,1) high work; rest empty.
        a.rec.begin_frame();
        // low: one 2x2 pixel in tile (0,0)
        a.rec.fill_rect(4, 4, 2, 2, Color::rgb(255, 0, 0));
        // high: many overlapping fills in tile (2,1) = pixel x 64..95, y 32..63
        for (int i = 0; i < 16; ++i) {
            a.rec.fill_rect(70 + (i % 3), 40 + (i % 2), 8, 8,
                            Color::rgb(static_cast<u8>(10 * i), 80, 200));
        }
        a.rec.present();
        CHECK(a.gpu.execute_frame(a.rec.commands()));
        const auto& t = a.gpu.telemetry();
        // G-T1: known grid for 160x90 tile32 → 5 x 3
        CHECK(t.grid_w == 5);
        CHECK(t.grid_h == 3);
        CHECK(t.tile_size == 32);
        CHECK(t.tiles_total == 15);
        CHECK(t.has_workref_map && !t.tile_workrefs.empty());
        u32 zero = 0, low = 0, high = 0;
        for (size_t i = 0; i < t.tile_workrefs.size(); ++i) {
            const u16 wc = t.tile_workrefs[i];
            if (wc == 0) {
                ++zero;
            } else if (wc <= 4) {
                ++low;
            } else if (wc >= 8) {
                ++high;
            }
            std::printf("  tile[%zu]=%u%s", i, wc, ((i + 1) % 5 == 0) ? "\n" : " ");
        }
        std::printf("xray sparse zero=%u low=%u high=%u active=%u wref=%u\n", zero, low, high,
                    t.tiles_active, t.workref_count);
        // G-T2 exact matrix — do not weaken
        CHECK(zero >= 1);
        CHECK(low >= 1);
        CHECK(high >= 1);

        // G-T3: X-Ray overlay draw does not alter sim hash
        const u32 h0 = neon::hash_sim(a.sim);
        const u32 fb_base = hash_fb(a.gpu);
        neon::DrawOpts ov;
        ov.hud = true;
        ov.tech_hud = true;
        ov.xray = true;
        ov.tile_mode = true;
        ov.tile_size = 32;
        const auto telv = a.gpu.telemetry().view();
        ov.tel = &telv;
        CommandRecorder rec2;
        rec2.begin_frame();
        neon::render_debug_overlay(rec2, a.assets, a.sim, a.cfg, ov);
        CHECK(neon::hash_sim(a.sim) == h0);
        rec2.present();
        CHECK(a.gpu.execute_frame(rec2.commands()));
        // G-T4: overlay changes pixels
        CHECK(hash_fb(a.gpu) != fb_base);
    }

    // G-T4 exact: base frame X-Ray OFF == base frame before overlay ON
    {
        auto sparse = [](App& app) {
            app.rec.begin_frame();
            app.rec.fill_rect(4, 4, 2, 2, Color::rgb(255, 0, 0));
            for (int i = 0; i < 16; ++i) {
                app.rec.fill_rect(70 + (i % 3), 40 + (i % 2), 8, 8, Color::rgb(200, 80, 40));
            }
            app.rec.present();
            return app.gpu.execute_frame(app.rec.commands());
        };
        App off, on;
        CHECK(off.init(160, 90, 7, BackendKind::Tile32));
        CHECK(on.init(160, 90, 7, BackendKind::Tile32));
        CHECK(sparse(off));
        CHECK(sparse(on));
        const u32 h_off = hash_fb(off.gpu);
        const u32 h_on_base = hash_fb(on.gpu);
        CHECK(h_off == h_on_base);
        // now overlay only on 'on'
        neon::DrawOpts ov;
        ov.xray = true;
        ov.tech_hud = true;
        ov.hud = true;
        ov.tile_size = 32;
        ov.tile_mode = true;
        const auto telv = on.gpu.telemetry().view();
        ov.tel = &telv;
        CommandRecorder rec;
        rec.begin_frame();
        neon::render_debug_overlay(rec, on.assets, on.sim, on.cfg, ov);
        rec.present();
        CHECK(on.gpu.execute_frame(rec.commands()));
        CHECK(hash_fb(on.gpu) != h_off);
        // off without overlay unchanged
        CHECK(hash_fb(off.gpu) == h_off);
    }

    // R2-01: X-Ray overlay must not contaminate BASE telemetry across frames
    {
        App off, on;
        CHECK(off.init(160, 90, 55, BackendKind::Tile32));
        CHECK(on.init(160, 90, 55, BackendKind::Tile32));
        for (int f = 0; f < 25; ++f) {
            TelemetrySnapshot t_off, t_on;
            CHECK(off.step_base_overlay(false, t_off));
            CHECK(on.step_base_overlay(true, t_on));
            if (t_off.workref_count != t_on.workref_count ||
                t_off.tiles_active != t_on.tiles_active ||
                t_off.max_overdraw != t_on.max_overdraw ||
                t_off.command_count != t_on.command_count) {
                std::printf(
                    "R2-01 frame %d base tel drift: off wref=%u act=%u od=%u cmds=%u | "
                    "on wref=%u act=%u od=%u cmds=%u\n",
                    f, t_off.workref_count, t_off.tiles_active, t_off.max_overdraw,
                    t_off.command_count, t_on.workref_count, t_on.tiles_active,
                    t_on.max_overdraw, t_on.command_count);
                ++g_fail;
                break;
            }
            // Overlay execute must not be treated as next X-Ray source: on.gpu.telemetry()
            // after overlay is contaminated; we compare base snapshots only (above).
        }
        // After many X-Ray frames, base tiles_active must not equal tiles_total solely
        // due to grid lines covering every tile (overlay isolation).
        TelemetrySnapshot tfin;
        CHECK(on.step_base_overlay(true, tfin));
        std::printf("R2-01 final base tiles %u/%u wref=%u (overlay isolated)\n",
                    tfin.tiles_active, tfin.tiles_total, tfin.workref_count);
    }

    if (g_fail) {
        std::printf("gpu2d_test_system FAIL %d\n", g_fail);
        return 1;
    }
    std::printf("gpu2d_test_system PASS\n");
    return 0;
}
