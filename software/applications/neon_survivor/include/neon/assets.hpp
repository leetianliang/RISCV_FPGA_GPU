#pragma once

#include "gpu2d/graphics_api.hpp"
#include "gpu2d/telemetry.hpp"
#include "gpu2d/types.hpp"
#include "neon/sim.hpp"

#include <vector>

namespace neon {

// Procedural pixel payloads (game-owned data, no Golden types).
struct SpriteBlob {
    std::vector<gpu2d::u8> pixels;  // RGB565 packed
    u32 w = 0;
    u32 h = 0;
    u32 stride = 0;
    bool indexed8 = false;
    std::vector<gpu2d::u32> palette;  // 256 RGBA when indexed8
};

struct AssetBlobs {
    SpriteBlob player;
    SpriteBlob enemy_n;
    SpriteBlob enemy_f;
    SpriteBlob enemy_h;
    SpriteBlob bullet;
    SpriteBlob bullet_e;
    SpriteBlob particle;
    SpriteBlob glow;
    SpriteBlob font;
    SpriteBlob font_index8;
};

AssetBlobs build_procedural_assets();

// Texture handles created by the host/backend layer (game only stores IDs).
struct Assets {
    gpu2d::TextureId player{};
    gpu2d::TextureId enemy_n{};
    gpu2d::TextureId enemy_f{};
    gpu2d::TextureId enemy_h{};
    gpu2d::TextureId bullet{};
    gpu2d::TextureId bullet_e{};
    gpu2d::TextureId particle{};
    gpu2d::TextureId glow{};
    gpu2d::TextureId font{};
    gpu2d::TextureId font_pal{};
};

struct DrawOpts {
    bool hud = true;
    bool tech_hud = false;
    bool xray = false;
    bool show_grid = false;
    bool tile_mode = false;
    u32 tile_size = 32;  // for X-Ray grid when telemetry has no map
    const gpu2d::RendererTelemetry* tel = nullptr;
    double host_fps = 0.0;
    u32 particles_drawn = 0;
    u32 enemies_drawn = 0;
    u32 bullets_drawn = 0;
};

void draw_text(gpu2d::GraphicsApi& api, const Assets& a, i32 x, i32 y, const char* s,
               gpu2d::Color c, u8 alpha = 255);
// Indexed8+Palette glyph path (technical HUD).
void draw_text_pal(gpu2d::GraphicsApi& api, const Assets& a, i32 x, i32 y, const char* s,
                   gpu2d::Color c, u8 alpha = 255);

// Base game scene only — no HUD / tech HUD / X-Ray. Authoritative telemetry source.
void render_scene_base(gpu2d::GraphicsApi& api, const Assets& a, const SimState& sim,
                       const SimConfig& cfg);
// Debug overlay only (HUD / tech HUD / X-Ray). Must not clear or redraw the game.
// opts.tel must be BASE telemetry from the completed base execute, not overlay telemetry.
void render_debug_overlay(gpu2d::GraphicsApi& api, const Assets& a, const SimState& sim,
                          const SimConfig& cfg, const DrawOpts& opts);
// Combined convenience (base + optional overlay) for simple tests.
void render_frame(gpu2d::GraphicsApi& api, const Assets& a, const SimState& sim,
                  const SimConfig& cfg, const DrawOpts& opts);

// Count of submitted draws this frame (for stress thresholds).
struct DrawCounts {
    u32 sprites = 0;
    u32 fills = 0;
    u32 alpha_draws = 0;
    u32 additive_draws = 0;
    u32 scaled_draws = 0;
    u32 bilinear_draws = 0;
    u32 palette_draws = 0;
    u32 dither_draws = 0;
    u32 clipped_draws = 0;
    u32 bullet_draws = 0;  // projectile sprite submissions only
};

DrawCounts last_render_counts();
u32 font_glyph_w();
u32 font_glyph_h();

// R2-08 / F-T3: pure formatting for technical HUD (unit-testable).
struct TechHudStrings {
    char line0[96];
    char line1[96];
    char line2[96];
    char line3[96];
};
TechHudStrings make_tech_hud_strings(const gpu2d::RendererTelemetry& t, double host_fps);

}  // namespace neon
