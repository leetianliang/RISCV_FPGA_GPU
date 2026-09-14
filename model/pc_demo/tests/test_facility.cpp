#include "facility/app.hpp"

#include <cstdio>
#include <cstring>
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
}  // namespace

int main() {
    using namespace facility;
    // MAP freeze
    CHECK(kWorldW == 4096);
    CHECK(kWorldH == 4096);
    CHECK(kMapTiles == 128);
    CHECK(kMapTileSize == 32);

    AppState s;
    sim_reset(s, 1234);
    CHECK(s.ready);
    CHECK(s.map.size() == kMapTiles * kMapTiles);
    CHECK(s.player.world_x == static_cast<i32>(kWorldW / 2));

    // camera center / clamps
    camera_follow(s, 640, 360);
    CHECK(s.cam.x == static_cast<i32>(kWorldW / 2) - 320);
    CHECK(s.cam.y == static_cast<i32>(kWorldH / 2) - 180);
    s.player.world_x = 0;
    s.player.world_y = 0;
    camera_follow(s, 640, 360);
    CHECK(s.cam.x == 0 && s.cam.y == 0);
    s.player.world_x = static_cast<i32>(kWorldW) - 1;
    s.player.world_y = static_cast<i32>(kWorldH) - 1;
    camera_follow(s, 640, 360);
    CHECK(s.cam.x == static_cast<i32>(kWorldW) - 640);
    CHECK(s.cam.y == static_cast<i32>(kWorldH) - 360);

    // world→screen
    s.player.world_x = 2000;
    s.player.world_y = 1500;
    camera_follow(s, 640, 360);
    CHECK(world_to_screen_x(s, 2000) >= 0 && world_to_screen_x(s, 2000) < 640);
    CHECK(world_to_screen_y(s, 1500) >= 0 && world_to_screen_y(s, 1500) < 360);

    // visible MapTile range
    i32 tx0, ty0, tx1, ty1;
    visible_map_range(s, 640, 360, 1, tx0, ty0, tx1, ty1);
    CHECK(tx0 >= 0 && ty0 >= 0);
    CHECK(tx1 < static_cast<i32>(kMapTiles));
    CHECK(tx1 - tx0 + 1 <= 23);  // ~20 tiles + guard band

    // E-09 move > one screen changes camera / visible tiles
    const i32 cam0 = s.cam.x;
    i32 vtx0, vty0, vtx1, vty1;
    visible_map_range(s, 640, 360, 1, vtx0, vty0, vtx1, vty1);
    for (int i = 0; i < 200; ++i) {
        player_move(s, false, false, false, true, 4);
    }
    camera_follow(s, 640, 360);
    CHECK(s.cam.x > cam0 + 200);
    i32 vtx0b, vty0b, vtx1b, vty1b;
    visible_map_range(s, 640, 360, 1, vtx0b, vty0b, vtx1b, vty1b);
    CHECK(vtx0b > vtx0);

    // F: 4 directions
    {
        AppState p;
        sim_reset(p, 1);
        player_move(p, true, false, false, false);
        CHECK(p.player.dir == 1);
        player_move(p, false, true, false, false);
        CHECK(p.player.dir == 0);
        player_move(p, false, false, true, false);
        CHECK(p.player.dir == 2);
        player_move(p, false, false, false, true);
        CHECK(p.player.dir == 3);
        CHECK(std::string(player_sprite_name(p.player)) == "engineer_d0" ||
              std::string(player_sprite_name(p.player)) == "engineer_d1");
    }
    // F: idle
    {
        AppState p;
        sim_reset(p, 1);
        CHECK(std::string(player_sprite_name(p.player)) == "engineer_idle");
    }
    // F: 2-frame walk cycle
    {
        AppState p;
        sim_reset(p, 1);
        for (int i = 0; i < 8; ++i) {
            player_move(p, false, false, false, true, 2);
        }
        const u32 f0 = player_walk_frame(p.player);
        for (int i = 0; i < 8; ++i) {
            player_move(p, false, false, false, true, 2);
        }
        const u32 f1 = player_walk_frame(p.player);
        CHECK(f0 != f1);
        const char* n0 = player_sprite_name(p.player);
        CHECK(std::string(n0).find("engineer_d") == 0);
    }
    // F: world boundary
    {
        AppState p;
        sim_reset(p, 1);
        p.player.world_x = 10;
        for (int i = 0; i < 20; ++i) {
            player_move(p, false, false, true, false, 4);
        }
        CHECK(p.player.world_x >= 8);
        p.player.world_y = static_cast<i32>(kWorldH) - 10;
        for (int i = 0; i < 20; ++i) {
            player_move(p, true, false, false, false, 4);
        }
        // moving up decreases y
        CHECK(p.player.world_y < static_cast<i32>(kWorldH) - 10);
    }
    // F: hurt flash timer
    {
        AppState p;
        sim_reset(p, 1);
        player_hurt(p, 10);
        CHECK(p.player.hp == 90);
        CHECK(p.player.hurt_timer > 0);
        const u32 t0 = p.player.hurt_timer;
        sim_step(p, nullptr);
        CHECK(p.player.hurt_timer == t0 - 1);
        for (u32 i = 0; i < 20; ++i) {
            sim_step(p, nullptr);
        }
        CHECK(p.player.hurt_timer == 0);
    }
    // F test: scripted route > one viewport, camera stays valid
    {
        AppState p;
        sim_reset(p, 77);
        const bool r[4] = {false, false, false, true};
        const bool d[4] = {false, true, false, false};  // down
        for (int i = 0; i < 120; ++i) {
            sim_step(p, r);
            camera_follow(p, 640, 360);
            CHECK(p.player.world_x >= 8 && p.player.world_x <= static_cast<i32>(kWorldW) - 8);
            CHECK(p.cam.x >= 0 && p.cam.x + 640 <= static_cast<i32>(kWorldW));
        }
        const i32 x_after_right = p.player.world_x;
        for (int i = 0; i < 80; ++i) {
            sim_step(p, d);
            camera_follow(p, 640, 360);
        }
        CHECK(p.player.world_x == x_after_right);
        CHECK(p.player.world_y > 2048);
        CHECK(p.player.dir == 0);
    }

    // deterministic reset + map
    AppState a, b;
    sim_reset(a, 99);
    sim_reset(b, 99);
    CHECK(a.map == b.map);

    // runtime sprites
    std::vector<SpriteBlob> blobs;
    if (load_runtime_sprites("assets/facility_omega/runtime", blobs)) {
        std::printf("loaded %zu facility sprites\n", blobs.size());
        CHECK(blobs.size() >= 20);
        bool has_eng = false, has_floor = false;
        u32 floor_n = 0;
        for (const auto& bl : blobs) {
            if (bl.name == "engineer_idle") {
                has_eng = true;
                CHECK(bl.width == 32 && bl.height == 32);
            }
            if (bl.name.rfind("floor_", 0) == 0) {
                has_floor = true;
                ++floor_n;
            }
        }
        CHECK(has_eng);
        CHECK(has_floor);
        CHECK(floor_n >= 8);  // E-07
    } else {
        std::printf("WARN facility runtime assets not found (run assetc)\n");
    }

    if (g_fail) {
        std::printf("gpu2d_test_facility FAIL %d\n", g_fail);
        return 1;
    }
    std::printf("gpu2d_test_facility PASS\n");
    return 0;
}
