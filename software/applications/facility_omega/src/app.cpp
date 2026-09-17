#include "facility/app.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace facility {
namespace {

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
    s.map.assign(static_cast<size_t>(kMapTiles) * kMapTiles, 0);
    s.environment = make_hero_environment();
    s.player = Player{};
    s.player.world_x = static_cast<i32>(kWorldW / 2);
    s.player.world_y = static_cast<i32>(kWorldH / 2);
    s.player.xp_need = xp_to_level(s.player.level);
    s.sim_seed = seed ? seed : 1u;
    s.rng.seed(s.sim_seed);
    s.enemies.clear();
    s.enemies.reserve(kEnemyCapacity);
    s.bullets.clear();
    s.bullets.reserve(kProjectileCapacity);
    s.effects.reserve(kEffectCapacity);
    s.xp_gems.clear();
    s.xp_gems.reserve(kPickupCapacity);
    s.spawn_timer = 0;
    s.enemy_count_target = kEnemyCapacity;
    init_gameplay(s);
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
    const i32 original_x = s.player.world_x, original_y = s.player.world_y;
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
    const i32 dx = s.player.world_x - original_x, dy = s.player.world_y - original_y;
    s.player.world_x = original_x; s.player.world_y = original_y;
    move_in_environment(s.environment, s.player.world_x, s.player.world_y, dx, dy, 8);
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
        case EnemyKind::Runner:
            return f ? "runner_1" : "runner_0";
        case EnemyKind::Elite:
            return f ? "elite_1" : "elite_0";
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
    render_environment(api, s.environment, tex, s.cam.x, s.cam.y, view_w, view_h);
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
    auto art = [&](const TexRef* t, i32 wx, i32 wy, bool opaque = false,
                   i32 scale = 1, gpu2d::BlendMode blend = gpu2d::BlendMode::StraightAlpha,
                   u8 alpha = 255, bool shadow = true) {
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
    const TexRef* enemy_art[5][2]{};
    for(u32 k=0;k<5;++k)for(u32 f=0;f<2;++f)enemy_art[k][f]=tex.find(enemy_sprite_name(static_cast<EnemyKind>(k),f*16));
    const auto* glow=tex.find("glow_small");
    const auto* ring=tex.find("ring");
    const auto* field=tex.find("glow_large");
    const auto* orbit=tex.find("orbit_drone");
    const auto* spark=tex.find("spark");const auto* explosion=tex.find("explosion");
    auto area=[&](const TexRef* t,i32 x,i32 y,i32 radius,u8 alpha,gpu2d::BlendMode blend,bool bilinear) {
        if(!t || radius<=0)return;
        gpu2d::SpriteParams p;p.tex=t->id;p.src_x=t->sx;p.src_y=t->sy;p.w=t->w;p.h=t->h;
        p.dst_x=x-s.cam.x-radius;p.dst_y=y-s.cam.y-radius;p.scale_w=radius*2;p.scale_h=radius*2;
        p.blend=blend;p.global_alpha=alpha;p.filter=bilinear?gpu2d::FilterMode::Bilinear:gpu2d::FilterMode::Nearest;
        api.draw_sprite(p);
    };
    const auto& field_state=s.gameplay.weapons[3];
    if(field_state.level) {
        const i32 radius=weapon_tuning(WeaponId::Field,field_state.level).radius;
        area(field,s.player.world_x,s.player.world_y,radius,42,gpu2d::BlendMode::StraightAlpha,false);
        area(ring,s.player.world_x,s.player.world_y,radius,64,gpu2d::BlendMode::StraightAlpha,false);
    }
    const auto& nova=s.gameplay.weapons[2];
    if(nova.level && nova.age) {
        const auto duration=weapon_tuning(WeaponId::Nova,nova.level).duration;
        const u8 alpha=static_cast<u8>(220*(duration-std::min(duration,nova.age))/duration);
        area(ring,nova.x,nova.y,nova.radius,alpha,gpu2d::BlendMode::AddSat,true);
    }
    // Texture resolution is outside entity hot loops.
    // enemies (cull off-screen — G-05)
    for (const auto& e : s.enemies) {
        if (!e.alive) {
            continue;
        }
        if (!in_view(s, e.world_x, e.world_y, 48)) {
            continue;
        }
        const TexRef* et = enemy_art[static_cast<u32>(e.kind)][(e.anim>>4)&1u];
        if (!et) {
            continue;
        }
        const i32 esc = (e.kind == EnemyKind::Tank || e.kind==EnemyKind::Elite) ? 3 : 2;
        const i32 ew = static_cast<i32>(et->w) * esc / 2;
        const i32 eh = static_cast<i32>(et->h) * esc / 2;
        contact_shadow(e.world_x, e.world_y, ew / 2, eh / 5 > 4 ? eh / 5 : 5);
        // Lift body off dark floor: faint additive aura behind sprite.
        {
            const TexRef* gl = glow;
            if (gl) {
                gpu2d::SpriteParams g;
                g.tex = gl->id; g.src_x = gl->sx; g.src_y = gl->sy;
                g.w = gl->w; g.h = gl->h;
                g.scale_w = ew + 8; g.scale_h = eh + 8;
                g.dst_x = e.world_x - s.cam.x - (ew + 8) / 2;
                g.dst_y = e.world_y - s.cam.y - (eh + 8) / 2;
                g.blend = gpu2d::BlendMode::AddSat;
                g.global_alpha = e.kind==EnemyKind::Elite?140:48;
                g.color_mod = true;
                g.mod = e.kind==EnemyKind::Elite?gpu2d::Color::rgb(215,70,255):gpu2d::Color::rgb(255,60,60);
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
    const TexRef* xp_art = tex.find("xp_small");
    const TexRef* repair_art = tex.find("repair_pickup");
    if (xp_art && repair_art) {
        for (const auto& g : s.xp_gems) {
            if (!g.alive || !in_view(s, g.world_x, g.world_y, 16)) {
                continue;
            }
            const auto* xg=g.repair?repair_art:xp_art;
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
    if(s.gameplay.weapons[1].level) {
        const auto t=weapon_tuning(WeaponId::Orbit,s.gameplay.weapons[1].level);
        for(u32 i=0;i<t.count;++i) {
            i32 x,y;orbit_position(s,i,x,y);
            area(glow,x,y,16,70,gpu2d::BlendMode::AddSat,false);
            art(orbit,x,y,false,1,gpu2d::BlendMode::StraightAlpha,255,false);
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
        art(fx.explosion ? explosion : spark, fx.world_x, fx.world_y, false, 1,
            gpu2d::BlendMode::AddSat, static_cast<u8>(fx.explosion ? fx.life * 14 : fx.life * 36));
    }
    // Industrial HUD foundation. Text stays in the existing bitmap-font presenter.
    api.fill_rect(0, 0, view_w, 32, gpu2d::Color::rgb(5, 13, 21));
    api.fill_rect(0, 31, view_w, 1, gpu2d::Color::rgb(43, 97, 119));
    api.fill_rect(10, 9, 3, 14, gpu2d::Color::rgb(245, 149, 43));
    api.fill_rect(155, 9, 116, 14, gpu2d::Color::rgb(48, 65, 77));
    api.fill_rect(157, 11, 112, 10, gpu2d::Color::rgb(17, 25, 34));
    const u32 hpw = static_cast<u32>(s.player.hp > 0 ? s.player.hp : 0) * 112 / static_cast<u32>(std::max(1,s.player.max_hp));
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
    const TexRef* build_icons[]={icon,orbit,ring,tex.find("energy_field_core")};
    auto icon_at=[&](u32 kind,i32 x,i32 y) {
        const auto* t=kind<4?build_icons[kind]:repair_art;if(!t)return;
        gpu2d::SpriteParams p;p.tex=t->id;p.src_x=t->sx;p.src_y=t->sy;p.w=t->w;p.h=t->h;
        p.dst_x=x;p.dst_y=y;p.scale_w=16;p.scale_h=16;p.blend=gpu2d::BlendMode::StraightAlpha;api.draw_sprite(p);
    };
    for(u32 i=0;i<4;++i)if(s.gameplay.weapons[i].level)icon_at(i,300+i*72,34);
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
            icon_at(s.gameplay.choices[i].kind,bx+5,cy-20);
        }
    }
}

}  // namespace facility
