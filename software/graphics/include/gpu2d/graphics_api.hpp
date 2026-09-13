#pragma once

#include "gpu2d/types.hpp"

namespace gpu2d {

// Application-facing Graphics API. Backend-neutral.
// Records an ordered Draw2D stream; does not touch Golden internals.
class GraphicsApi {
public:
    virtual ~GraphicsApi() = default;

    virtual void begin_frame() = 0;
    virtual void set_clip(bool enable, i32 xmin, i32 ymin, i32 xmax, i32 ymax) = 0;
    virtual void clear_clip() = 0;
    virtual void fill_rect(i32 x, i32 y, u32 w, u32 h, Color c) = 0;
    virtual void draw_sprite(const SpriteParams& p) = 0;
    // Convenience wrappers implemented on top of draw_sprite.
    virtual void draw_sprite_key(const SpriteParams& p, u32 key_rgb) {
        SpriteParams s = p;
        s.color_key = true;
        s.color_key_rgb = key_rgb;
        draw_sprite(s);
    }
    virtual void draw_sprite_alpha(const SpriteParams& p, u8 alpha) {
        SpriteParams s = p;
        s.global_alpha = alpha;
        s.blend = BlendMode::StraightAlpha;
        draw_sprite(s);
    }
    virtual void draw_sprite_scaled(const SpriteParams& p, i32 dw, i32 dh) {
        SpriteParams s = p;
        s.scale_w = dw;
        s.scale_h = dh;
        draw_sprite(s);
    }
    virtual void present() = 0;
};

}  // namespace gpu2d
