#include "neon/assets.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace neon {
namespace {

using gpu2d::Color;
using gpu2d::i32;
using gpu2d::u16;
using gpu2d::u32;
using gpu2d::u8;

DrawCounts g_counts{};
constexpr u32 kGlyphW = 8;
constexpr u32 kGlyphH = 8;

// Classic 8x8 font for ASCII 32..90 (space + digits + uppercase + few symbols).
// Each glyph: 8 bytes, MSB = left pixel.
const u8 kFont[96][8] = {
    {0,0,0,0,0,0,0,0}, // 32 space
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, // !
    {0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00}, // "
    {0x6C,0xFE,0x6C,0x6C,0x6C,0xFE,0x6C,0x00}, // #
    {0x18,0x7E,0xC0,0x7C,0x06,0xFC,0x18,0x00}, // $
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, // %
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // &
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // '
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // (
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ,
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // .
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // /
    {0x7C,0xCE,0xDE,0xF6,0xE6,0xC6,0x7C,0x00}, // 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
    {0x7C,0xC6,0x06,0x1C,0x30,0x66,0xFE,0x00}, // 2
    {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00}, // 3
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, // 4
    {0xFE,0xC0,0xC0,0xFC,0x06,0xC6,0x7C,0x00}, // 5
    {0x38,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00}, // 6
    {0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00}, // 7
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, // 8
    {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00}, // 9
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // :
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // ;
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // <
    {0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00}, // =
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, // >
    {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00}, // ?
    {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00}, // @
    {0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // A
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, // B
    {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, // C
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, // D
    {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, // E
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, // F
    {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00}, // G
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // H
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, // J
    {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, // K
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, // L
    {0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00}, // M
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, // N
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // O
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, // P
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xCE,0x7C,0x0E}, // Q
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, // R
    {0x7C,0xC6,0xE0,0x78,0x0E,0xC6,0x7C,0x00}, // S
    {0x7E,0x7E,0x5A,0x18,0x18,0x18,0x3C,0x00}, // T
    {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // U
    {0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, // V
    {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, // W
    {0xC6,0xC6,0x6C,0x38,0x6C,0xC6,0xC6,0x00}, // X
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, // Y
    {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00}, // Z
};

u16 pack565(u8 r, u8 g, u8 b) {
    return static_cast<u16>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void put_px(SpriteBlob& s, u32 x, u32 y, u16 c) {
    if (x >= s.w || y >= s.h) {
        return;
    }
    s.pixels[static_cast<size_t>(y) * s.stride + x * 2] = static_cast<u8>(c & 0xFF);
    s.pixels[static_cast<size_t>(y) * s.stride + x * 2 + 1] = static_cast<u8>(c >> 8);
}

void fill_blob(SpriteBlob& s, u32 w, u32 h) {
    s.w = w;
    s.h = h;
    s.stride = w * 2;
    s.pixels.assign(static_cast<size_t>(s.stride) * h, 0);
}

SpriteBlob make_player() {
    SpriteBlob s;
    fill_blob(s, 16, 16);
    const u16 body = pack565(40, 220, 255);
    const u16 wing = pack565(20, 80, 180);
    const u16 cock = pack565(255, 240, 120);
    for (u32 y = 2; y < 14; ++y) {
        put_px(s, 7, y, body);
        put_px(s, 8, y, body);
    }
    for (u32 x = 4; x < 12; ++x) {
        put_px(s, x, 10, wing);
        put_px(s, x, 11, wing);
    }
    put_px(s, 7, 4, cock);
    put_px(s, 8, 4, cock);
    put_px(s, 6, 13, pack565(255, 120, 40));
    put_px(s, 9, 13, pack565(255, 120, 40));
    return s;
}

SpriteBlob make_enemy(EnemyKind k) {
    SpriteBlob s;
    u16 a, b;
    u32 sz = 12;
    if (k == EnemyKind::Normal) {
        a = pack565(255, 80, 120);
        b = pack565(160, 20, 60);
        sz = 12;
    } else if (k == EnemyKind::Fast) {
        a = pack565(255, 220, 40);
        b = pack565(180, 100, 0);
        sz = 10;
    } else {
        a = pack565(180, 80, 255);
        b = pack565(80, 20, 140);
        sz = 20;
    }
    fill_blob(s, sz, sz);
    const i32 c = static_cast<i32>(sz / 2);
    for (i32 y = 0; y < static_cast<i32>(sz); ++y) {
        for (i32 x = 0; x < static_cast<i32>(sz); ++x) {
            const i32 dx = x - c;
            const i32 dy = y - c;
            if (dx * dx + dy * dy <= c * c) {
                put_px(s, static_cast<u32>(x), static_cast<u32>(y), (dx + dy) & 1 ? a : b);
            }
        }
    }
    return s;
}

SpriteBlob make_bullet(bool enemy) {
    SpriteBlob s;
    fill_blob(s, 6, 6);
    const u16 col = enemy ? pack565(255, 60, 60) : pack565(80, 255, 200);
    const u16 core = enemy ? pack565(255, 200, 200) : pack565(220, 255, 255);
    for (u32 y = 1; y < 5; ++y) {
        for (u32 x = 1; x < 5; ++x) {
            put_px(s, x, y, (x == 2 || x == 3) && (y == 2 || y == 3) ? core : col);
        }
    }
    return s;
}

SpriteBlob make_particle() {
    SpriteBlob s;
    fill_blob(s, 4, 4);
    const u16 c = pack565(255, 255, 255);
    put_px(s, 1, 1, c);
    put_px(s, 2, 1, c);
    put_px(s, 1, 2, c);
    put_px(s, 2, 2, c);
    return s;
}

SpriteBlob make_glow() {
    SpriteBlob s;
    fill_blob(s, 32, 32);
    for (i32 y = 0; y < 32; ++y) {
        for (i32 x = 0; x < 32; ++x) {
            const float dx = x - 15.5f;
            const float dy = y - 15.5f;
            const float d = std::sqrt(dx * dx + dy * dy) / 16.0f;
            if (d < 1.0f) {
                const u8 v = static_cast<u8>(255 * (1.0f - d));
                put_px(s, static_cast<u32>(x), static_cast<u32>(y),
                       pack565(v, static_cast<u8>(v / 2), v));
            }
        }
    }
    return s;
}

SpriteBlob make_font() {
    // 96 glyphs × 8×8, color key = pure magenta
    SpriteBlob s;
    const u32 cols = 16;
    const u32 rows = 6;
    fill_blob(s, cols * kGlyphW, rows * kGlyphH);
    const u16 key = pack565(255, 0, 255);
    const u16 fg = pack565(255, 255, 255);
    for (u32 y = 0; y < s.h; ++y) {
        for (u32 x = 0; x < s.w; ++x) {
            put_px(s, x, y, key);
        }
    }
    for (u32 gi = 0; gi < 96; ++gi) {
        const u32 gx = (gi % cols) * kGlyphW;
        const u32 gy = (gi / cols) * kGlyphH;
        for (u32 row = 0; row < 8; ++row) {
            const u8 bits = kFont[gi][row];
            for (u32 col = 0; col < 8; ++col) {
                if (bits & (0x80u >> col)) {
                    put_px(s, gx + col, gy + row, fg);
                }
            }
        }
    }
    return s;
}

SpriteBlob make_font_index8() {
    SpriteBlob s;
    s.indexed8 = true;
    const u32 cols = 16;
    const u32 rows = 6;
    s.w = cols * kGlyphW;
    s.h = rows * kGlyphH;
    s.stride = s.w;
    s.pixels.assign(static_cast<size_t>(s.w) * s.h, 0);
    s.palette.assign(256, 0);
    s.palette[0] = 0;                       // transparent / key
    s.palette[1] = 0xFFFFFFFFu;             // white
    s.palette[2] = 0xFF40E0FFu;             // cyan accent
    for (u32 gi = 0; gi < 96; ++gi) {
        const u32 gx = (gi % cols) * kGlyphW;
        const u32 gy = (gi / cols) * kGlyphH;
        for (u32 row = 0; row < 8; ++row) {
            const u8 bits = kFont[gi][row];
            for (u32 col = 0; col < 8; ++col) {
                if (bits & (0x80u >> col)) {
                    s.pixels[static_cast<size_t>(gy + row) * s.w + gx + col] = 1;
                }
            }
        }
    }
    return s;
}

}  // namespace

u32 font_glyph_w() { return kGlyphW; }
u32 font_glyph_h() { return kGlyphH; }
DrawCounts last_render_counts() { return g_counts; }

AssetBlobs build_procedural_assets() {
    AssetBlobs a;
    a.player = make_player();
    a.enemy_n = make_enemy(EnemyKind::Normal);
    a.enemy_f = make_enemy(EnemyKind::Fast);
    a.enemy_h = make_enemy(EnemyKind::Heavy);
    a.bullet = make_bullet(false);
    a.bullet_e = make_bullet(true);
    a.particle = make_particle();
    a.glow = make_glow();
    a.font = make_font();
    a.font_index8 = make_font_index8();
    return a;
}

void draw_text(gpu2d::GraphicsApi& api, const Assets& a, i32 x, i32 y, const char* s,
               Color c, u8 alpha) {
    if (!s || !a.font.valid()) {
        return;
    }
    i32 cx = x;
    for (const char* p = s; *p; ++p) {
        unsigned ch = static_cast<unsigned char>(*p);
        if (ch == '\n') {
            cx = x;
            y += static_cast<i32>(kGlyphH) + 1;
            continue;
        }
        if (ch < 32 || ch > 127) {
            ch = '?';
        }
        // Skip glyphs that would be fully off-screen (avoid negative-rect faults).
        if (cx + static_cast<i32>(kGlyphW) <= 0 || y + static_cast<i32>(kGlyphH) <= 0) {
            cx += static_cast<i32>(kGlyphW);
            continue;
        }
        const u32 gi = ch - 32;
        const u32 gx = (gi % 16) * kGlyphW;
        const u32 gy = (gi / 16) * kGlyphH;
        gpu2d::SpriteParams sp;
        sp.tex = a.font;
        sp.dst_x = cx < 0 ? 0 : cx;
        sp.dst_y = y < 0 ? 0 : y;
        sp.w = kGlyphW;
        sp.h = kGlyphH;
        sp.src_x = gx;
        sp.src_y = gy;
        sp.color_key = true;
        // Canonical 24-bit RGB magenta (not RGB565 packed 0xF81F).
        sp.color_key_rgb = 0x00FF00FFu;
        sp.color_mod = (c.r != 255 || c.g != 255 || c.b != 255);
        sp.mod = c;
        sp.blend = alpha < 255 ? gpu2d::BlendMode::StraightAlpha : gpu2d::BlendMode::Copy;
        sp.global_alpha = alpha;
        api.draw_sprite(sp);
        ++g_counts.sprites;
        cx += static_cast<i32>(kGlyphW);
    }
}

namespace {

void draw_enemy(gpu2d::GraphicsApi& api, const Assets& a, const Enemy& e) {
    gpu2d::SpriteParams sp;
    switch (e.kind) {
        case EnemyKind::Fast:
            sp.tex = a.enemy_f;
            sp.w = 10;
            sp.h = 10;
            break;
        case EnemyKind::Heavy:
            sp.tex = a.enemy_h;
            sp.w = 20;
            sp.h = 20;
            break;
        case EnemyKind::Normal:
        default:
            sp.tex = a.enemy_n;
            sp.w = 12;
            sp.h = 12;
            break;
    }
    sp.dst_x = static_cast<i32>(e.x) - static_cast<i32>(sp.w / 2);
    sp.dst_y = static_cast<i32>(e.y) - static_cast<i32>(sp.h / 2);
    if (e.flash) {
        sp.color_mod = true;
        sp.mod = Color::rgb(255, 255, 255);
    }
    if (e.kind == EnemyKind::Heavy) {
        // Visible Bilinear scale (FX-03): large Heavy body pulse.
        sp.scale_w = 24;
        sp.scale_h = 24;
        sp.dst_x = static_cast<i32>(e.x) - 12;
        sp.dst_y = static_cast<i32>(e.y) - 12;
        sp.filter = gpu2d::FilterMode::Bilinear;
        ++g_counts.scaled_draws;
        ++g_counts.bilinear_draws;
    }
    api.draw_sprite(sp);
    ++g_counts.sprites;
}

void draw_bullet(gpu2d::GraphicsApi& api, const Assets& a, const Bullet& b) {
    gpu2d::SpriteParams sp;
    sp.tex = b.enemy ? a.bullet_e : a.bullet;
    sp.w = 6;
    sp.h = 6;
    sp.dst_x = static_cast<i32>(b.x) - 3;
    sp.dst_y = static_cast<i32>(b.y) - 3;
    sp.blend = gpu2d::BlendMode::AddSat;
    api.draw_sprite(sp);
    ++g_counts.sprites;
    ++g_counts.additive_draws;
}

void draw_particle(gpu2d::GraphicsApi& api, const Assets& a, const Particle& p) {
    gpu2d::SpriteParams sp;
    if (p.mode == 1) {
        sp.tex = a.glow;
        const i32 sz = static_cast<i32>(16 * p.scale);
        sp.scale_w = sz;
        sp.scale_h = sz;
        sp.w = 32;
        sp.h = 32;
        sp.dst_x = static_cast<i32>(p.x) - sz / 2;
        sp.dst_y = static_cast<i32>(p.y) - sz / 2;
        sp.blend = gpu2d::BlendMode::AddSat;
        sp.global_alpha = static_cast<u8>((p.life * 255) / (p.max_life ? p.max_life : 1));
        // Default competition path: dither + bilinear on large glow (F-05 / F-09).
        sp.filter = gpu2d::FilterMode::Bilinear;
        sp.dither = true;
        ++g_counts.additive_draws;
        ++g_counts.scaled_draws;
        ++g_counts.bilinear_draws;
        ++g_counts.dither_draws;
    } else {
        sp.tex = a.particle;
        sp.w = 4;
        sp.h = 4;
        const i32 sc = p.scale > 1.2f ? 8 : 4;
        if (sc != 4) {
            sp.scale_w = sc;
            sp.scale_h = sc;
            ++g_counts.scaled_draws;
        }
        sp.dst_x = static_cast<i32>(p.x) - 2;
        sp.dst_y = static_cast<i32>(p.y) - 2;
        sp.blend = gpu2d::BlendMode::StraightAlpha;
        sp.global_alpha = static_cast<u8>((p.life * 255) / (p.max_life ? p.max_life : 1));
        sp.color_mod = true;
        sp.mod = Color::rgb(p.r, p.g, p.b);
        ++g_counts.alpha_draws;
    }
    api.draw_sprite(sp);
    ++g_counts.sprites;
}

}  // namespace

void draw_text_pal(gpu2d::GraphicsApi& api, const Assets& a, i32 x, i32 y, const char* s,
                   Color c, u8 alpha) {
    // Indexed8 + Palette HUD path (FX-05).
    if (!s || !a.font_pal.valid()) {
        draw_text(api, a, x, y, s, c, alpha);
        return;
    }
    i32 cx = x;
    for (const char* p = s; *p; ++p) {
        unsigned ch = static_cast<unsigned char>(*p);
        if (ch == '\n') {
            cx = x;
            y += static_cast<i32>(kGlyphH) + 1;
            continue;
        }
        if (ch < 32 || ch > 127) {
            ch = '?';
        }
        if (cx + static_cast<i32>(kGlyphW) <= 0 || y + static_cast<i32>(kGlyphH) <= 0) {
            cx += static_cast<i32>(kGlyphW);
            continue;
        }
        const u32 gi = ch - 32;
        const u32 gx = (gi % 16) * kGlyphW;
        const u32 gy = (gi / 16) * kGlyphH;
        gpu2d::SpriteParams sp;
        sp.tex = a.font_pal;
        sp.dst_x = cx < 0 ? 0 : cx;
        sp.dst_y = y < 0 ? 0 : y;
        sp.w = kGlyphW;
        sp.h = kGlyphH;
        sp.src_x = gx;
        sp.src_y = gy;
        sp.palette = true;
        sp.color_mod = (c.r != 255 || c.g != 255 || c.b != 255);
        sp.mod = c;
        sp.blend = alpha < 255 ? gpu2d::BlendMode::StraightAlpha : gpu2d::BlendMode::Copy;
        sp.global_alpha = alpha;
        api.draw_sprite(sp);
        ++g_counts.sprites;
        ++g_counts.palette_draws;
        cx += static_cast<i32>(kGlyphW);
    }
}

void render_scene_base(gpu2d::GraphicsApi& api, const Assets& a, const SimState& sim,
                       const SimConfig& cfg) {
    g_counts = DrawCounts{};
    // Background: two-layer parallax fills
    const u32 sc = static_cast<u32>(sim.frame);
    api.fill_rect(0, 0, cfg.width, cfg.height, Color::rgb(8, 10, 24));
    for (int i = 0; i < 24; ++i) {
        const u32 sx = (i * 97u + sc / 3) % cfg.width;
        const u32 sy = (i * 53u) % cfg.height;
        api.fill_rect(static_cast<i32>(sx), static_cast<i32>(sy), 2, 2,
                      Color::rgb(40, 50, 90));
        ++g_counts.fills;
    }
    for (u32 x = 0; x < cfg.width; x += 64) {
        api.fill_rect(static_cast<i32>((x + sc / 2) % cfg.width), 0, 1, cfg.height,
                      Color::rgb(16, 24, 48));
        ++g_counts.fills;
    }

    // Periodic bilinear shockwave (FX-03)
    {
        const u32 period = 90;
        const u32 ph = sc % period;
        if (ph < 40) {
            const i32 sz = static_cast<i32>(16 + ph * 3);
            gpu2d::SpriteParams sp;
            sp.tex = a.glow;
            sp.w = 32;
            sp.h = 32;
            sp.scale_w = sz;
            sp.scale_h = sz;
            sp.dst_x = static_cast<i32>(cfg.width / 2) - sz / 2;
            sp.dst_y = static_cast<i32>(cfg.height / 2) - sz / 2;
            sp.blend = gpu2d::BlendMode::AddSat;
            sp.filter = gpu2d::FilterMode::Bilinear;
            sp.dither = true;
            sp.global_alpha = static_cast<u8>(255 - ph * 6);
            api.draw_sprite(sp);
            ++g_counts.sprites;
            ++g_counts.scaled_draws;
            ++g_counts.bilinear_draws;
            ++g_counts.additive_draws;
            ++g_counts.dither_draws;
        }
    }

    for (const auto& p : sim.particles) {
        if (p.life) {
            draw_particle(api, a, p);
        }
    }
    for (const auto& e : sim.enemies) {
        if (e.alive) {
            draw_enemy(api, a, e);
        }
    }
    {
        gpu2d::SpriteParams sp;
        sp.tex = a.player;
        sp.w = 16;
        sp.h = 16;
        sp.dst_x = static_cast<i32>(sim.player.x) - 8;
        sp.dst_y = static_cast<i32>(sim.player.y) - 8;
        if (sim.player.flash || (sim.player.invuln && (sim.frame & 2))) {
            sp.color_mod = true;
            sp.mod = Color::rgb(255, 120, 120);
        }
        api.draw_sprite(sp);
        ++g_counts.sprites;
    }
    for (const auto& b : sim.bullets) {
        if (b.alive) {
            draw_bullet(api, a, b);
        }
    }
}

void render_debug_overlay(gpu2d::GraphicsApi& api, const Assets& a, const SimState& sim,
                          const SimConfig& cfg, const DrawOpts& opts) {
    // Clip path demo: HUD panel (clipped_fill)
    if (opts.hud) {
        api.set_clip(true, 0, 0, static_cast<i32>(cfg.width), 14);
        api.fill_rect(0, 0, cfg.width, 14, Color::rgba(0, 0, 0, 160));
        api.clear_clip();
        ++g_counts.clipped_draws;
        char buf[128];
        u32 en = 0, bl = 0, pt = 0;
        for (const auto& e : sim.enemies) {
            en += e.alive ? 1 : 0;
        }
        for (const auto& b : sim.bullets) {
            bl += b.alive ? 1 : 0;
        }
        for (const auto& p : sim.particles) {
            pt += p.life ? 1 : 0;
        }
        std::snprintf(buf, sizeof(buf), "HP %d  TIME %u  KILLS %u  SCORE %u", sim.player.hp,
                      static_cast<u32>(sim.frame / 60), sim.kills, sim.score);
        draw_text_pal(api, a, 4, 3, buf, Color::rgb(180, 255, 255));
        std::snprintf(buf, sizeof(buf), "EN %u  BL %u  PT %u  MODE %s", en, bl, pt,
                      opts.tile_mode ? "TILE32" : "IMMEDIATE");
        const i32 right =
            static_cast<i32>(cfg.width) > 240 ? static_cast<i32>(cfg.width) - 220 : 4;
        const i32 ry = static_cast<i32>(cfg.width) > 240 ? 3 : 16;
        draw_text_pal(api, a, right, ry, buf, Color::rgb(255, 200, 80));
    }

    if (opts.tech_hud && opts.tel) {
        const auto& t = *opts.tel;
        char buf[192];
        i32 y = static_cast<i32>(cfg.height) - 70;
        api.fill_rect(0, y - 2, 280, 68, Color::rgba(0, 0, 0, 180));
        std::snprintf(buf, sizeof(buf), "CMD %u  SPR %u  WREF %u", t.command_count,
                      t.sprite_count, t.workref_count);
        draw_text_pal(api, a, 4, y, buf, Color::rgb(120, 255, 160));
        std::snprintf(buf, sizeof(buf), "TILE %u/%u MAXREF %u MAXOD %u", t.tiles_active,
                      t.tiles_total, t.max_workrefs_per_tile, t.max_overdraw);
        draw_text_pal(api, a, 4, y + 12, buf, Color::rgb(120, 255, 160));
        if (opts.host_fps > 0.0) {
            std::snprintf(buf, sizeof(buf), "PC GOLDEN HOST FPS %.1f", opts.host_fps);
        } else {
            std::snprintf(buf, sizeof(buf), "PC GOLDEN HOST FPS N/A");
        }
        draw_text_pal(api, a, 4, y + 24, buf, Color::rgb(255, 120, 120));
        std::snprintf(buf, sizeof(buf), "GRID %ux%u TILE %u", t.grid_w, t.grid_h, t.tile_size);
        draw_text_pal(api, a, 4, y + 36, buf, Color::rgb(160, 160, 255));
    }

    // Architecture X-Ray (uses opts.tel — must be BASE telemetry, not overlay).
    if (opts.xray) {
        const u32 ts = opts.tile_size ? opts.tile_size : 32u;
        const u32 gw = (cfg.width + ts - 1) / ts;
        const u32 gh = (cfg.height + ts - 1) / ts;
        const bool have_map =
            opts.tel && opts.tel->has_workref_map && opts.tel->tile_workrefs;
        for (u32 ty = 0; ty <= gh; ++ty) {
            const i32 y = static_cast<i32>(ty * ts);
            if (y >= static_cast<i32>(cfg.height)) {
                break;
            }
            api.fill_rect(0, y, cfg.width, 1, Color::rgba(0, 255, 255, 90));
        }
        for (u32 tx = 0; tx <= gw; ++tx) {
            const i32 x = static_cast<i32>(tx * ts);
            if (x >= static_cast<i32>(cfg.width)) {
                break;
            }
            api.fill_rect(x, 0, 1, cfg.height, Color::rgba(0, 255, 255, 90));
        }
        if (have_map) {
            const auto& t = *opts.tel;
            for (u32 ty = 0; ty < gh && ty < t.grid_h; ++ty) {
                for (u32 tx = 0; tx < gw && tx < t.grid_w; ++tx) {
                    const u16 wc = t.tile_workrefs[ty * t.grid_w + tx];
                    if (wc == 0) {
                        continue;
                    }
                    const i32 x = static_cast<i32>(tx) * static_cast<i32>(ts);
                    const i32 y = static_cast<i32>(ty) * static_cast<i32>(ts);
                    const u8 v = static_cast<u8>(wc > 32 ? 255 : (wc * 8));
                    api.fill_rect(x + 2, y + 2, 6, 6, Color::rgba(v, 80, 255, 160));
                    if (t.has_overdraw && t.overdraw) {
                        const u32 px = static_cast<u32>(x + static_cast<i32>(ts) / 2);
                        const u32 py = static_cast<u32>(y + static_cast<i32>(ts) / 2);
                        if (px < cfg.width && py < cfg.height) {
                            const u32 od = t.overdraw[py * cfg.width + px];
                            if (od >= 2) {
                                const u8 o = static_cast<u8>(od > 16 ? 255 : od * 16);
                                api.fill_rect(x + static_cast<i32>(ts) - 8, y + 2, 6, 6,
                                              Color::rgba(255, o, 40, 160));
                            }
                        }
                    }
                }
            }
        }
        char buf[96];
        std::snprintf(buf, sizeof(buf), "X-RAY %s BASE-TEL%s",
                      opts.tile_mode ? "TILE32" : "IMM", have_map ? "" : " NO-MAP");
        draw_text(api, a, 4, static_cast<i32>(cfg.height) - 16, buf, Color::rgb(0, 255, 255));
    }
}

void render_frame(gpu2d::GraphicsApi& api, const Assets& a, const SimState& sim,
                  const SimConfig& cfg, const DrawOpts& opts) {
    render_scene_base(api, a, sim, cfg);
    if (opts.hud || opts.tech_hud || opts.xray) {
        render_debug_overlay(api, a, sim, cfg, opts);
    }
}

}  // namespace neon
