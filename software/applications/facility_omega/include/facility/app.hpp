#pragma once

#include "gpu2d/graphics_api.hpp"
#include "gpu2d/types.hpp"

#include <string>
#include <vector>

namespace facility {

using gpu2d::i32;
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
};

struct TexRef {
    gpu2d::TextureId id{};
    u32 w = 0;
    u32 h = 0;
    u32 ax = 0;
    u32 ay = 0;
};

class TexBank {
public:
    void put(const std::string& n, TexRef t) { items_.push_back({n, t}); }
    const TexRef* find(const std::string& n) const {
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

// App state: world + camera + player (vertical slice skeleton).
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
    u32 kills = 0;
    u32 hurt_timer = 0;  // frames of Color-Mod flash remaining
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
    std::vector<u8> map;  // kMapTiles*kMapTiles tile index
    bool ready = false;
};

void sim_reset(AppState& s, u32 seed);
void camera_follow(AppState& s, u32 view_w, u32 view_h);
void player_move(AppState& s, bool up, bool down, bool left, bool right, i32 speed = 3);
void player_hurt(AppState& s, i32 damage);
void sim_step(AppState& s, const bool* keys /*UDLR*/);

inline i32 world_to_screen_x(const AppState& s, i32 wx) { return wx - s.cam.x; }
inline i32 world_to_screen_y(const AppState& s, i32 wy) { return wy - s.cam.y; }

// Inclusive visible MapTile range with optional guard band.
void visible_map_range(const AppState& s, u32 view_w, u32 view_h, i32 guard,
                       i32& tx0, i32& ty0, i32& tx1, i32& ty1);

// 2-frame walk cycle index (0/1) from player.frame.
inline u32 player_walk_frame(const Player& p) { return (p.frame >> 3) & 1u; }

// Sprite name for current player state.
const char* player_sprite_name(const Player& p);

// Draw visible map + player using screen-space API (world already camera-relative).
void render_scene(gpu2d::GraphicsApi& api, const AppState& s, const TexBank& tex,
                  u32 view_w, u32 view_h);

}  // namespace facility
