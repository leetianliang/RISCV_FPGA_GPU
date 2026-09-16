#pragma once
#include "gpu2d/graphics_api.hpp"
#include <vector>

namespace facility {
using gpu2d::i32;
using gpu2d::u32;
class TexBank;
struct Bounds { i32 x, y, w, h; };
struct Zone { const char* name; Bounds bounds; };
struct Structure { Bounds bounds; bool power = false; };
struct EnvironmentArt {
    const char* sprite;
    i32 x, y;
    bool opaque = false;
    gpu2d::u8 alpha = 255;
};
struct Environment {
    Bounds hero{1536,1536,1024,1024};
    std::vector<Zone> zones;
    std::vector<Structure> structures;
    std::vector<Bounds> collision;
    std::vector<EnvironmentArt> props;
    std::vector<EnvironmentArt> decals;
    std::vector<EnvironmentArt> effects;
};
Environment make_hero_environment();
bool position_clear(const Environment&, i32 x, i32 y, i32 radius);
void move_in_environment(const Environment&, i32& x, i32& y, i32 dx, i32 dy, i32 radius);
void move_enemy_in_environment(const Environment&, i32& x, i32& y, i32 dx, i32 dy,
                               i32 radius, i32 target_x, i32 target_y);
void render_environment(gpu2d::GraphicsApi&, const Environment&, const TexBank&,
                        i32 camera_x, i32 camera_y, u32 width, u32 height, bool debug=false);
}
