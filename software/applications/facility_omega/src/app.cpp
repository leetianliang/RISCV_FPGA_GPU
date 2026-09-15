#include "facility/app.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace facility {
namespace {

// Binary restoring sqrt: bounded by 16 iterations for a 32-bit argument.
u32 distance_root(u32 value) {
    u32 result = 0, bit = 1u << 30;
    while (bit > value) bit >>= 2;
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result ? result : 1;
}

void emit_effect(AppState& s, i32 x, i32 y, bool explosion) {
    Effect fx{x, y, explosion ? 18u : 7u, explosion};
    for (auto& old : s.effects) {
        if (!old.life) { old = fx; return; }
    }
    if (s.effects.size() < 96) s.effects.push_back(fx);
}

u32 map_index(const AppState& s, u32 tx, u32 ty) {
    if (tx >= kMapTiles || ty >= kMapTiles) {
        return 0;
    }
    return s.map[static_cast<size_t>(ty) * kMapTiles + tx];
}

const char* floor_name(u32 idx) {
    static const char* kFloors[8] = {"floor_00", "floor_01", "floor_02", "floor_03",
                                     "floor_04", "floor_05", "floor_06", "floor_07"};
    return kFloors[idx & 7u];
}

bool read_file(const std::string& path, std::vector<u8>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(0, std::ios::end);
    const auto n = f.tellg();
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(n));
    if (n > 0) {
        f.read(reinterpret_cast<char*>(out.data()), n);
    }
    return true;
}

}  // namespace

bool load_runtime_sprites(const std::string& runtime_dir, std::vector<SpriteBlob>& out) {
    // Minimal JSON scan: name / width / height / stride / anchor / format / runtime_file
    std::ifstream mf(runtime_dir + "/facility_omega_assets.json");
    if (!mf) {
        return false;
    }
    std::stringstream ss;
    ss << mf.rdbuf();
    const std::string js = ss.str();
    size_t pos = 0;
    while (true) {
        const size_t npos = js.find("\"name\":", pos);
        if (npos == std::string::npos) {
            break;
        }
        SpriteBlob b;
        auto grab_str = [&](const char* key) -> std::string {
            const size_t k = js.find(key, npos);
            if (k == std::string::npos || k > npos + 800) {
                return "";
            }
            const size_t q1 = js.find('"', js.find(':', k) + 1);
            const size_t q2 = js.find('"', q1 + 1);
            if (q1 == std::string::npos || q2 == std::string::npos) {
                return "";
            }
            return js.substr(q1 + 1, q2 - q1 - 1);
        };
        auto grab_u32 = [&](const char* key) -> u32 {
            const size_t k = js.find(key, npos);
            if (k == std::string::npos || k > npos + 800) {
                return 0;
            }
            return static_cast<u32>(std::strtoul(js.c_str() + js.find(':', k) + 1, nullptr, 10));
        };
        b.name = grab_str("\"name\"");
        b.width = grab_u32("\"width\"");
        b.height = grab_u32("\"height\"");
        b.stride = grab_u32("\"stride\"");
        b.anchor_x = grab_u32("\"anchor_x\"");
        b.anchor_y = grab_u32("\"anchor_y\"");
        b.atlas_file = grab_str("\"atlas_file\"");
        b.atlas_width = grab_u32("\"atlas_width\"");
        b.atlas_height = grab_u32("\"atlas_height\"");
        b.atlas_stride = grab_u32("\"atlas_stride\"");
        b.atlas_x = grab_u32("\"atlas_x\"");
        b.atlas_y = grab_u32("\"atlas_y\"");
        const std::string fmt = grab_str("\"format\"");
        b.rgb565 = (fmt == "rgb565");
        b.indexed = (fmt == "index8");
        const std::string rf = grab_str("\"runtime_file\"");
        if (b.name.empty() || rf.empty() || !read_file(runtime_dir + "/" + rf, b.pixels)) {
            return false;
        }
        if (!b.width || !b.height || b.pixels.size() != static_cast<size_t>(b.stride)*b.height ||
            b.atlas_x+b.width>b.atlas_width || b.atlas_y+b.height>b.atlas_height) return false;
        out.push_back(std::move(b));
        pos = npos + 7;
    }
    return !out.empty();
}

bool load_runtime_atlases(const std::string& dir, const std::vector<SpriteBlob>& sprites,
                         std::vector<SpriteBlob>& atlases) {
    atlases.clear();
    for (const auto& s : sprites) {
        bool found = false;
        for (const auto& a : atlases) if (a.name == s.atlas_file) found = true;
        if (found) continue;
        SpriteBlob a;
        a.name=s.atlas_file; a.width=s.atlas_width; a.height=s.atlas_height;
        a.stride=s.atlas_stride; a.rgb565=s.rgb565;
        if (a.name.empty() || !a.width || !a.height ||
            !read_file(dir+"/"+a.name,a.pixels) || a.pixels.size()!=static_cast<size_t>(a.stride)*a.height)
            return false;
        atlases.push_back(std::move(a));
    }
    return !atlases.empty();
}

void sim_reset(AppState& s, u32 seed) {
    s = AppState{};
    s.map_seed = seed ? seed : 1u;
    s.map.resize(static_cast<size_t>(kMapTiles) * kMapTiles);
    // R7: base floors dominate; specials only sparse / authored.
    u32 x = s.map_seed;
    for (u32 ty = 0; ty < kMapTiles; ++ty) {
        for (u32 tx = 0; tx < kMapTiles; ++tx) {
            x = x * 1664525u + 1013904223u;
            u8 t = 0;
            const u32 r = (x >> 16) % 100u;
            // Calm continuous panels; wear is uncommon, service grates form runs.
            if (r > 96) t = 1;
            else if (r > 94) t = 6;
            else if (r > 92) t = 3;
            if (ty % 32 == 8 || ty % 32 == 24) t = 2;
            if ((tx % 32 == 7 || tx % 32 == 25) && ty % 32 >= 10 && ty % 32 <= 22)
                t = (ty & 1u) ? 4 : 5;
            if (ty % 32 == 10 && tx % 32 >= 14 && tx % 32 <= 17) t = 7;
            s.map[static_cast<size_t>(ty) * kMapTiles + tx] = t;
        }
    }
    s.player = Player{};
    s.player.world_x = static_cast<i32>(kWorldW / 2);
    s.player.world_y = static_cast<i32>(kWorldH / 2);
    s.player.xp_need = xp_to_level(s.player.level);
    s.sim_seed = seed ? seed : 1u;
    s.rng.seed(s.sim_seed);
    s.enemies.clear();
    s.enemies.reserve(256);
    s.bullets.clear();
    s.bullets.reserve(256);
    s.effects.reserve(96);
    s.xp_gems.clear();
    s.xp_gems.reserve(512);
    s.spawn_timer = 0;
    s.enemy_count_target = 6;
    s.frame = 0;
    s.level_up_pending = false;
    s.upgrades_taken = 0;
    s.ready = true;
}

void camera_follow(AppState& s, u32 view_w, u32 view_h) {
    i32 cx = s.player.world_x - static_cast<i32>(view_w / 2);
    i32 cy = s.player.world_y - static_cast<i32>(view_h / 2);
    const i32 max_x = static_cast<i32>(kWorldW) - static_cast<i32>(view_w);
    const i32 max_y = static_cast<i32>(kWorldH) - static_cast<i32>(view_h);
    if (cx < 0) {
        cx = 0;
    }
    if (cy < 0) {
        cy = 0;
    }
    if (max_x > 0 && cx > max_x) {
        cx = max_x;
    }
    if (max_y > 0 && cy > max_y) {
        cy = max_y;
    }
    s.cam.x = cx;
    s.cam.y = cy;
}

void player_move(AppState& s, bool up, bool down, bool left, bool right, i32 speed) {
    s.player.moving = up || down || left || right;
    if (up) {
        s.player.world_y -= speed;
        s.player.dir = 1;
    }
    if (down) {
        s.player.world_y += speed;
        s.player.dir = 0;
    }
    if (left) {
        s.player.world_x -= speed;
        s.player.dir = 2;
    }
    if (right) {
        s.player.world_x += speed;
        s.player.dir = 3;
    }
    // world boundary
    if (s.player.world_x < 8) {
        s.player.world_x = 8;
    }
    if (s.player.world_y < 8) {
        s.player.world_y = 8;
    }
    if (s.player.world_x > static_cast<i32>(kWorldW) - 8) {
        s.player.world_x = static_cast<i32>(kWorldW) - 8;
    }
    if (s.player.world_y > static_cast<i32>(kWorldH) - 8) {
        s.player.world_y = static_cast<i32>(kWorldH) - 8;
    }
    if (s.player.moving) {
        s.player.frame++;
    } else {
        s.player.frame = 0;
    }
}

void sim_step(AppState& s, const bool* keys) {
    // I-05: freeze world while upgrade choice is open.
    if (s.level_up_pending) {
        return;
    }
    ++s.frame;
    for (auto& fx : s.effects) if (fx.life) --fx.life;
    player_move(s, keys && keys[0], keys && keys[1], keys && keys[2], keys && keys[3]);
    if (s.player.hurt_timer > 0) {
        --s.player.hurt_timer;
    }
    // Spawn outside current viewport (G-02)
    if (++s.spawn_timer >= 45) {
        s.spawn_timer = 0;
        const u32 live = live_enemy_count(s);
        if (live < s.enemy_count_target) {
            const u32 r = s.rng.range(0, 10);
            EnemyKind k = r < 5 ? EnemyKind::Drone : (r < 8 ? EnemyKind::Crawler : EnemyKind::Tank);
            spawn_enemy(s, k);
            if (s.frame > 600 && s.enemy_count_target < 40) {
                ++s.enemy_count_target;
            }
        }
    }
    // Auto Pulse Shot (H-01..H-02)
    if (s.player.fire_cooldown > 0) {
        --s.player.fire_cooldown;
    } else {
        fire_pulse(s);
    }
    // Enemies chase player
    for (auto& e : s.enemies) {
        if (!e.alive) {
            continue;
        }
        const i32 dx = s.player.world_x - e.world_x;
        const i32 dy = s.player.world_y - e.world_y;
        i32 dist2 = dx * dx + dy * dy;
        if (dist2 < 1) {
            dist2 = 1;
        }
        i32 sp = 2;
        i32 hit_r = 14;
        if (e.kind == EnemyKind::Crawler) {
            sp = 1;
            hit_r = 16;
        } else if (e.kind == EnemyKind::Tank) {
            sp = 1;
            hit_r = 22;
        }
        // integer step toward player (deterministic, no float wall-clock)
        if (dist2 > 0) {
            const i32 len = static_cast<i32>(distance_root(static_cast<u32>(dist2)));
            e.motion_x += (dx * sp * 256) / len;
            e.motion_y += (dy * sp * 256) / len;
            e.world_x += e.motion_x / 256;
            e.world_y += e.motion_y / 256;
            e.motion_x %= 256;
            e.motion_y %= 256;
        }
        ++e.anim;
        if (e.flash) {
            --e.flash;
        }
        // touch damage
        const i32 tdx = e.world_x - s.player.world_x;
        const i32 tdy = e.world_y - s.player.world_y;
        if (tdx * tdx + tdy * tdy < hit_r * hit_r && s.player.hurt_timer == 0) {
            player_hurt(s, e.kind == EnemyKind::Tank ? 6 : 2);
        }
    }
    // Bullets
    for (auto& b : s.bullets) {
        if (!b.alive) {
            continue;
        }
        b.world_x += b.vx;
        b.world_y += b.vy;
        if (b.world_x < 0 || b.world_y < 0 || b.world_x > static_cast<i32>(kWorldW) ||
            b.world_y > static_cast<i32>(kWorldH)) {
            b.alive = false;
            continue;
        }
        if (!b.enemy) {
            for (auto& e : s.enemies) {
                if (!e.alive) {
                    continue;
                }
                const i32 dx = e.world_x - b.world_x;
                const i32 dy = e.world_y - b.world_y;
                i32 hit = 10;
                if (e.kind == EnemyKind::Tank) {
                    hit = 18;
                }
                if (dx * dx + dy * dy < hit * hit) {
                    e.hp -= s.player.pulse_damage;
                    e.flash = 6;
                    emit_effect(s, e.world_x, e.world_y, e.hp <= 0);
                    b.alive = false;
                    if (e.hp <= 0) {
                        e.alive = false;
                        ++s.player.kills;
                        const u32 xv = e.kind == EnemyKind::Tank ? 3u
                                           : (e.kind == EnemyKind::Crawler ? 2u : 1u);
                        spawn_xp(s, e.world_x, e.world_y, xv);
                    }
                    break;
                }
            }
        }
    }
    // XP gems: drift toward player in magnet radius; collect (I-02/I-03).
    for (auto& g : s.xp_gems) {
        if (!g.alive) {
            continue;
        }
        const i32 dx = s.player.world_x - g.world_x;
        const i32 dy = s.player.world_y - g.world_y;
        const i32 d2 = dx * dx + dy * dy;
        if (d2 < 28 * 28) {
            const i32 len = static_cast<i32>(distance_root(static_cast<u32>(d2 < 1 ? 1 : d2)));
            g.vx = (dx * 5) / len;
            g.vy = (dy * 5) / len;
        } else {
            g.vx = 0;
            g.vy = 0;
        }
        g.world_x += g.vx;
        g.world_y += g.vy;
        if (d2 < 12 * 12) {
            g.alive = false;
            s.player.xp += g.value;
            while (s.player.xp >= s.player.xp_need) {
                s.player.xp -= s.player.xp_need;
                ++s.player.level;
                s.player.xp_need = xp_to_level(s.player.level);
                s.level_up_pending = true;
            }
        }
    }
}

void spawn_xp(AppState& s, i32 x, i32 y, u32 value) {
    XpGem g;
    g.alive = true;
    g.world_x = x + s.rng.irange(-6, 6);
    g.world_y = y + s.rng.irange(-6, 6);
    g.value = value ? value : 1;
    for (auto& slot : s.xp_gems) {
        if (!slot.alive) {
            slot = g;
            return;
        }
    }
    if (s.xp_gems.size() < 512) {
        s.xp_gems.push_back(g);
    }
}

bool apply_upgrade(AppState& s, u32 choice) {
    if (!s.level_up_pending) {
        return false;
    }
    switch (choice) {
        case 0:
            ++s.player.pulse_damage;
            break;
        case 1:
            if (s.player.fire_period > 6) {
                s.player.fire_period -= 2;
            }
            break;
        default:
            if (s.player.projectile_count < 5) {
                ++s.player.projectile_count;
            }
            break;
    }
    ++s.upgrades_taken;
    s.level_up_pending = false;
    return true;
}

void player_hurt(AppState& s, i32 damage) {
    if (damage <= 0) {
        return;
    }
    s.player.hp -= damage;
    if (s.player.hp < 0) {
        s.player.hp = 0;
    }
    s.player.hurt_timer = 12;
}

void set_viewport(AppState& s, u32 w, u32 h) {
    s.view_w = w;
    s.view_h = h;
}

void spawn_enemy(AppState& s, EnemyKind kind) {
    Enemy e;
    e.kind = kind;
    e.alive = true;
    e.flash = 0;
    e.anim = 0;
    if (kind == EnemyKind::Drone) {
        e.hp = 2;
    } else if (kind == EnemyKind::Crawler) {
        e.hp = 3;
    } else {
        e.hp = 10;
    }
    // Spawn on a ring outside the current camera viewport (G-02)
    const i32 vw = static_cast<i32>(s.view_w ? s.view_w : 640);
    const i32 vh = static_cast<i32>(s.view_h ? s.view_h : 360);
    const i32 margin = 48;
    const i32 side = static_cast<i32>(s.rng.range(0, 4));
    i32 sx = 0, sy = 0;
    switch (side) {
        case 0:  // top
            sx = s.cam.x + s.rng.irange(0, vw);
            sy = s.cam.y - margin;
            break;
        case 1:  // bottom
            sx = s.cam.x + s.rng.irange(0, vw);
            sy = s.cam.y + vh + margin;
            break;
        case 2:  // left
            sx = s.cam.x - margin;
            sy = s.cam.y + s.rng.irange(0, vh);
            break;
        default:
            sx = s.cam.x + vw + margin;
            sy = s.cam.y + s.rng.irange(0, vh);
            break;
    }
    if (sx < 16) {
        sx = 16;
    }
    if (sy < 16) {
        sy = 16;
    }
    if (sx > static_cast<i32>(kWorldW) - 16) {
        sx = static_cast<i32>(kWorldW) - 16;
    }
    if (sy > static_cast<i32>(kWorldH) - 16) {
        sy = static_cast<i32>(kWorldH) - 16;
    }
    e.world_x = sx;
    e.world_y = sy;
    for (auto& slot : s.enemies) {
        if (!slot.alive) {
            slot = e;
            return;
        }
    }
    if (s.enemies.size() < 256) {
        s.enemies.push_back(e);
    }
}

void fire_pulse(AppState& s) {
    // nearest living enemy
    const Enemy* best = nullptr;
    i64 best_d = 0;
    for (const auto& e : s.enemies) {
        if (!e.alive) {
            continue;
        }
        const i64 dx = e.world_x - s.player.world_x;
        const i64 dy = e.world_y - s.player.world_y;
        const i64 d = dx * dx + dy * dy;
        if (!best || d < best_d) {
            best = &e;
            best_d = d;
        }
    }
    i32 base_vx = 0, base_vy = -6;
    if (best) {
        const i32 dx = best->world_x - s.player.world_x;
        const i32 dy = best->world_y - s.player.world_y;
        const i32 d2 = dx * dx + dy * dy;
        const i32 len = static_cast<i32>(distance_root(static_cast<u32>(d2)));
        base_vx = (dx * 6) / len;
        base_vy = (dy * 6) / len;
    }
    s.player.fire_cooldown = s.player.fire_period;
    const u32 n = s.player.projectile_count ? s.player.projectile_count : 1u;
    for (u32 i = 0; i < n; ++i) {
        Bullet b;
        b.alive = true;
        b.enemy = false;
        b.world_x = s.player.world_x;
        b.world_y = s.player.world_y;
        if (n == 1) {
            b.vx = base_vx;
            b.vy = base_vy;
        } else {
            // deterministic fan around aim direction
            const i32 off = static_cast<i32>(i) - static_cast<i32>(n / 2);
            // rotate ~off*10° using small integer approximation
            b.vx = base_vx - base_vy * off / 6;
            b.vy = base_vy + base_vx * off / 6;
        }
        bool placed = false;
        for (auto& slot : s.bullets) {
            if (!slot.alive) {
                slot = b;
                placed = true;
                break;
            }
        }
        if (!placed && s.bullets.size() < 256) {
            s.bullets.push_back(b);
        }
    }
}

bool in_view(const AppState& s, i32 wx, i32 wy, i32 margin) {
    const i32 sx = wx - s.cam.x;
    const i32 sy = wy - s.cam.y;
    return sx >= -margin && sy >= -margin && sx < static_cast<i32>(s.view_w) + margin &&
           sy < static_cast<i32>(s.view_h) + margin;
}

const char* enemy_sprite_name(EnemyKind k, u32 anim) {
    const u32 f = (anim >> 4) & 1u;
    switch (k) {
        case EnemyKind::Drone:
            return f ? "drone_1" : "drone_0";
        case EnemyKind::Crawler:
            return f ? "crawler_1" : "crawler_0";
        default:
            return f ? "tank_1" : "tank_0";
    }
}

u32 live_enemy_count(const AppState& s) {
    u32 n = 0;
    for (const auto& e : s.enemies) {
        if (e.alive) {
            ++n;
        }
    }
    return n;
}

u32 live_bullet_count(const AppState& s) {
    u32 n = 0;
    for (const auto& b : s.bullets) {
        if (b.alive) {
            ++n;
        }
    }
    return n;
}

u32 hash_sim(const AppState& s) {
    u32 h = 2166136261u;
    auto mix = [&](u32 v) {
        h ^= v;
        h *= 16777619u;
    };
    mix(static_cast<u32>(s.frame));
    mix(static_cast<u32>(s.player.world_x));
    mix(static_cast<u32>(s.player.world_y));
    mix(static_cast<u32>(s.player.hp));
    mix(s.player.kills);
    mix(s.player.xp);
    mix(s.player.level);
    mix(s.level_up_pending ? 1u : 0u);
    for (const auto& e : s.enemies) {
        if (!e.alive) {
            continue;
        }
        mix(static_cast<u32>(e.world_x));
        mix(static_cast<u32>(e.world_y));
        mix(static_cast<u32>(e.hp));
        mix(static_cast<u32>(e.kind));
        mix(static_cast<u32>(e.motion_x));
        mix(static_cast<u32>(e.motion_y));
    }
    for (const auto& b : s.bullets) {
        if (!b.alive) {
            continue;
        }
        mix(static_cast<u32>(b.world_x));
        mix(static_cast<u32>(b.world_y));
    }
    return h;
}

void visible_map_range(const AppState& s, u32 view_w, u32 view_h, i32 guard,
                       i32& tx0, i32& ty0, i32& tx1, i32& ty1) {
    const i32 ts = static_cast<i32>(kMapTileSize);
    tx0 = s.cam.x / ts - guard;
    ty0 = s.cam.y / ts - guard;
    tx1 = (s.cam.x + static_cast<i32>(view_w)) / ts + guard;
    ty1 = (s.cam.y + static_cast<i32>(view_h)) / ts + guard;
    if (tx0 < 0) {
        tx0 = 0;
    }
    if (ty0 < 0) {
        ty0 = 0;
    }
    if (tx1 > static_cast<i32>(kMapTiles) - 1) {
        tx1 = static_cast<i32>(kMapTiles) - 1;
    }
    if (ty1 > static_cast<i32>(kMapTiles) - 1) {
        ty1 = static_cast<i32>(kMapTiles) - 1;
    }
}

const char* player_sprite_name(const Player& p) {
    const u32 f = player_walk_frame(p);
    // Source labels: a=UP  b=DOWN  c=LEFT  d=RIGHT
    if (!p.moving) {
        // Hold last facing after stop (no dedicated side-idle art; use walk frame 0).
        switch (p.dir) {
            case 1:
                return "engineer_a0";
            case 2:
                return "engineer_c0";
            case 3:
                return "engineer_d0";
            default:
                return "engineer_idle";  // front idle when facing down/spawn
        }
    }
    switch (p.dir) {
        case 0:
            return f ? "engineer_b1" : "engineer_b0";
        case 1:
            return f ? "engineer_a1" : "engineer_a0";
        case 2:
            return f ? "engineer_c1" : "engineer_c0";
        default:
            return f ? "engineer_d1" : "engineer_d0";
    }
}

void render_scene(gpu2d::GraphicsApi& api, const AppState& s, const TexBank& tex,
                  u32 view_w, u32 view_h) {
    i32 tx0, ty0, tx1, ty1;
    visible_map_range(s, view_w, view_h, 1, tx0, ty0, tx1, ty1);
    api.fill_rect(0, 0, view_w, view_h, gpu2d::Color::rgb(8, 12, 20));
    const i32 ts = static_cast<i32>(kMapTileSize);
    for (i32 ty = ty0; ty <= ty1; ++ty) {
        for (i32 tx = tx0; tx <= tx1; ++tx) {
            const u32 idx = map_index(s, static_cast<u32>(tx), static_cast<u32>(ty));
            static constexpr const char* panels[] = {
                "floor_base_0", "floor_base_1", "floor_base_2", "floor_base_3"};
            const TexRef* t = tex.find(idx == 0 ? panels[(ty & 1) * 2 + (tx & 1)] : floor_name(idx));
            if (!t) {
                continue;
            }
            gpu2d::SpriteParams sp;
            sp.tex = t->id; sp.src_x = t->sx; sp.src_y = t->sy;
            sp.w = t->w;
            sp.h = t->h;
            sp.dst_x = tx * ts - s.cam.x;
            sp.dst_y = ty * ts - s.cam.y;
            api.draw_sprite(sp);
        }
    }
    // Authored service bays repeat across the large world; only visible art is submitted.
    // These are visual landmarks, not newly introduced collision walls.
    auto world_rect = [&](i32 wx, i32 wy, u32 w, u32 h, gpu2d::Color color) {
        if (wx + static_cast<i32>(w) > s.cam.x && wy + static_cast<i32>(h) > s.cam.y &&
            wx < s.cam.x + static_cast<i32>(view_w) && wy < s.cam.y + static_cast<i32>(view_h))
            api.fill_rect(wx - s.cam.x, wy - s.cam.y, w, h, color);
    };
    // Soft contact shadow (two stacked rects — no host blur).
    auto contact_shadow = [&](i32 wx, i32 wy, i32 half_w, i32 half_h) {
        const i32 sx = wx - s.cam.x - half_w;
        const i32 sy = wy - s.cam.y - half_h / 2;
        if (sx + half_w * 2 <= 0 || sy + half_h <= 0 || sx >= static_cast<i32>(view_w) ||
            sy >= static_cast<i32>(view_h)) {
            return;
        }
        api.fill_rect(sx, sy + 1, static_cast<u32>(half_w * 2), static_cast<u32>(half_h - 1),
                      gpu2d::Color::rgba(0, 0, 0, 70));
        api.fill_rect(sx + 2, sy, static_cast<u32>(half_w * 2 - 4), static_cast<u32>(half_h),
                      gpu2d::Color::rgba(0, 0, 0, 90));
    };
    auto art = [&](const char* name, i32 wx, i32 wy, bool opaque = false,
                   i32 scale = 1, gpu2d::BlendMode blend = gpu2d::BlendMode::StraightAlpha,
                   u8 alpha = 255, bool shadow = true) {
        const TexRef* t = tex.find(name);
        if (!t) return;
        const i32 sx = wx - s.cam.x - static_cast<i32>(t->ax) * scale;
        const i32 sy = wy - s.cam.y - static_cast<i32>(t->ay) * scale;
        const i32 w = static_cast<i32>(t->w) * scale, h = static_cast<i32>(t->h) * scale;
        if (sx + w <= 0 || sy + h <= 0 || sx >= static_cast<i32>(view_w) || sy >= static_cast<i32>(view_h)) return;
        if (shadow && !opaque && t->w >= 12) {
            contact_shadow(wx, wy, w / 2, h / 6 > 4 ? h / 6 : 5);
        }
        gpu2d::SpriteParams sp;
        sp.tex = t->id; sp.src_x = t->sx; sp.src_y = t->sy; sp.w = t->w; sp.h = t->h;
        sp.dst_x = sx; sp.dst_y = sy;
        sp.scale_w = w; sp.scale_h = h;
        sp.blend = opaque ? gpu2d::BlendMode::Copy : blend;
        sp.global_alpha = alpha;
        api.draw_sprite(sp);
    };
    for (i32 cy = 512; cy <= 3584; cy += 768) {
        for (i32 cx = 512; cx <= 3584; cx += 768) {
            // North/south service islands frame an open fighting area with side exits.
            for (i32 sign : {-1, 1}) {
                const i32 y = cy + (sign < 0 ? -86 : 150);
                world_rect(cx - 256, y - 28, 512, 58, gpu2d::Color::rgb(12, 22, 30));
                world_rect(cx - 256, y + 30, 512, 3, gpu2d::Color::rgb(54, 73, 82));
                world_rect(cx - 240, y + 19, 480, 3, gpu2d::Color::rgb(26, 63, 76));
                for (i32 x = -240; x <= 224; x += 32) {
                    if (x < -64 || x > 64) art("hazard_stripe", cx+x, y+34, true);
                }
                for (i32 x : {-224, -180, 164, 208}) {
                    art("server", cx+x, y+18);
                    world_rect(cx+x-5, y+22, 10, 2, gpu2d::Color::rgb(42, 126, 151));
                }
                art("pipe_elbow", cx-114, y+16);
                art("console", cx+104, y+18);
                art("power_panel", cx-32, y-42, true);
                art("canister", cx-50, y+18);
                art("canister", cx+50, y+18);
            }
            art("mark_a3", cx-204, cy-60, true);
            art("mark_service", cx+138, cy+35, true);
            art("warning_lamp", cx-270, cy-88);
            art("warning_lamp", cx+270, cy+150);
            art("barrier", cx-248, cy+104);
            art("barrier", cx+250, cy-60);
            art("stacked_crates", cx-262, cy+152);
            art("crate", cx-214, cy+140);
            art("barrel", cx+263, cy+42);
            art("cable_spool", cx+228, cy+63);
            // Inter-room landmarks preserve meaning after travelling > one viewport.
            art("access_door", cx+448, cy-96);
            art("power_cabinet", cx+404, cy-80);
            art("machine_wreck", cx+464, cy+48);
        }
    }
    // enemies (cull off-screen — G-05)
    for (const auto& e : s.enemies) {
        if (!e.alive) {
            continue;
        }
        if (!in_view(s, e.world_x, e.world_y, 48)) {
            continue;
        }
        const TexRef* et = tex.find(enemy_sprite_name(e.kind, e.anim));
        if (!et) {
            continue;
        }
        const i32 esc = e.kind == EnemyKind::Tank ? 3 : 2;  // *1.5
        const i32 ew = static_cast<i32>(et->w) * esc / 2;
        const i32 eh = static_cast<i32>(et->h) * esc / 2;
        contact_shadow(e.world_x, e.world_y, ew / 2, eh / 5 > 4 ? eh / 5 : 5);
        // Lift body off dark floor: faint additive aura behind sprite.
        {
            const TexRef* gl = tex.find("glow_small");
            if (gl) {
                gpu2d::SpriteParams g;
                g.tex = gl->id; g.src_x = gl->sx; g.src_y = gl->sy;
                g.w = gl->w; g.h = gl->h;
                g.scale_w = ew + 8; g.scale_h = eh + 8;
                g.dst_x = e.world_x - s.cam.x - (ew + 8) / 2;
                g.dst_y = e.world_y - s.cam.y - (eh + 8) / 2;
                g.blend = gpu2d::BlendMode::AddSat;
                g.global_alpha = 48;
                g.color_mod = true;
                g.mod = gpu2d::Color::rgb(255, 60, 60);
                api.draw_sprite(g);
            }
        }
        gpu2d::SpriteParams sp;
        sp.blend = gpu2d::BlendMode::StraightAlpha;
        sp.tex = et->id; sp.src_x = et->sx; sp.src_y = et->sy;
        sp.w = et->w;
        sp.h = et->h;
        sp.scale_w = ew; sp.scale_h = eh;
        sp.dst_x = e.world_x - s.cam.x - static_cast<i32>(et->ax) * esc / 2;
        sp.dst_y = e.world_y - s.cam.y - static_cast<i32>(et->ay) * esc / 2;
        if (e.flash) {
            sp.color_mod = true;
            sp.mod = gpu2d::Color::rgb(255, 120, 120);
        } else {
            // Mild lighten so dark chassis reads against floor (multiply >1 not available;
            // use near-white mod to avoid double-darkening).
            sp.color_mod = true;
            sp.mod = gpu2d::Color::rgb(230, 230, 235);
        }
        api.draw_sprite(sp);
    }
    // pulse shots
    const TexRef* bt = tex.find("pulse_shot");
    if (bt) {
        for (const auto& b : s.bullets) {
            if (!b.alive || !in_view(s, b.world_x, b.world_y, 16)) {
                continue;
            }
            gpu2d::SpriteParams sp;
            sp.tex = bt->id; sp.src_x = bt->sx; sp.src_y = bt->sy;
            sp.w = bt->w;
            sp.h = bt->h;
            sp.dst_x = b.world_x - s.cam.x - static_cast<i32>(bt->ax);
            sp.dst_y = b.world_y - s.cam.y - static_cast<i32>(bt->ay);
            sp.blend = gpu2d::BlendMode::AddSat;
            api.draw_sprite(sp);
        }
    }
    // XP crystals (I-02)
    const TexRef* xg = tex.find("xp_small");
    if (xg) {
        for (const auto& g : s.xp_gems) {
            if (!g.alive || !in_view(s, g.world_x, g.world_y, 16)) {
                continue;
            }
            gpu2d::SpriteParams sp;
            sp.tex = xg->id; sp.src_x = xg->sx; sp.src_y = xg->sy;
            sp.w = xg->w; sp.h = xg->h;
            sp.scale_w = static_cast<i32>(xg->w) * 3 / 2;
            sp.scale_h = static_cast<i32>(xg->h) * 3 / 2;
            sp.dst_x = g.world_x - s.cam.x - static_cast<i32>(xg->ax) * 3 / 2;
            sp.dst_y = g.world_y - s.cam.y - static_cast<i32>(xg->ay) * 3 / 2;
            sp.blend = gpu2d::BlendMode::AddSat;
            api.draw_sprite(sp);
        }
    }
    // player — 48px (3/2 of 32) for showcase readability
    const TexRef* pt = tex.find(player_sprite_name(s.player));
    if (pt) {
        contact_shadow(s.player.world_x, s.player.world_y, 14, 6);
        gpu2d::SpriteParams sp;
        sp.blend = gpu2d::BlendMode::StraightAlpha;
        sp.tex = pt->id; sp.src_x = pt->sx; sp.src_y = pt->sy;
        sp.w = pt->w;
        sp.h = pt->h;
        sp.scale_w = static_cast<i32>(pt->w) * 3 / 2;
        sp.scale_h = static_cast<i32>(pt->h) * 3 / 2;
        sp.dst_x = s.player.world_x - s.cam.x - static_cast<i32>(pt->ax) * 3 / 2;
        sp.dst_y = s.player.world_y - s.cam.y - static_cast<i32>(pt->ay) * 3 / 2;
        if (s.player.hurt_timer > 0) {
            sp.color_mod = true;
            sp.mod = gpu2d::Color::rgb(255, 80, 80);
        }
        api.draw_sprite(sp);
    }
    // Short-lived world-space feedback, drawn above bodies. No permanent glow veil.
    for (const auto& fx : s.effects) {
        if (!fx.life) continue;
        art(fx.explosion ? "explosion" : "spark", fx.world_x, fx.world_y, false, 1,
            gpu2d::BlendMode::AddSat, static_cast<u8>(fx.explosion ? fx.life * 14 : fx.life * 36));
    }
    // Industrial HUD foundation. Text stays in the existing bitmap-font presenter.
    api.fill_rect(0, 0, view_w, 32, gpu2d::Color::rgb(5, 13, 21));
    api.fill_rect(0, 31, view_w, 1, gpu2d::Color::rgb(43, 97, 119));
    api.fill_rect(10, 9, 3, 14, gpu2d::Color::rgb(245, 149, 43));
    api.fill_rect(155, 9, 116, 14, gpu2d::Color::rgb(48, 65, 77));
    api.fill_rect(157, 11, 112, 10, gpu2d::Color::rgb(17, 25, 34));
    const u32 hpw = static_cast<u32>(s.player.hp > 0 ? s.player.hp : 0) * 112 / 100;
    if (hpw) {
        api.fill_rect(157, 11, hpw, 10, gpu2d::Color::rgb(195, 52, 50));
        api.fill_rect(157, 11, hpw, 2, gpu2d::Color::rgb(247, 102, 75));
    }
    const TexRef* icon = tex.find("weapon_icon");
    if (icon) {
        gpu2d::SpriteParams sp;
        sp.tex=icon->id; sp.src_x=icon->sx; sp.src_y=icon->sy; sp.w=icon->w; sp.h=icon->h;
        sp.dst_x=static_cast<i32>(view_w)-35; sp.dst_y=4;
        sp.blend=gpu2d::BlendMode::StraightAlpha;
        api.draw_sprite(sp);
    }
    // XP bar under HP (I-04)
    api.fill_rect(155, 26, 116, 4, gpu2d::Color::rgb(30, 48, 58));
    if (s.player.xp_need) {
        const u32 xpw = s.player.xp * 112 / s.player.xp_need;
        if (xpw) {
            api.fill_rect(157, 27, xpw, 2, gpu2d::Color::rgb(64, 210, 160));
        }
    }
    // Level-up panel (I-05): dark scrim + industrial cards; text drawn by host font in main.
    if (s.level_up_pending) {
        api.fill_rect(0, 0, view_w, view_h, gpu2d::Color::rgba(4, 8, 14, 200));
        const i32 cx = static_cast<i32>(view_w) / 2;
        const i32 cy = static_cast<i32>(view_h) / 2;
        api.fill_rect(cx - 150, cy - 56, 300, 112, gpu2d::Color::rgb(12, 22, 32));
        api.fill_rect(cx - 150, cy - 56, 300, 2, gpu2d::Color::rgb(64, 160, 190));
        api.fill_rect(cx - 150, cy + 54, 300, 2, gpu2d::Color::rgb(64, 160, 190));
        for (int i = 0; i < 3; ++i) {
            const i32 bx = cx - 140 + i * 96;
            api.fill_rect(bx, cy - 24, 88, 48, gpu2d::Color::rgb(22, 40, 52));
            api.fill_rect(bx, cy - 24, 88, 2, gpu2d::Color::rgb(245, 149, 43));
            api.fill_rect(bx, cy + 22, 88, 2, gpu2d::Color::rgb(40, 90, 110));
        }
    }
}

}  // namespace facility
