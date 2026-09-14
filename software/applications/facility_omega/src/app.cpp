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
    s.player.world_x = static_cast<i32>(kWorldW / 2);
    s.player.world_y = static_cast<i32>(kWorldH / 2);
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
}

void render_scene(gpu2d::GraphicsApi& api, const AppState& s, const TexBank& tex,
                  u32 view_w, u32 view_h) {
    // Visible MapTiles + 1 guard band
    const i32 tx0 = s.cam.x / static_cast<i32>(kMapTileSize) - 1;
    const i32 ty0 = s.cam.y / static_cast<i32>(kMapTileSize) - 1;
    const i32 tx1 = (s.cam.x + static_cast<i32>(view_w)) / static_cast<i32>(kMapTileSize) + 1;
    const i32 ty1 = (s.cam.y + static_cast<i32>(view_h)) / static_cast<i32>(kMapTileSize) + 1;
    api.fill_rect(0, 0, view_w, view_h, gpu2d::Color::rgb(8, 12, 20));
    for (i32 ty = ty0; ty <= ty1; ++ty) {
        if (ty < 0 || ty >= static_cast<i32>(kMapTiles)) {
            continue;
        }
        for (i32 tx = tx0; tx <= tx1; ++tx) {
            if (tx < 0 || tx >= static_cast<i32>(kMapTiles)) {
                continue;
            }
            const u32 idx = map_index(s, static_cast<u32>(tx), static_cast<u32>(ty));
            const TexRef* t = tex.find(floor_name(idx));
            if (!t) {
                continue;
            }
            gpu2d::SpriteParams sp;
            sp.tex = t->id;
            sp.w = t->w;
            sp.h = t->h;
            sp.dst_x = tx * static_cast<i32>(kMapTileSize) - s.cam.x;
            sp.dst_y = ty * static_cast<i32>(kMapTileSize) - s.cam.y;
            api.draw_sprite(sp);
        }
    }
    // props near spawn
    static const char* kProps[] = {"barrel", "crate", "console", "canister", "grate",
                                   "hazard_stripe"};
    for (u32 i = 0; i < 6; ++i) {
        const TexRef* t = tex.find(kProps[i]);
        if (!t) {
            continue;
        }
        const i32 wx = static_cast<i32>(kWorldW / 2) + static_cast<i32>(i % 3) * 48 - 80;
        const i32 wy = static_cast<i32>(kWorldH / 2) + static_cast<i32>(i / 3) * 48 - 40;
        const i32 sx = wx - s.cam.x;
        const i32 sy = wy - s.cam.y;
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
    // player
    const char* pname = "engineer_idle";
    switch (s.player.dir) {
        case 0:
            pname = (s.player.frame & 8) ? "engineer_a0" : "engineer_a1";
            break;
        case 1:
            pname = (s.player.frame & 8) ? "engineer_b0" : "engineer_b1";
            break;
        case 2:
            pname = (s.player.frame & 8) ? "engineer_c0" : "engineer_c1";
            break;
        default:
            pname = (s.player.frame & 8) ? "engineer_d0" : "engineer_d1";
            break;
    }
    if (!s.player.moving) {
        pname = "engineer_idle";
    }
    const TexRef* pt = tex.find(pname);
    if (pt) {
        gpu2d::SpriteParams sp;
        sp.tex = pt->id;
        sp.w = pt->w;
        sp.h = pt->h;
        sp.dst_x = s.player.world_x - s.cam.x - static_cast<i32>(pt->ax);
        sp.dst_y = s.player.world_y - s.cam.y - static_cast<i32>(pt->ay);
        if (s.player.hp < s.player.max_hp && (s.frame & 2)) {
            sp.color_mod = true;
            sp.mod = gpu2d::Color::rgb(255, 80, 80);
        }
        api.draw_sprite(sp);
    }
}

}  // namespace facility
