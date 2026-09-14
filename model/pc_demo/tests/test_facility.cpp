#include "facility/app.hpp"

#include <cstdio>
#include <cstring>

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
    // MAP: world size freeze
    CHECK(kWorldW == 4096);
    CHECK(kWorldH == 4096);
    CHECK(kMapTiles == 128);
    CHECK(kMapTileSize == 32);

    AppState s;
    sim_reset(s, 1234);
    CHECK(s.ready);
    CHECK(s.map.size() == kMapTiles * kMapTiles);
    CHECK(s.player.world_x == static_cast<i32>(kWorldW / 2));

    // camera center then clamp left/top
    camera_follow(s, 640, 360);
    CHECK(s.cam.x == static_cast<i32>(kWorldW / 2) - 320);
    CHECK(s.cam.y == static_cast<i32>(kWorldH / 2) - 180);
    s.player.world_x = 0;
    s.player.world_y = 0;
    player_move(s, true, false, true, false);  // still at boundary
    camera_follow(s, 640, 360);
    CHECK(s.cam.x == 0);
    CHECK(s.cam.y == 0);
    // right/bottom clamp
    s.player.world_x = static_cast<i32>(kWorldW) - 1;
    s.player.world_y = static_cast<i32>(kWorldH) - 1;
    camera_follow(s, 640, 360);
    CHECK(s.cam.x == static_cast<i32>(kWorldW) - 640);
    CHECK(s.cam.y == static_cast<i32>(kWorldH) - 360);

    // world→screen
    s.player.world_x = 2000;
    s.player.world_y = 1500;
    camera_follow(s, 640, 360);
    const i32 sx = s.player.world_x - s.cam.x;
    const i32 sy = s.player.world_y - s.cam.y;
    CHECK(sx >= 0 && sx < 640);
    CHECK(sy >= 0 && sy < 360);

    // visible MapTile range
    const i32 tx0 = s.cam.x / 32;
    const i32 ty0 = s.cam.y / 32;
    const i32 tx1 = (s.cam.x + 640) / 32;
    CHECK(tx1 - tx0 <= 22);  // ~20 + guard
    CHECK(tx0 >= 0);

    // move > one screen
    const i32 x0 = s.cam.x;
    for (int i = 0; i < 250; ++i) {
        player_move(s, false, false, false, true, 4);  // right 1000px
    }
    camera_follow(s, 640, 360);
    CHECK(s.cam.x > x0);
    CHECK(s.player.world_x > 2000 + 640);

    // deterministic reset
    AppState a, b;
    sim_reset(a, 99);
    sim_reset(b, 99);
    CHECK(a.map == b.map);

    // runtime sprites load (if assets present)
    std::vector<SpriteBlob> blobs;
    if (load_runtime_sprites("assets/facility_omega/runtime", blobs)) {
        std::printf("loaded %zu facility sprites\n", blobs.size());
        CHECK(blobs.size() >= 20);
        bool has_eng = false, has_floor = false;
        for (const auto& bl : blobs) {
            if (bl.name == "engineer_idle") {
                has_eng = true;
                CHECK(bl.width == 32 && bl.height == 32);
            }
            if (bl.name == "floor_00") {
                has_floor = true;
            }
        }
        CHECK(has_eng);
        CHECK(has_floor);
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
