#include "facility/app.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace facility {
namespace {

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
        const std::string fmt = grab_str("\"format\"");
        b.rgb565 = (fmt == "rgb565");
        b.indexed = (fmt == "index8");
        const std::string rf = grab_str("\"runtime_file\"");
        if (b.name.empty() || rf.empty() || !read_file(runtime_dir + "/" + rf, b.pixels)) {
            pos = npos + 7;
            continue;
        }
        out.push_back(std::move(b));
        pos = npos + 7;
    }
    return !out.empty();
}

void sim_reset(AppState& s, u32 seed) {
    s = AppState{};
    s.map_seed = seed ? seed : 1u;
    s.map.resize(static_cast<size_t>(kMapTiles) * kMapTiles);
    // Deterministic facility floor mix (not random wall-clock).
    u32 x = s.map_seed;
    for (u32 i = 0; i < s.map.size(); ++i) {
        x = x * 1664525u + 1013904223u;
        s.map[i] = static_cast<u8>((x >> 16) % 8u);
    }
    // Zone stamp: Power Test Area hazard ring near spawn
    for (u32 ty = 60; ty < 68; ++ty) {
        for (u32 tx = 60; tx < 68; ++tx) {
            if (ty == 60 || ty == 67 || tx == 60 || tx == 67) {
                s.map[static_cast<size_t>(ty) * kMapTiles + tx] = 7;  // reuse floor as marker
            }
        }
    }
    s.player = Player{};
    s.player.world_x = static_cast<i32>(kWorldW / 2);
    s.player.world_y = static_cast<i32>(kWorldH / 2);
    s.sim_seed = seed ? seed : 1u;
    s.rng.seed(s.sim_seed);
    s.enemies.clear();
    s.enemies.reserve(256);
    s.bullets.clear();
    s.bullets.reserve(256);
    s.spawn_timer = 0;
    s.enemy_count_target = 6;
    s.frame = 0;
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
    ++s.frame;
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
            // step = speed * dx / approx_len
            i32 len = 1;
            // crude integer sqrt
            while ((len + 1) * (len + 1) <= dist2 && len < 10000) {
                ++len;
            }
            e.world_x += (dx * sp) / len;
            e.world_y += (dy * sp) / len;
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
                    b.alive = false;
                    if (e.hp <= 0) {
                        e.alive = false;
                        ++s.player.kills;
                    }
                    break;
                }
            }
        }
    }
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
    Bullet b;
    b.alive = true;
    b.enemy = false;
    b.world_x = s.player.world_x;
    b.world_y = s.player.world_y;
    if (best) {
        const i32 dx = best->world_x - s.player.world_x;
        const i32 dy = best->world_y - s.player.world_y;
        i32 len = 1;
        const i32 d2 = dx * dx + dy * dy;
        while ((len + 1) * (len + 1) <= d2 && len < 10000) {
            ++len;
        }
        b.vx = (dx * 6) / len;
        b.vy = (dy * 6) / len;
    } else {
        b.vx = 0;
        b.vy = -6;
    }
    s.player.fire_cooldown = s.player.fire_period;
    for (auto& slot : s.bullets) {
        if (!slot.alive) {
            slot = b;
            return;
        }
    }
    if (s.bullets.size() < 256) {
        s.bullets.push_back(b);
    }
}

bool in_view(const AppState& s, i32 wx, i32 wy, i32 margin) {
    const i32 sx = wx - s.cam.x;
    const i32 sy = wy - s.cam.y;
    return sx >= -margin && sy >= -margin && sx < static_cast<i32>(s.view_w) + margin &&
           sy < static_cast<i32>(s.view_h) + margin;
}

const char* enemy_sprite_name(EnemyKind k, u32 anim) {
    (void)anim;
    switch (k) {
        case EnemyKind::Drone:
            return "drone_0";
        case EnemyKind::Crawler:
            return "crawler_0";
        default:
            return "tank_0";
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
    for (const auto& e : s.enemies) {
        if (!e.alive) {
            continue;
        }
        mix(static_cast<u32>(e.world_x));
        mix(static_cast<u32>(e.world_y));
        mix(static_cast<u32>(e.hp));
        mix(static_cast<u32>(e.kind));
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
    if (!p.moving) {
        return "engineer_idle";
    }
    const u32 f = player_walk_frame(p);
    switch (p.dir) {
        case 0:
            return f ? "engineer_a1" : "engineer_a0";
        case 1:
            return f ? "engineer_b1" : "engineer_b0";
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
            const TexRef* t = tex.find(floor_name(idx));
            if (!t) {
                continue;
            }
            gpu2d::SpriteParams sp;
            sp.tex = t->id;
            sp.w = t->w;
            sp.h = t->h;
            sp.dst_x = tx * ts - s.cam.x;
            sp.dst_y = ty * ts - s.cam.y;
            api.draw_sprite(sp);
        }
    }
    // Deterministic props across the facility (E-08, ≥6 types).
    struct Prop {
        const char* name;
        i32 wx, wy;
    };
    static const Prop kProps[] = {
        {"barrel", 2048, 2048}, {"crate", 2100, 2060},   {"console", 1980, 2100},
        {"canister", 2200, 1980}, {"grate", 1900, 2200},  {"hazard_stripe", 2048, 1920},
        {"barrel", 1000, 1000},  {"crate", 3000, 3000},   {"console", 3200, 1200},
        {"canister", 800, 3400}, {"grate", 3600, 800},    {"hazard_stripe", 1500, 2800},
    };
    for (const auto& pr : kProps) {
        const TexRef* t = tex.find(pr.name);
        if (!t) {
            continue;
        }
        const i32 sx = pr.wx - s.cam.x;
        const i32 sy = pr.wy - s.cam.y;
        if (sx < -64 || sy < -64 || sx > static_cast<i32>(view_w) ||
            sy > static_cast<i32>(view_h)) {
            continue;
        }
        gpu2d::SpriteParams sp;
        sp.tex = t->id;
        sp.w = t->w;
        sp.h = t->h;
        sp.dst_x = sx - static_cast<i32>(t->ax);
        sp.dst_y = sy - static_cast<i32>(t->ay);
        api.draw_sprite(sp);
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
        gpu2d::SpriteParams sp;
        sp.tex = et->id;
        sp.w = et->w;
        sp.h = et->h;
        sp.dst_x = e.world_x - s.cam.x - static_cast<i32>(et->ax);
        sp.dst_y = e.world_y - s.cam.y - static_cast<i32>(et->ay);
        if (e.flash) {
            sp.color_mod = true;
            sp.mod = gpu2d::Color::rgb(255, 100, 100);
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
            sp.tex = bt->id;
            sp.w = bt->w;
            sp.h = bt->h;
            sp.dst_x = b.world_x - s.cam.x - static_cast<i32>(bt->ax);
            sp.dst_y = b.world_y - s.cam.y - static_cast<i32>(bt->ay);
            sp.blend = gpu2d::BlendMode::AddSat;
            api.draw_sprite(sp);
        }
    }
    // player
    const TexRef* pt = tex.find(player_sprite_name(s.player));
    if (pt) {
        gpu2d::SpriteParams sp;
        sp.tex = pt->id;
        sp.w = pt->w;
        sp.h = pt->h;
        sp.dst_x = s.player.world_x - s.cam.x - static_cast<i32>(pt->ax);
        sp.dst_y = s.player.world_y - s.cam.y - static_cast<i32>(pt->ay);
        if (s.player.hurt_timer > 0) {
            sp.color_mod = true;
            sp.mod = gpu2d::Color::rgb(255, 80, 80);
        }
        api.draw_sprite(sp);
    }
}

}  // namespace facility
