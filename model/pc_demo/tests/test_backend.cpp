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

struct Rig {
    GoldenBackend imm;
    GoldenBackend tile;
    neon::Assets a_imm;
    neon::Assets a_tile;
    ProfileDesc prof;
};

bool upload_all(GoldenBackend& g, neon::Assets& a) {
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
    return a.player.valid() && a.font.valid() && a.enemy_h.valid() && a.glow.valid();
}

bool make_rig(Rig& r, u32 w, u32 h) {
    r.prof.width = w;
    r.prof.height = h;
    r.prof.format = PixelFormat::RGB565;
    r.prof.tile_size = 32;
    if (!r.imm.init(r.prof) || !r.tile.init(r.prof)) {
        return false;
    }
    r.imm.set_backend(BackendKind::Immediate);
    r.tile.set_backend(BackendKind::Tile32);
    return upload_all(r.imm, r.a_imm) && upload_all(r.tile, r.a_tile);
}

u32 fb_hash(const u8* p, u32 n) {
    u32 h = 2166136261u;
    for (u32 i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

}  // namespace

int main() {
    // C-T1 / BACK-03: integrated scene Immediate == Tile
    {
        Rig r;
        CHECK(make_rig(r, 160, 90));
        neon::SimConfig cfg;
        cfg.width = 160;
        cfg.height = 90;
        neon::SimState sim;
        neon::Rng rng(42);
        neon::sim_reset(sim, cfg, 42);
        // deterministic mixed scene: fill + alpha + additive + scale + palette font + clip
        for (int i = 0; i < 8; ++i) {
            neon::sim_step(sim, cfg, rng, nullptr, true);
        }
        CommandRecorder rec;
        rec.begin_frame();
        neon::DrawOpts opts;
        opts.hud = true;
        opts.tech_hud = false;
        neon::render_frame(rec, r.a_imm, sim, cfg, opts);
        // force clip + bilinear + palette font path already in render
        rec.present();
        const auto cmds = rec.commands();
        if (!r.imm.execute_frame(cmds)) {
            std::printf("imm fault=0x%X idx=%u cmds=%zu\n", r.imm.last_fault(),
                        r.imm.last_fault_index(), cmds.size());
            const auto& c = cmds[r.imm.last_fault_index()];
            std::printf("  op=%d fx=%d fy=%d fw=%u fh=%u tex=%u dx=%d dy=%d w=%u h=%u sw=%d sh=%d blend=%u\n",
                        static_cast<int>(c.op), c.fx, c.fy, c.fw, c.fh, c.sp.tex.v, c.sp.dst_x,
                        c.sp.dst_y, c.sp.w, c.sp.h, c.sp.scale_w, c.sp.scale_h,
                        static_cast<unsigned>(c.sp.blend));
            ++g_fail;
        }
        if (!r.tile.execute_frame(cmds)) {
            std::printf("tile fault=0x%X cmds=%zu\n", r.tile.last_fault(), cmds.size());
            ++g_fail;
        }
        const u8* fi = r.imm.framebuffer();
        const u8* ft = r.tile.framebuffer();
        CHECK(fi && ft);
        const u32 n = r.imm.fb_stride() * r.imm.fb_height();
        if (fi && ft) {
            if (std::memcmp(fi, ft, n) != 0) {
                std::printf("FB mismatch imm=%08X tile=%08X n=%u\n", fb_hash(fi, n),
                            fb_hash(ft, n), n);
                ++g_fail;
            }
        }
    }

    // C-T4: Alpha/Additive/Scale/Palette/Clip in one scene
    {
        Rig r;
        CHECK(make_rig(r, 64, 64));
        CommandRecorder rec;
        rec.begin_frame();
        rec.fill_rect(0, 0, 64, 64, Color::rgb(10, 10, 20));
        SpriteParams sp;
        sp.tex = r.a_imm.glow;
        sp.dst_x = 8;
        sp.dst_y = 8;
        sp.w = 32;
        sp.h = 32;
        sp.blend = BlendMode::AddSat;
        rec.draw_sprite(sp);
        sp = SpriteParams{};
        sp.tex = r.a_imm.player;
        sp.dst_x = 20;
        sp.dst_y = 20;
        sp.w = 16;
        sp.h = 16;
        sp.blend = BlendMode::StraightAlpha;
        sp.global_alpha = 128;
        rec.draw_sprite(sp);
        sp = SpriteParams{};
        sp.tex = r.a_imm.enemy_h;
        sp.dst_x = 30;
        sp.dst_y = 30;
        sp.w = 20;
        sp.h = 20;
        sp.scale_w = 40;
        sp.scale_h = 40;
        rec.draw_sprite(sp);
        rec.set_clip(true, 0, 0, 48, 48);
        rec.fill_rect(0, 0, 64, 64, Color::rgb(255, 0, 0));
        rec.clear_clip();
        neon::draw_text(rec, r.a_imm, 2, 50, "PAL", Color::rgb(80, 255, 80));
        rec.present();
        CHECK(r.imm.execute_frame(rec.commands()));
        CHECK(r.tile.execute_frame(rec.commands()));
        const u32 n = r.imm.fb_stride() * r.imm.fb_height();
        CHECK(std::memcmp(r.imm.framebuffer(), r.tile.framebuffer(), n) == 0);
    }

    // C-T2 / BACK-04: runtime switch Immediate→Tile→Immediate
    {
        Rig r;
        CHECK(make_rig(r, 64, 64));
        GoldenBackend g;
        ProfileDesc p;
        p.width = 64;
        p.height = 64;
        CHECK(g.init(p));
        auto blobs = neon::build_procedural_assets();
        TextureDesc td;
        td.width = blobs.player.w;
        td.height = blobs.player.h;
        td.stride = blobs.player.stride;
        td.pixels = blobs.player.pixels.data();
        auto tid = g.create_texture(td);
        CHECK(tid.valid());
        for (int k = 0; k < 3; ++k) {
            g.set_backend(k == 1 ? BackendKind::Tile32 : BackendKind::Immediate);
            CommandRecorder rec;
            rec.begin_frame();
            rec.fill_rect(0, 0, 64, 64, Color::rgb(0, 0, 0));
            SpriteParams sp;
            sp.tex = tid;
            sp.dst_x = 16;
            sp.dst_y = 16;
            sp.w = 16;
            sp.h = 16;
            rec.draw_sprite(sp);
            rec.present();
            CHECK(g.execute_frame(rec.commands()));
        }
    }

    // invalid texture handle must fail (B-T3)
    {
        Rig r;
        CHECK(make_rig(r, 32, 32));
        CommandRecorder rec;
        rec.begin_frame();
        SpriteParams sp;
        sp.tex = TextureId{999};
        sp.w = 4;
        sp.h = 4;
        rec.draw_sprite(sp);
        rec.present();
        CHECK(!r.imm.execute_frame(rec.commands()));
    }

    if (g_fail) {
        std::printf("gpu2d_test_backend FAIL %d\n", g_fail);
        return 1;
    }
    std::printf("gpu2d_test_backend PASS\n");
    return 0;
}
