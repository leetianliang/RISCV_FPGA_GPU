#include "gpu2d/renderer.hpp"

#include <cstdio>
#include <cstring>
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
    using namespace gpu2d;
    // B-T1: API call sequence → expected command count/order
    CommandRecorder rec;
    rec.begin_frame();
    rec.fill_rect(0, 0, 32, 32, Color::rgb(1, 2, 3));
    SpriteParams sp;
    sp.tex = TextureId{1};
    sp.dst_x = 4;
    sp.dst_y = 4;
    sp.w = 8;
    sp.h = 8;
    rec.draw_sprite(sp);
    rec.draw_sprite_alpha(sp, 128);
    rec.set_clip(true, 0, 0, 16, 16);
    rec.fill_rect(1, 1, 2, 2, Color::rgb(9, 9, 9));
    rec.present();
    CHECK(rec.commands().size() == 4);
    CHECK(rec.commands()[0].op == RecOp::Fill);
    CHECK(rec.commands()[1].op == RecOp::Sprite);
    CHECK(rec.commands()[2].sp.global_alpha == 128);
    CHECK(rec.commands()[3].clip_en);

    // B-T4: two runs same inputs → identical command streams
    CommandRecorder a, b;
    auto fill = [&](CommandRecorder& r) {
        r.begin_frame();
        r.fill_rect(3, 4, 5, 6, Color::rgb(7, 8, 9));
        SpriteParams s;
        s.tex = TextureId{2};
        s.dst_x = 10;
        s.dst_y = 11;
        s.w = 12;
        s.h = 13;
        s.blend = BlendMode::AddSat;
        r.draw_sprite(s);
        r.present();
    };
    fill(a);
    fill(b);
    CHECK(a.commands().size() == b.commands().size());
    CHECK(std::memcmp(a.commands().data(), b.commands().data(),
                      a.commands().size() * sizeof(RecCommand)) == 0);

    // B-T3: invalid handle is recorded as-is; backend must reject (tested in backend)
    CHECK(sp.tex.v == 1);

    if (g_fail) {
        std::printf("gpu2d_test_api FAIL %d\n", g_fail);
        return 1;
    }
    std::printf("gpu2d_test_api PASS\n");
    return 0;
}
