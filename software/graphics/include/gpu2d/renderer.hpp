#pragma once

#include "gpu2d/graphics_api.hpp"
#include "gpu2d/resource.hpp"
#include "gpu2d/types.hpp"

#include <memory>
#include <vector>

namespace gpu2d {

// Low-level recorded command (backend-neutral).
enum class RecOp : u32 { Fill, Sprite };

struct RecCommand {
    RecOp op = RecOp::Fill;
    // Fill
    i32 fx = 0, fy = 0;
    u32 fw = 0, fh = 0;
    Color fcolor{};
    // Sprite
    SpriteParams sp{};
    // Clip (applies to subsequent draws until next set)
    bool clip_en = false;
    ClipRect clip{};
};

class CommandRecorder final : public GraphicsApi {
public:
    void begin_frame() override {
        cmds_.clear();
        clip_ = ClipRect{};
        clip_en_ = false;
    }
    void set_clip(bool enable, i32 xmin, i32 ymin, i32 xmax, i32 ymax) override {
        clip_en_ = enable;
        clip_ = ClipRect{xmin, ymin, xmax, ymax, enable};
    }
    void clear_clip() override { set_clip(false, 0, 0, 0x7FFF, 0x7FFF); }
    void fill_rect(i32 x, i32 y, u32 w, u32 h, Color c) override {
        RecCommand cmd;
        cmd.op = RecOp::Fill;
        cmd.fx = x;
        cmd.fy = y;
        cmd.fw = w;
        cmd.fh = h;
        cmd.fcolor = c;
        cmd.clip_en = clip_en_;
        cmd.clip = clip_;
        cmds_.push_back(cmd);
    }
    void draw_sprite(const SpriteParams& p) override {
        RecCommand cmd;
        cmd.op = RecOp::Sprite;
        cmd.sp = p;
        cmd.clip_en = clip_en_;
        cmd.clip = clip_;
        cmds_.push_back(cmd);
    }
    void present() override { /* backends consume cmds_ at present */ }

    const std::vector<RecCommand>& commands() const noexcept { return cmds_; }
    u32 command_count() const noexcept { return static_cast<u32>(cmds_.size()); }
    u32 sprite_count() const noexcept {
        u32 n = 0;
        for (const auto& c : cmds_) {
            if (c.op == RecOp::Sprite) {
                ++n;
            }
        }
        return n;
    }

private:
    std::vector<RecCommand> cmds_;
    ClipRect clip_{};
    bool clip_en_ = false;
};

}  // namespace gpu2d
