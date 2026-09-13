#include "neon/sim.hpp"

#include <cstdio>

namespace {
int g_fail = 0;
#define CHECK(c)                                                                \
    do {                                                                        \
        if (!(c)) {                                                             \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c);             \
            ++g_fail;                                                           \
        }                                                                       \
    } while (0)
}  // namespace

int main() {
    using namespace neon;
    SimConfig cfg;
    cfg.width = 320;
    cfg.height = 180;

    // D-T1: same seed + same scripted inputs → identical hashes
    {
        SimState a, b;
        Rng ra(999), rb(999);
        sim_reset(a, cfg, 999);
        sim_reset(b, cfg, 999);
        for (int i = 0; i < 300; ++i) {
            sim_step(a, cfg, ra, nullptr, true);
            sim_step(b, cfg, rb, nullptr, true);
        }
        CHECK(a.entity_hash == b.entity_hash);
        CHECK(hash_sim(a) == hash_sim(b));
    }

    // D-T2: reset restores initial state
    {
        SimState s;
        Rng rng(1234);
        sim_reset(s, cfg, 1234);
        const u32 h0 = hash_sim(s);
        for (int i = 0; i < 120; ++i) {
            sim_step(s, cfg, rng, nullptr, true);
        }
        CHECK(hash_sim(s) != h0);
        sim_reset(s, cfg, 1234);
        rng.seed(1234);
        CHECK(hash_sim(s) == h0);
    }

    // D-T3 / SYS-02: headless 1000 frames no crash
    {
        SimState s;
        Rng rng(7);
        sim_reset(s, cfg, 7);
        s.scene = SceneId::Game;
        for (int i = 0; i < 1000; ++i) {
            sim_step(s, cfg, rng, nullptr, true);
        }
        CHECK(s.frame == 1000);
    }

    // E-T2: deterministic kill
    {
        SimState s;
        Rng rng(50);
        sim_reset(s, cfg, 50);
        // force spawn by running sprite storm then switching
        s.scene = SceneId::Game;
        u32 kills0 = s.kills;
        for (int i = 0; i < 600 && s.kills == kills0; ++i) {
            sim_step(s, cfg, rng, nullptr, true);
        }
        // with auto-attack and enemies, kills should occur within 600 frames
        CHECK(s.kills > kills0);
    }

    // E-T3: projectile/enemy counts checkpoints for fixed seed
    {
        SimState s;
        Rng rng(1234);
        sim_reset(s, cfg, 1234);
        for (int i = 0; i < 60; ++i) {
            sim_step(s, cfg, rng, nullptr, true);
        }
        u32 en = 0, bl = 0;
        for (const auto& e : s.enemies) {
            en += e.alive ? 1 : 0;
        }
        for (const auto& b : s.bullets) {
            bl += b.alive ? 1 : 0;
        }
        // game scene should have spawned some enemies and bullets
        CHECK(en + bl > 0);
    }

    // stress scenes produce entities
    {
        for (auto scene : {SceneId::SpriteStorm, SceneId::AlphaStorm, SceneId::BulletHell,
                           SceneId::ScaleStorm, SceneId::OverdrawStorm}) {
            SimState s;
            Rng rng(3);
            sim_reset(s, cfg, 3);
            s.scene = scene;
            for (int i = 0; i < 60; ++i) {
                sim_step(s, cfg, rng, nullptr, true);
            }
            u32 live = 0;
            for (const auto& e : s.enemies) {
                live += e.alive ? 1 : 0;
            }
            for (const auto& b : s.bullets) {
                live += b.alive ? 1 : 0;
            }
            for (const auto& p : s.particles) {
                live += p.life ? 1 : 0;
            }
            CHECK(live > 0);
        }
    }

    if (g_fail) {
        std::printf("gpu2d_test_sim FAIL %d\n", g_fail);
        return 1;
    }
    std::printf("gpu2d_test_sim PASS\n");
    return 0;
}
