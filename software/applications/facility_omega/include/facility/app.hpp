#pragma once

#include "gpu2d/graphics_api.hpp"
#include "gpu2d/types.hpp"
#include "facility/environment.hpp"
#include "facility/gameplay.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace facility {

using gpu2d::i32;
using gpu2d::i64;
using gpu2d::u16;
using gpu2d::u32;
using gpu2d::u64;
using gpu2d::u8;

// MapTile = application/world tile. RenderTile = GPU tile (elsewhere).
inline constexpr u32 kMapTiles = 128;
inline constexpr u32 kMapTileSize = 32;
inline constexpr u32 kWorldW = kMapTiles * kMapTileSize;  // 4096
inline constexpr u32 kWorldH = kMapTiles * kMapTileSize;

struct SpriteBlob {
    std::string name;
    u32 width = 0;
    u32 height = 0;
    u32 stride = 0;
    u32 anchor_x = 0;
    u32 anchor_y = 0;
    bool rgb565 = false;
    bool indexed = false;
    std::vector<u8> pixels;
    std::string atlas_file;
    u32 atlas_width = 0, atlas_height = 0, atlas_stride = 0;
    u32 atlas_x = 0, atlas_y = 0;
};

struct TexRef {
    gpu2d::TextureId id{};
    u32 w = 0;
    u32 h = 0;
    u32 ax = 0;
    u32 ay = 0;
    u32 sx = 0, sy = 0;
};

class TexBank {
public:
    void put(const std::string& n, TexRef t) { items_.push_back({n, t}); }
    const TexRef* find(std::string_view n) const {
        for (const auto& it : items_) {
            if (it.name == n) {
                return &it.tex;
            }
        }
        return nullptr;
    }
    size_t size() const { return items_.size(); }

private:
    struct Item {
        std::string name;
        TexRef tex;
    };
    std::vector<Item> items_;
};

// Load processed runtime blobs (no PNG). Returns false on IO error.
bool load_runtime_sprites(const std::string& runtime_dir, std::vector<SpriteBlob>& out);
bool load_runtime_atlases(const std::string& runtime_dir, const std::vector<SpriteBlob>& sprites,
                         std::vector<SpriteBlob>& atlases);

// Deterministic xorshift32 (same family as NEON; independent state).
class Rng {
public:
    explicit Rng(u32 seed = 1234u) : s_(seed ? seed : 1u) {}
    u32 next() {
        u32 x = s_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        s_ = x;
        return s_;
    }
    u32 range(u32 lo, u32 hi) {
        if (hi <= lo) {
            return lo;
        }
        return lo + (next() % (hi - lo));
    }
    i32 irange(i32 lo, i32 hi) {
        return static_cast<i32>(range(static_cast<u32>(lo), static_cast<u32>(hi)));
    }
    void seed(u32 s) { s_ = s ? s : 1u; }
    u32 state() const { return s_; }

private:
    u32 s_;
};

enum class EnemyKind : u8 { Drone = 0, Crawler = 1, Tank = 2, Runner=3, Elite=4 };

struct Enemy {
    i32 world_x = 0;
    i32 world_y = 0;
    i32 hp = 1;
    EnemyKind kind = EnemyKind::Drone;
    bool alive = false;
    u8 flash = 0;
    u16 anim = 0;
    // Application-local Q8 displacement remainder; GPU coordinates stay integer.
    i32 motion_x = 0;
    i32 motion_y = 0;
};

struct Effect {
    i32 world_x = 0, world_y = 0;
    u32 life = 0;
    bool explosion = false;
};

struct Bullet {
    u32 life=90;
    i32 world_x = 0;
    i32 world_y = 0;
    i32 vx = 0;
    i32 vy = 0;
    bool alive = false;
    bool enemy = false;
};

struct XpGem {
    bool repair=false;
    i32 world_x = 0;
    i32 world_y = 0;
    i32 vx = 0;
    i32 vy = 0;
    u32 value = 1;
    bool alive = false;
};

enum class UpgradeId : u8 {
    PulseDamage = 0,
    FireRate = 1,
    ProjectileCount = 2,
};

// App state: world + camera + player + combat (vertical slice).
struct Player {
    i32 world_x = static_cast<i32>(kWorldW / 2);
    i32 world_y = static_cast<i32>(kWorldH / 2);
    u32 frame = 0;
    u8 dir = 0;  // 0 down 1 up 2 left 3 right
    bool moving = false;
    i32 hp = 100;
    i32 max_hp = 100;
    u32 level = 1;
    u32 xp = 0;
    u32 xp_need = 10;  // xp_to_level(1)
    u32 kills = 0;
    u32 hurt_timer = 0;
    u32 fire_cooldown = 0;
    i32 pulse_damage = 1;
    u32 fire_period = 18;  // frames between shots
    u32 projectile_count = 1;
};

struct Camera {
    i32 x = 0;
    i32 y = 0;
};

struct AppState {
    Player player;
    Camera cam;
    u64 frame = 0;
    u32 map_seed = 1;
    u32 sim_seed = 1234;
    Rng rng{1234};
    std::vector<u8> map;
    Environment environment;
    GameplayState gameplay;
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Effect> effects;
    std::vector<XpGem> xp_gems;
    u32 spawn_timer = 0;
    u32 enemy_count_target = 8;
    bool ready = false;
    u32 view_w = 640;
    u32 view_h = 360;
    bool level_up_pending = false;
    u32 upgrades_taken = 0;
};

void sim_reset(AppState& s, u32 seed);
void camera_follow(AppState& s, u32 view_w, u32 view_h);
void player_move(AppState& s, bool up, bool down, bool left, bool right, i32 speed = 3);
void player_hurt(AppState& s, i32 damage);
void set_viewport(AppState& s, u32 w, u32 h);
void spawn_enemy(AppState& s, EnemyKind kind);
void spawn_xp(AppState& s, i32 x, i32 y, u32 value);
void fire_pulse(AppState& s);
// keys: 0 up 1 down 2 left 3 right; while level-up pending sim freezes (I-05).
void sim_step(AppState& s, const bool* keys /*UDLR*/);
// I-07: choice 0/1/2 applies upgrade and resumes.
bool apply_upgrade(AppState& s, u32 choice);
inline u32 xp_to_level(u32 level) { return 6u + level * 4u; }

inline i32 world_to_screen_x(const AppState& s, i32 wx) { return wx - s.cam.x; }
inline i32 world_to_screen_y(const AppState& s, i32 wy) { return wy - s.cam.y; }

void visible_map_range(const AppState& s, u32 view_w, u32 view_h, i32 guard,
                       i32& tx0, i32& ty0, i32& tx1, i32& ty1);

// Is world point inside camera viewport + margin? (cull helper)
bool in_view(const AppState& s, i32 wx, i32 wy, i32 margin);

inline u32 player_walk_frame(const Player& p) { return (p.frame >> 3) & 1u; }
const char* player_sprite_name(const Player& p);
const char* enemy_sprite_name(EnemyKind k, u32 anim);

u32 live_enemy_count(const AppState& s);
u32 live_bullet_count(const AppState& s);
u32 hash_sim(const AppState& s);

void render_scene(gpu2d::GraphicsApi& api, const AppState& s, const TexBank& tex,
                  u32 view_w, u32 view_h);

}  // namespace facility
