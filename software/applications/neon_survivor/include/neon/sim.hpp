#pragma once

#include "gpu2d/types.hpp"

#include <cstdint>
#include <vector>

namespace neon {

using gpu2d::i32;
using gpu2d::u16;
using gpu2d::u32;
using gpu2d::u64;
using gpu2d::u8;

// Deterministic xorshift32 RNG (fixed seed mode).
class Rng {
public:
    explicit Rng(u32 seed = 1234u) : s_(seed ? seed : 1u) {}
    u32 next() {
        u32 x = s_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        s_ = x;
        return x;
    }
    u32 range(u32 lo, u32 hi) {  // [lo, hi)
        if (hi <= lo) {
            return lo;
        }
        return lo + (next() % (hi - lo));
    }
    i32 irange(i32 lo, i32 hi) { return static_cast<i32>(range(static_cast<u32>(lo), static_cast<u32>(hi))); }
    float frand() { return static_cast<float>(next() & 0xFFFFFF) / 16777216.0f; }
    u32 state() const { return s_; }
    void seed(u32 s) { s_ = s ? s : 1u; }

private:
    u32 s_;
};

enum class EnemyKind : u8 { Normal = 0, Fast = 1, Heavy = 2 };

struct Enemy {
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    i32 hp = 1;
    EnemyKind kind = EnemyKind::Normal;
    bool alive = false;
    u8 flash = 0;
};

struct Bullet {
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    bool enemy = false;
    bool alive = false;
    u8 kind = 0;  // 0 straight, 1 radial member, 2 spiral
};

struct Particle {
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    u16 life = 0;
    u16 max_life = 0;
    u8 r = 255, g = 255, b = 255;
    u8 mode = 0;  // 0 fade, 1 additive, 2 trail
    float scale = 1.0f;
};

struct Player {
    float x = 0, y = 0;
    i32 hp = 100;
    i32 max_hp = 100;
    u8 invuln = 0;
    u8 flash = 0;
};

enum class SceneId : u32 {
    Game = 0,
    SpriteStorm = 1,
    AlphaStorm = 2,
    BulletHell = 3,
    ScaleStorm = 4,
    OverdrawStorm = 5,
};

struct SimConfig {
    u32 seed = 1234;
    u32 tick_hz = 60;
    u32 width = 640;
    u32 height = 360;
};

struct SimState {
    u64 frame = 0;
    Player player{};
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Particle> particles;
    u32 score = 0;
    u32 kills = 0;
    u32 spawn_timer = 0;
    u32 attack_timer = 0;
    u32 spiral_angle = 0;
    bool paused = false;
    SceneId scene = SceneId::Game;
    u32 entity_hash = 0;
};

// FNV-1a over entity fields — deterministic simulation hash.
u32 hash_sim(const SimState& s);

void sim_reset(SimState& s, const SimConfig& cfg, u32 seed);
void sim_step(SimState& s, const SimConfig& cfg, Rng& rng, const bool* keys /* 4: UDLR */,
              bool script_idle);

}  // namespace neon
