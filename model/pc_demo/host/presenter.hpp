#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace host {

using u32 = std::uint32_t;

struct InputState {
    bool up = false, down = false, left = false, right = false;
    bool quit = false;
    bool f1 = false, f2 = false, f3 = false, f4 = false, f5 = false;
    bool f6 = false, f7 = false, f8 = false, f9 = false, f10 = false;
    bool pause = false, reset = false;
    bool edge_up = false, edge_down = false, edge_left = false, edge_right = false;
    bool edge_quit = false;
    bool edge_f1 = false, edge_f2 = false, edge_f3 = false, edge_f4 = false, edge_f5 = false;
    bool edge_f6 = false, edge_f7 = false, edge_f8 = false, edge_f9 = false, edge_f10 = false;
    bool edge_pause = false, edge_reset = false;
    bool edge_1 = false, edge_2 = false, edge_3 = false, edge_4 = false, edge_5 = false;
    void clear_edges() {
        edge_up = edge_down = edge_left = edge_right = edge_quit = false;
        edge_f1 = edge_f2 = edge_f3 = edge_f4 = edge_f5 = false;
        edge_f6 = edge_f7 = edge_f8 = edge_f9 = edge_f10 = false;
        edge_pause = edge_reset = false;
        edge_1 = edge_2 = edge_3 = edge_4 = edge_5 = false;
    }
};

// Presenter: window + RGB565 framebuffer upload. Does NOT draw game content.
class Presenter {
public:
    Presenter() = default;
    ~Presenter();

    bool open(u32 fb_w, u32 fb_h, u32 window_w, u32 window_h, bool headless,
              const std::string& title);
    void close();
    bool is_open() const noexcept { return open_; }
    bool headless() const noexcept { return headless_; }

    // Pump OS events; updates input. Returns false if quit requested.
    bool pump(InputState& in);
    // Upload completed RGB565 (or ARGB8888) framebuffer and present.
    void present(const uint8_t* fb, u32 w, u32 h, u32 stride, bool rgb565);

private:
    bool open_ = false;
    bool headless_ = false;
    u32 fb_w_ = 0, fb_h_ = 0;
    u32 win_w_ = 0, win_h_ = 0;
    std::vector<uint32_t> converted_;
#ifdef _WIN32
    void* hwnd_ = nullptr;
    void* hbmp_ = nullptr;
    void* bits_ = nullptr;
    void* hdc_ = nullptr;
#endif
};

// RGB565 → XRGB8888 conversion (presentation-only, after Golden render).
void rgb565_to_xrgb8888(const uint8_t* src, uint32_t* dst, size_t pixels);

}  // namespace host
