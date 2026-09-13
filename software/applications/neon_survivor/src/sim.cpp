#include "neon/sim.hpp"

#include <cmath>
#include <cstring>

namespace neon {
namespace {

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline void fnv(u32& h, u32 v) {
    h ^= v;
    h *= 16777619u;
}

}  // namespace

u32 hash_sim(const SimState& s) {
    u32 h = 2166136261u;
    fnv(h, static_cast<u32>(s.frame));
    fnv(h, static_cast<u32>(s.player.x * 10));
    fnv(h, static_cast<u32>(s.player.y * 10));
    fnv(h, static_cast<u32>(s.player.hp));
    fnv(h, s.score);
    fnv(h, s.kills);
    for (const auto& e : s.enemies) {
        if (!e.alive) {
            continue;
        }
        fnv(h, static_cast<u32>(e.x));
        fnv(h, static_cast<u32>(e.y));
        fnv(h, static_cast<u32>(e.hp));
        fnv(h, static_cast<u32>(e.kind));
    }
    for (const auto& b : s.bullets) {
        if (!b.alive) {
            continue;
        }
        fnv(h, static_cast<u32>(b.x));
        fnv(h, static_cast<u32>(b.y));
        fnv(h, b.kind);
        fnv(h, b.enemy ? 1u : 0u);
    }
    return h;
}

void sim_reset(SimState& s, const SimConfig& cfg, u32 seed) {
    s = SimState{};
    s.frame = 0;
    s.player.x = cfg.width * 0.5f;
    s.player.y = cfg.height * 0.5f;
    s.player.hp = 100;
    s.player.max_hp = 100;
    s.enemies.reserve(2048);
    s.bullets.reserve(4096);
    s.particles.reserve(2048);
    s.spawn_timer = 0;
    s.attack_timer = 0;
    s.entity_hash = seed;
}

namespace {

void spawn_enemy(SimState& s, const SimConfig& cfg, Rng& rng, EnemyKind kind) {
    Enemy e;
    e.kind = kind;
    e.alive = true;
    const int edge = static_cast<int>(rng.range(0, 4));
    const float margin = 24.0f;
    switch (edge) {
        case 0:
            e.x = rng.range(0, cfg.width);
            e.y = -margin;
            break;
        case 1:
            e.x = rng.range(0, cfg.width);
            e.y = cfg.height + margin;
            break;
        case 2:
            e.x = -margin;
            e.y = rng.range(0, cfg.height);
            break;
        default:
            e.x = cfg.width + margin;
            e.y = rng.range(0, cfg.height);
            break;
    }
    if (kind == EnemyKind::Normal) {
        e.hp = 2;
    } else if (kind == EnemyKind::Fast) {
        e.hp = 1;
    } else {
        e.hp = 8;
    }
    // find dead slot
    for (auto& slot : s.enemies) {
        if (!slot.alive) {
            slot = e;
            return;
        }
    }
    if (s.enemies.size() < 2048) {
        s.enemies.push_back(e);
    }
}

void spawn_bullet(SimState& s, Bullet b) {
    for (auto& slot : s.bullets) {
        if (!slot.alive) {
            slot = b;
            return;
        }
    }
    if (s.bullets.size() < 8192) {
        s.bullets.push_back(b);
    }
}

void spawn_particle(SimState& s, Particle p) {
    for (auto& slot : s.particles) {
        if (!slot.life) {
            slot = p;
            return;
        }
    }
    if (s.particles.size() < 4096) {
        s.particles.push_back(p);
    }
}

void emit_explosion(SimState& s, Rng& rng, float x, float y, u8 r, u8 g, u8 b, int n) {
    for (int i = 0; i < n; ++i) {
        Particle p;
        p.x = x;
        p.y = y;
        const float a = rng.frand() * 6.2831853f;
        const float sp = 0.5f + rng.frand() * 2.5f;
        p.vx = std::cos(a) * sp;
        p.vy = std::sin(a) * sp;
        p.life = p.max_life = static_cast<u16>(12 + rng.range(0, 20));
        p.r = r;
        p.g = g;
        p.b = b;
        p.mode = (i % 3 == 0) ? 1 : 0;
        p.scale = 0.5f + rng.frand();
        spawn_particle(s, p);
    }
}

}  // namespace

void sim_step(SimState& s, const SimConfig& cfg, Rng& rng, const bool* keys, bool script_idle) {
    if (s.paused) {
        return;
    }
    ++s.frame;
    const float cx = cfg.width * 0.5f;
    const float cy = cfg.height * 0.5f;
    const float sp = 2.2f;
    if (!script_idle && keys) {
        if (keys[0]) {
            s.player.y -= sp;
        }
        if (keys[1]) {
            s.player.y += sp;
        }
        if (keys[2]) {
            s.player.x -= sp;
        }
        if (keys[3]) {
            s.player.x += sp;
        }
    } else if (script_idle) {
        // gentle circular idle path — deterministic
        s.player.x = cx + std::cos(static_cast<float>(s.frame) * 0.02f) * 40.0f;
        s.player.y = cy + std::sin(static_cast<float>(s.frame) * 0.02f) * 24.0f;
    }
    s.player.x = clampf(s.player.x, 8, static_cast<float>(cfg.width) - 8);
    s.player.y = clampf(s.player.y, 8, static_cast<float>(cfg.height) - 8);
    if (s.player.invuln) {
        --s.player.invuln;
    }
    if (s.player.flash) {
        --s.player.flash;
    }

    // Auto-attack: straight projectile toward nearest enemy
    if (++s.attack_timer >= 10) {
        s.attack_timer = 0;
        float best_d = 1e12f;
        float tx = cx, ty = 0;
        bool found = false;
        for (const auto& e : s.enemies) {
            if (!e.alive) {
                continue;
            }
            const float dx = e.x - s.player.x;
            const float dy = e.y - s.player.y;
            const float d = dx * dx + dy * dy;
            if (d < best_d) {
                best_d = d;
                tx = e.x;
                ty = e.y;
                found = true;
            }
        }
        if (found) {
            const float dx = tx - s.player.x;
            const float dy = ty - s.player.y;
            const float len = std::sqrt(dx * dx + dy * dy) + 1e-3f;
            Bullet b;
            b.x = s.player.x;
            b.y = s.player.y;
            b.vx = dx / len * 5.0f;
            b.vy = dy / len * 5.0f;
            b.alive = true;
            b.kind = 0;
            spawn_bullet(s, b);
        } else {
            Bullet b;
            b.x = s.player.x;
            b.y = s.player.y - 4;
            b.vx = 0;
            b.vy = -5.0f;
            b.alive = true;
            b.kind = 0;
            spawn_bullet(s, b);
        }
    }

    // Scene-specific pressure
    const u32 tick = static_cast<u32>(s.frame);
    if (s.scene == SceneId::Game) {
        if (++s.spawn_timer >= 30) {
            s.spawn_timer = 0;
            const u32 r = rng.range(0, 10);
            EnemyKind k = r < 6 ? EnemyKind::Normal : (r < 9 ? EnemyKind::Fast : EnemyKind::Heavy);
            spawn_enemy(s, cfg, rng, k);
        }
    } else if (s.scene == SceneId::SpriteStorm) {
        if (tick % 2 == 0) {
            for (int i = 0; i < 8; ++i) {
                spawn_enemy(s, cfg, rng, EnemyKind::Normal);
            }
        }
    } else if (s.scene == SceneId::AlphaStorm) {
        if (tick % 2 == 0) {
            Particle p;
            p.x = rng.range(0, cfg.width);
            p.y = rng.range(0, cfg.height);
            p.vx = (rng.frand() - 0.5f) * 2;
            p.vy = (rng.frand() - 0.5f) * 2;
            p.life = p.max_life = 40;
            p.r = 80;
            p.g = 200;
            p.b = 255;
            p.mode = 0;
            spawn_particle(s, p);
        }
        for (int i = 0; i < 4; ++i) {
            spawn_particle(s, Particle{});
        }
    } else if (s.scene == SceneId::BulletHell) {
        // radial + spiral from center
        if (tick % 8 == 0) {
            for (int i = 0; i < 12; ++i) {
                const float a = static_cast<float>(i) / 12.0f * 6.2831853f;
                Bullet b;
                b.x = cx;
                b.y = cy;
                b.vx = std::cos(a) * 2.0f;
                b.vy = std::sin(a) * 2.0f;
                b.alive = true;
                b.kind = 1;
                b.enemy = true;
                spawn_bullet(s, b);
            }
        }
        if (tick % 3 == 0) {
            s.spiral_angle += 7;
            const float a = static_cast<float>(s.spiral_angle) * 0.0174533f;
            for (int arm = 0; arm < 3; ++arm) {
                const float aa = a + arm * 2.094f;
                Bullet b;
                b.x = cx;
                b.y = cy;
                b.vx = std::cos(aa) * 1.6f;
                b.vy = std::sin(aa) * 1.6f;
                b.alive = true;
                b.kind = 2;
                b.enemy = true;
                spawn_bullet(s, b);
            }
        }
    } else if (s.scene == SceneId::ScaleStorm) {
        if (tick % 3 == 0) {
            spawn_enemy(s, cfg, rng, EnemyKind::Heavy);
            spawn_enemy(s, cfg, rng, EnemyKind::Normal);
        }
    } else if (s.scene == SceneId::OverdrawStorm) {
        if (tick % 2 == 0) {
            for (int i = 0; i < 6; ++i) {
                Particle p;
                p.x = cx + (rng.frand() - 0.5f) * 40;
                p.y = cy + (rng.frand() - 0.5f) * 40;
                p.vx = (rng.frand() - 0.5f);
                p.vy = (rng.frand() - 0.5f);
                p.life = p.max_life = 24;
                p.r = 255;
                p.g = 80;
                p.b = 200;
                p.mode = 1;
                p.scale = 1.5f + rng.frand();
                spawn_particle(s, p);
            }
        }
    }

    // Update enemies
    for (auto& e : s.enemies) {
        if (!e.alive) {
            continue;
        }
        const float dx = s.player.x - e.x;
        const float dy = s.player.y - e.y;
        const float len = std::sqrt(dx * dx + dy * dy) + 1e-3f;
        float es = 0.6f;
        if (e.kind == EnemyKind::Fast) {
            es = 1.4f;
        } else if (e.kind == EnemyKind::Heavy) {
            es = 0.35f;
        }
        e.x += dx / len * es;
        e.y += dy / len * es;
        if (e.flash) {
            --e.flash;
        }
        // collide player
        const float hit = (e.kind == EnemyKind::Heavy) ? 14.0f : 10.0f;
        if (len < hit && !s.player.invuln) {
            s.player.hp -= (e.kind == EnemyKind::Heavy) ? 8 : 3;
            s.player.invuln = 30;
            s.player.flash = 8;
            e.alive = false;
            emit_explosion(s, rng, e.x, e.y, 255, 80, 80, 6);
        }
    }

    // Update bullets
    for (auto& b : s.bullets) {
        if (!b.alive) {
            continue;
        }
        b.x += b.vx;
        b.y += b.vy;
        if (b.x < -16 || b.y < -16 || b.x > cfg.width + 16 || b.y > cfg.height + 16) {
            b.alive = false;
            continue;
        }
        if (b.enemy) {
            const float dx = b.x - s.player.x;
            const float dy = b.y - s.player.y;
            if (dx * dx + dy * dy < 36.0f && !s.player.invuln) {
                s.player.hp -= 2;
                s.player.invuln = 15;
                s.player.flash = 6;
                b.alive = false;
            }
            continue;
        }
        for (auto& e : s.enemies) {
            if (!e.alive) {
                continue;
            }
            const float dx = b.x - e.x;
            const float dy = b.y - e.y;
            const float hit = (e.kind == EnemyKind::Heavy) ? 12.0f : 8.0f;
            if (dx * dx + dy * dy < hit * hit) {
                --e.hp;
                e.flash = 6;
                b.alive = false;
                if (e.hp <= 0) {
                    e.alive = false;
                    ++s.kills;
                    s.score += (e.kind == EnemyKind::Heavy) ? 50 : (e.kind == EnemyKind::Fast ? 15 : 10);
                    const int n = (e.kind == EnemyKind::Heavy) ? 16 : 8;
                    emit_explosion(s, rng, e.x, e.y, 80, 255, 200, n);
                }
                break;
            }
        }
    }

    // Particles
    for (auto& p : s.particles) {
        if (!p.life) {
            continue;
        }
        p.x += p.vx;
        p.y += p.vy;
        p.vx *= 0.96f;
        p.vy *= 0.96f;
        --p.life;
    }

    // Cap enemies in game scene
    if (s.scene == SceneId::Game) {
        u32 alive = 0;
        for (const auto& e : s.enemies) {
            if (e.alive) {
                ++alive;
            }
        }
        if (alive > 80) {
            // skip spawn already handled
        }
    }

    s.entity_hash = hash_sim(s);
}

}  // namespace neon
