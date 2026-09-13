#include "presenter.hpp"

#include <cstdio>
#include <vector>

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
    // A-T1: RGB565 conversion known colors
    {
        // black, white, red, green, blue, mid gray
        const uint16_t src[6] = {0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0x8410};
        uint32_t dst[6] = {0};
        host::rgb565_to_xrgb8888(reinterpret_cast<const uint8_t*>(src), dst, 6);
        CHECK(dst[0] == 0x000000u);
        CHECK((dst[1] & 0xFFFFFF) == 0xFFFFFFu);
        CHECK(((dst[2] >> 16) & 0xFF) == 0xFF && ((dst[2] >> 8) & 0xFF) == 0 &&
              (dst[2] & 0xFF) == 0);
        CHECK(((dst[3] >> 16) & 0xFF) == 0 && ((dst[3] >> 8) & 0xFF) > 200 &&
              (dst[3] & 0xFF) == 0);
        CHECK(((dst[4] >> 16) & 0xFF) == 0 && ((dst[4] >> 8) & 0xFF) == 0 &&
              (dst[4] & 0xFF) == 0xFF);
        // mid-tone has non-zero channels
        CHECK((dst[5] & 0xFFFFFF) != 0);
    }

    // A-T2/A-T3: headless 640x360 and 1280x720 without window
    {
        host::Presenter p;
        CHECK(p.open(640, 360, 1280, 720, true, "t"));
        CHECK(p.headless());
        host::InputState in;
        CHECK(p.pump(in));
        std::vector<uint8_t> fb(static_cast<size_t>(640) * 360 * 2, 0);
        p.present(fb.data(), 640, 360, 640 * 2, true);
        p.close();
    }
    {
        host::Presenter p;
        CHECK(p.open(1280, 720, 2560, 1440, true, "t"));
        std::vector<uint8_t> fb(static_cast<size_t>(1280) * 720 * 2, 0);
        p.present(fb.data(), 1280, 720, 1280 * 2, true);
        p.close();
    }

    if (g_fail) {
        std::printf("gpu2d_test_presenter FAIL %d\n", g_fail);
        return 1;
    }
    std::printf("gpu2d_test_presenter PASS\n");
    return 0;
}
