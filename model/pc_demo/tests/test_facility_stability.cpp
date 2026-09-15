// L-01..L-03: 600-frame headless gameplay stability + deterministic sim hash.
#include "facility/app.hpp"

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
    using namespace facility;
    AppState a, b;
    sim_reset(a, 1234);
    sim_reset(b, 1234);
    set_viewport(a, 640, 360);
    set_viewport(b, 640, 360);
    for (int i = 0; i < 600; ++i) {
        // headless: auto-resume upgrades so loop cannot stall
        if (a.level_up_pending) {
            apply_upgrade(a, 0);
        }
        if (b.level_up_pending) {
            apply_upgrade(b, 0);
        }
        sim_step(a, nullptr);
        sim_step(b, nullptr);
        camera_follow(a, 640, 360);
        camera_follow(b, 640, 360);
        CHECK(a.frame == b.frame);
    }
    CHECK(a.frame == 600);
    CHECK(hash_sim(a) == hash_sim(b));
    CHECK(a.player.level == b.player.level);
    CHECK(a.player.kills == b.player.kills);
    // resource integrity: vectors stay within reserve caps
    CHECK(a.enemies.size() <= 256);
    CHECK(a.bullets.size() <= 256);
    CHECK(a.xp_gems.size() <= 512);
    std::printf("facility 600-frame stability level=%u kills=%u hash=%08X\n",
                a.player.level, a.player.kills, hash_sim(a));
    if (g_fail) {
        std::printf("gpu2d_test_facility_stability FAIL %d\n", g_fail);
        return 1;
    }
    std::printf("gpu2d_test_facility_stability PASS\n");
    return 0;
}
