#include "presenter.hpp"

#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace host {

void rgb565_to_xrgb8888(const uint8_t* src, uint32_t* dst, size_t pixels) {
    for (size_t i = 0; i < pixels; ++i) {
        const uint16_t p = static_cast<uint16_t>(src[i * 2] | (src[i * 2 + 1] << 8));
        const uint32_t r = (p >> 11) & 0x1F;
        const uint32_t g = (p >> 5) & 0x3F;
        const uint32_t b = p & 0x1F;
        const uint32_t r8 = (r << 3) | (r >> 2);
        const uint32_t g8 = (g << 2) | (g >> 4);
        const uint32_t b8 = (b << 3) | (b >> 2);
        dst[i] = (r8 << 16) | (g8 << 8) | b8;
    }
}

Presenter::~Presenter() { close(); }

#ifdef _WIN32

static Presenter* g_presenter = nullptr;
static InputState* g_input = nullptr;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            if (g_input) {
                g_input->quit = true;
            }
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
        case WM_KEYUP: {
            if (!g_input) {
                break;
            }
            const bool down = (msg == WM_KEYDOWN);
            const bool prev_f = false;
            (void)prev_f;
            switch (wp) {
                case 'W':
                    if (down && !g_input->up) g_input->edge_up = true;
                    g_input->up = down;
                    break;
                case 'S':
                    if (down && !g_input->down) g_input->edge_down = true;
                    g_input->down = down;
                    break;
                case 'A':
                    if (down && !g_input->left) g_input->edge_left = true;
                    g_input->left = down;
                    break;
                case 'D':
                    if (down && !g_input->right) g_input->edge_right = true;
                    g_input->right = down;
                    break;
                case VK_ESCAPE:
                    if (down && !g_input->quit) g_input->edge_quit = true;
                    g_input->quit = down || g_input->quit;
                    break;
                case VK_F1:
                    if (down) g_input->edge_f1 = true;
                    g_input->f1 = down;
                    break;
                case VK_F2:
                    if (down) g_input->edge_f2 = true;
                    g_input->f2 = down;
                    break;
                case VK_F3:
                    if (down) g_input->edge_f3 = true;
                    g_input->f3 = down;
                    break;
                case VK_F4:
                    if (down) g_input->edge_f4 = true;
                    g_input->f4 = down;
                    break;
                case VK_F5:
                    if (down) g_input->edge_f5 = true;
                    g_input->f5 = down;
                    break;
                case VK_F6:
                    if (down) g_input->edge_f6 = true;
                    g_input->f6 = down;
                    break;
                case VK_F7:
                    if (down) g_input->edge_f7 = true;
                    g_input->f7 = down;
                    break;
                case VK_F8:
                    if (down) g_input->edge_f8 = true;
                    g_input->f8 = down;
                    break;
                case VK_F9:
                    if (down) g_input->edge_f9 = true;
                    g_input->f9 = down;
                    break;
                case VK_F10:
                    if (down) g_input->edge_f10 = true;
                    g_input->f10 = down;
                    break;
                case 'P':
                    if (down && !g_input->pause) g_input->edge_pause = true;
                    g_input->pause = down;
                    break;
                case 'R':
                    if (down && !g_input->reset) g_input->edge_reset = true;
                    g_input->reset = down;
                    break;
                case '1':
                case VK_NUMPAD1:
                    if (down && !(lp & (1LL << 30))) g_input->edge_1 = true;
                    break;
                case '2':
                case VK_NUMPAD2:
                    if (down && !(lp & (1LL << 30))) g_input->edge_2 = true;
                    break;
                case '3':
                case VK_NUMPAD3:
                    if (down && !(lp & (1LL << 30))) g_input->edge_3 = true;
                    break;
                case '4':
                    if (down) g_input->edge_4 = true;
                    break;
                case '5':
                    if (down) g_input->edge_5 = true;
                    break;
                default:
                    break;
            }
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

bool Presenter::open(u32 fb_w, u32 fb_h, u32 window_w, u32 window_h, bool headless,
                     const std::string& title) {
    fb_w_ = fb_w;
    fb_h_ = fb_h;
    win_w_ = window_w ? window_w : fb_w;
    win_h_ = window_h ? window_h : fb_h;
    headless_ = headless;
    open_ = true;
    if (headless_) {
        return true;
    }
    g_presenter = this;
    static bool class_reg = false;
    HINSTANCE hi = GetModuleHandleA(nullptr);
    if (!class_reg) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hi;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = "Gpu2dDemoWindow";
        RegisterClassA(&wc);
        class_reg = true;
    }
    HWND hwnd = CreateWindowExA(0, "Gpu2dDemoWindow", title.c_str(),
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                                static_cast<int>(win_w_), static_cast<int>(win_h_), nullptr,
                                nullptr, hi, nullptr);
    if (!hwnd) {
        return false;
    }
    hwnd_ = hwnd;
    HDC hdc = GetDC(hwnd);
    hdc_ = hdc;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(win_w_);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(win_h_);  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmp) {
        return false;
    }
    hbmp_ = hbmp;
    bits_ = bits;
    return true;
}

void Presenter::close() {
    if (!headless_ && hwnd_) {
        if (hbmp_) {
            DeleteObject(static_cast<HBITMAP>(hbmp_));
            hbmp_ = nullptr;
        }
        if (hdc_ && hwnd_) {
            ReleaseDC(static_cast<HWND>(hwnd_), static_cast<HDC>(hdc_));
            hdc_ = nullptr;
        }
        DestroyWindow(static_cast<HWND>(hwnd_));
        hwnd_ = nullptr;
    }
    open_ = false;
}

bool Presenter::pump(InputState& in) {
    if (headless_) {
        return !in.quit;
    }
    g_input = &in;
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            in.quit = true;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    g_input = nullptr;
    return !in.quit;
}

void Presenter::present(const uint8_t* fb, u32 w, u32 h, u32 stride, bool rgb565) {
    if (headless_ || !open_ || !fb || !hwnd_) {
        return;
    }
    // Convert to 32-bit then stretch into window DIB.
    converted_.resize(static_cast<size_t>(w) * h);
    if (rgb565) {
        for (u32 y = 0; y < h; ++y) {
            rgb565_to_xrgb8888(fb + static_cast<size_t>(y) * stride,
                               converted_.data() + static_cast<size_t>(y) * w, w);
        }
    } else {
        for (u32 y = 0; y < h; ++y) {
            std::memcpy(converted_.data() + static_cast<size_t>(y) * w,
                        fb + static_cast<size_t>(y) * stride, w * 4);
        }
    }
    // Single stretch path: internal FB → window. No intermediate BitBlt
    // (BitBlt of a window-sized DIB only partially filled by SetDIBitsToDevice
    //  flashed an unscaled ghost in the top-left).
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(h);  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    HDC win = GetDC(static_cast<HWND>(hwnd_));
    if (!win) {
        return;
    }
    SetStretchBltMode(win, HALFTONE);
    StretchDIBits(win, 0, 0, static_cast<int>(win_w_), static_cast<int>(win_h_), 0, 0,
                  static_cast<int>(w), static_cast<int>(h), converted_.data(), &bmi,
                  DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(static_cast<HWND>(hwnd_), win);
}

#else  // !_WIN32

bool Presenter::open(u32 fb_w, u32 fb_h, u32 window_w, u32 window_h, bool headless,
                     const std::string&) {
    fb_w_ = fb_w;
    fb_h_ = fb_h;
    win_w_ = window_w;
    win_h_ = window_h;
    headless_ = true;  // non-Windows: force headless for now
    open_ = true;
    (void)headless;
    return true;
}

void Presenter::close() { open_ = false; }

bool Presenter::pump(InputState& in) { return !in.quit; }

void Presenter::present(const uint8_t*, u32, u32, u32, bool) {}

#endif

}  // namespace host
