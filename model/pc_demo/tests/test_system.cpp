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
    // J-01 / SYS-01: 100 frames Immediate == Tile
    {
        App imm, tile;
        CHECK(imm.init(160, 90, 1234, BackendKind::Immediate));
        CHECK(tile.init(160, 90, 1234, BackendKind::Tile32));
        for (int f = 0; f < 100; ++f) {
            CHECK(imm.step_render());
            CHECK(tile.step_render());
            const u32 n = imm.gpu.fb_stride() * imm.gpu.fb_height();
            if (std::memcmp(imm.gpu.framebuffer(), tile.gpu.framebuffer(), n) != 0) {
                std::printf("frame %d FB mismatch cmds=%zu\n", f, imm.rec.commands().size());
                size_t shown = 0;
                for (const auto& c : imm.rec.commands()) {
                    if (c.op == gpu2d::RecOp::Sprite &&
                        (c.sp.scale_w || c.sp.blend != gpu2d::BlendMode::Copy || c.sp.color_mod ||
                         c.clip_en || c.sp.color_key)) {
                        std::printf("  sp t=%u %d,%d %ux%u sc=%d,%d bl=%u key=%d mod=%d\n",
                                    c.sp.tex.v, c.sp.dst_x, c.sp.dst_y, c.sp.w, c.sp.h,
                                    c.sp.scale_w, c.sp.scale_h,
                                    static_cast<unsigned>(c.sp.blend), c.sp.color_key ? 1 : 0,
                                    c.sp.color_mod ? 1 : 0);
                        if (++shown > 6) {
                            break;
                        }
                    }
                }
                ++g_fail;
                break;
            }
        }
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

    // XR G-T1..G-T4
    {
        App a;
        CHECK(a.init(160, 90, 42, BackendKind::Tile32));
        a.sim.scene = neon::SceneId::Game;
        for (int f = 0; f < 40; ++f) {
            CHECK(a.step_render());
        }
        const auto& t = a.gpu.telemetry();
        // G-T1: known grid for 160x90 tile32 → 5 x 3
        CHECK(t.grid_w == 5);
        CHECK(t.grid_h == 3);
        CHECK(t.tile_size == 32);
        // G-T2: inactive / low / high work tiles
        if (t.has_workref_map && !t.tile_workrefs.empty()) {
            u32 zero = 0, low = 0, high = 0;
            const size_t n = static_cast<size_t>(t.grid_w) * t.grid_h;
            for (size_t i = 0; i < n; ++i) {
                const u16 wc = t.tile_workrefs[i];
                if (wc == 0) {
                    ++zero;
                } else if (wc <= 4) {
                    ++low;
                } else if (wc >= 10) {
                    ++high;
                }
            }
            std::printf("xray grid %ux%u zero=%u low=%u high=%u\n", t.grid_w, t.grid_h, zero,
                        low, high);
            CHECK(zero >= 1 || low >= 1);  // corners often empty
            CHECK(low + high >= 1);
        }
        // G-T3: X-Ray does not alter sim
        const u32 h0 = neon::hash_sim(a.sim);
        CommandRecorder rec;
        rec.begin_frame();
        neon::DrawOpts opts;
        opts.hud = true;
        opts.xray = true;
        opts.tile_size = 32;
        opts.tile_mode = true;
        const auto tel = a.gpu.telemetry().view();
        opts.tel = &tel;
        neon::render_frame(rec, a.assets, a.sim, a.cfg, opts);
        CHECK(neon::hash_sim(a.sim) == h0);
        // G-T4: base scene FB before overlay execute unchanged after sim-only
        const u32 fb0 = hash_fb(a.gpu);
        rec.present();
        CHECK(a.gpu.execute_frame(rec.commands()));
        // overlay changes pixels
        CHECK(hash_fb(a.gpu) != fb0);
    }

    if (g_fail) {
        std::printf("gpu2d_test_system FAIL %d\n", g_fail);
        return 1;
    }
    std::printf("gpu2d_test_system PASS\n");
    return 0;
}
