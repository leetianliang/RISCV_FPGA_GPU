#include "gpu2d/renderer.hpp"
#include "gpu2d/types.hpp"
#include "golden_renderer.hpp"
#include "neon/assets.hpp"
#include "neon/sim.hpp"
#include "presenter.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using gpu2d::BackendKind;
using gpu2d::Color;
using gpu2d::ProfileDesc;
using gpu2d::u16;
using gpu2d::u32;
using gpu2d::u64;
using gpu2d::u8;

struct Cli {
    bool headless = false;
    bool valid = true;
    u32 frames = 0;
    u32 seed = 1234;
    BackendKind backend = BackendKind::Immediate;
    ProfileDesc profile{};
    neon::SceneId scene = neon::SceneId::Game;
    std::string capture;
    bool show_help = false;
    bool xray = false;
};

const char* usage() {
    return R"(gpu2d_demo — PC Golden Interactive Application (Stage 004.5)
Usage:
  gpu2d_demo [--headless] [--frames N] [--seed N]
             [--backend immediate|tile|tile16|tile64]
             [--profile interactive|showcase]
             [--scene game|sprite|alpha|bullet|scale|overdraw]
             [--capture <path.raw>]
             [--xray]
             [--help]

Controls (interactive):
  WASD move | F1-F5 stress | F6 Immediate | F7 Tile32
  F8 tech HUD | F9 normal | F10 X-Ray | P pause | R reset | ESC quit
)";
}

bool parse_args(int argc, char** argv, Cli& cli) {
    cli.profile.width = 640;
    cli.profile.height = 360;
    cli.profile.format = gpu2d::PixelFormat::RGB565;
    cli.profile.tile_size = 32;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* n) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", n);
                cli.valid = false;
                return "";
            }
            return argv[++i];
        };
        if (a == "--headless") {
            cli.headless = true;
        } else if (a == "--frames") {
            cli.frames = static_cast<u32>(std::atoi(need("--frames")));
        } else if (a == "--seed") {
            cli.seed = static_cast<u32>(std::atoi(need("--seed")));
        } else if (a == "--backend") {
            const std::string b = need("--backend");
            if (b == "immediate") {
                cli.backend = BackendKind::Immediate;
            } else if (b == "tile" || b == "tile32") {
                cli.backend = BackendKind::Tile32;
            } else if (b == "tile16") {
                cli.backend = BackendKind::Tile16;
            } else if (b == "tile64") {
                cli.backend = BackendKind::Tile64;
            } else {
                std::fprintf(stderr, "unknown backend %s\n", b.c_str());
                cli.valid = false;
            }
        } else if (a == "--profile") {
            const std::string p = need("--profile");
            if (p == "interactive") {
                cli.profile.width = 640;
                cli.profile.height = 360;
            } else if (p == "showcase") {
                cli.profile.width = 1280;
                cli.profile.height = 720;
            } else {
                std::fprintf(stderr, "unknown profile %s\n", p.c_str());
                cli.valid = false;
            }
        } else if (a == "--scene") {
            const std::string s = need("--scene");
            if (s == "game") {
                cli.scene = neon::SceneId::Game;
            } else if (s == "sprite") {
                cli.scene = neon::SceneId::SpriteStorm;
            } else if (s == "alpha") {
                cli.scene = neon::SceneId::AlphaStorm;
            } else if (s == "bullet") {
                cli.scene = neon::SceneId::BulletHell;
            } else if (s == "scale") {
                cli.scene = neon::SceneId::ScaleStorm;
            } else if (s == "overdraw") {
                cli.scene = neon::SceneId::OverdrawStorm;
            } else {
                std::fprintf(stderr, "unknown scene %s\n", s.c_str());
                cli.valid = false;
            }
        } else if (a == "--capture") {
            cli.capture = need("--capture");
        } else if (a == "--xray") {
            cli.xray = true;
        } else if (a == "--help" || a == "-h") {
            cli.show_help = true;
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", a.c_str());
            cli.valid = false;
        }
    }
    if (cli.headless && cli.frames == 0) {
        cli.frames = 60;  // default headless length
    }
    return cli.valid;
}

bool write_raw(const std::string& path, const u8* fb, u32 stride, u32 h) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    const size_t n = static_cast<size_t>(stride) * h;
    const size_t w = std::fwrite(fb, 1, n, f);
    std::fclose(f);
    return w == n;
}

bool write_ppm(const std::string& path, const u8* fb, u32 w, u32 h, u32 stride) {
    // path may end in .raw — also write sibling .ppm for visual check
    std::string ppm = path;
    const auto dot = ppm.find_last_of('.');
    if (dot != std::string::npos) {
        ppm = ppm.substr(0, dot);
    }
    ppm += ".ppm";
    FILE* f = std::fopen(ppm.c_str(), "wb");
    if (!f) {
        return false;
    }
    std::fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (u32 y = 0; y < h; ++y) {
        const u8* row = fb + static_cast<size_t>(y) * stride;
        for (u32 x = 0; x < w; ++x) {
            const u16 p = static_cast<u16>(row[x * 2] | (row[x * 2 + 1] << 8));
            const u8 r = static_cast<u8>(((p >> 11) & 0x1F) << 3);
            const u8 g = static_cast<u8>(((p >> 5) & 0x3F) << 2);
            const u8 b = static_cast<u8>((p & 0x1F) << 3);
            std::fputc(r, f);
            std::fputc(g, f);
            std::fputc(b, f);
        }
    }
    std::fclose(f);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    Cli cli;
    if (!parse_args(argc, argv, cli)) {
        std::fputs(usage(), stderr);
        return 2;
    }
    if (cli.show_help) {
        std::fputs(usage(), stdout);
        return 0;
    }

    gpu2d::GoldenBackend gpu;
    if (!gpu.init(cli.profile)) {
        std::fprintf(stderr, "Golden backend init failed\n");
        return 1;
    }
    gpu.set_backend(cli.backend);

    auto blobs = neon::build_procedural_assets();
    neon::Assets assets;
    auto upload = [&](const neon::SpriteBlob& b) -> gpu2d::TextureId {
        gpu2d::TextureDesc d;
        d.width = b.w;
        d.height = b.h;
        d.stride = b.stride;
        d.format = b.indexed8 ? gpu2d::PixelFormat::INDEX8 : gpu2d::PixelFormat::RGB565;
        d.pixels = b.pixels.data();
        if (b.indexed8) {
            d.palette = b.palette.data();
            d.palette_entries = 256;
        }
        return gpu.create_texture(d);
    };
    assets.player = upload(blobs.player);
    assets.enemy_n = upload(blobs.enemy_n);
    assets.enemy_f = upload(blobs.enemy_f);
    assets.enemy_h = upload(blobs.enemy_h);
    assets.bullet = upload(blobs.bullet);
    assets.bullet_e = upload(blobs.bullet_e);
    assets.particle = upload(blobs.particle);
    assets.glow = upload(blobs.glow);
    assets.font = upload(blobs.font);
    assets.font_pal = upload(blobs.font_index8);
    if (!assets.player.valid() || !assets.font.valid()) {
        std::fprintf(stderr, "asset upload failed\n");
        return 1;
    }

    host::Presenter pres;
    const u32 win_w = cli.profile.width * 2;
    const u32 win_h = cli.profile.height * 2;
    if (!pres.open(cli.profile.width, cli.profile.height, win_w, win_h, cli.headless,
                   "gpu2d_demo — NEON SURVIVOR")) {
        std::fprintf(stderr, "presenter open failed\n");
        return 1;
    }

    neon::SimConfig scfg;
    scfg.width = cli.profile.width;
    scfg.height = cli.profile.height;
    scfg.seed = cli.seed;
    neon::SimState sim;
    neon::Rng rng(cli.seed);
    neon::sim_reset(sim, scfg, cli.seed);
    sim.scene = cli.scene;

    gpu2d::CommandRecorder rec;
    host::InputState input;
    bool tech_hud = true;
    bool xray = cli.xray;
    u32 frame_limit = cli.frames;
    u64 frames_run = 0;
    double host_fps = 0.0;
    auto t0 = std::chrono::steady_clock::now();

    while (true) {
        if (!cli.headless) {
            if (!pres.pump(input)) {
                break;
            }
            if (input.edge_f6) {
                gpu.set_backend(BackendKind::Immediate);
            }
            if (input.edge_f7) {
                gpu.set_backend(BackendKind::Tile32);
            }
            if (input.edge_f8) {
                tech_hud = !tech_hud;
            }
            if (input.edge_f9) {
                xray = false;
            }
            if (input.edge_f10) {
                xray = true;
            }
            if (input.edge_pause) {
                sim.paused = !sim.paused;
            }
            if (input.edge_reset) {
                neon::sim_reset(sim, scfg, cli.seed);
                rng.seed(cli.seed);
                sim.scene = cli.scene;
            }
            if (input.edge_f1) {
                neon::sim_reset(sim, scfg, cli.seed);
                rng.seed(cli.seed);
                sim.scene = neon::SceneId::SpriteStorm;
            }
            if (input.edge_f2) {
                neon::sim_reset(sim, scfg, cli.seed);
                rng.seed(cli.seed);
                sim.scene = neon::SceneId::AlphaStorm;
            }
            if (input.edge_f3) {
                neon::sim_reset(sim, scfg, cli.seed);
                rng.seed(cli.seed);
                sim.scene = neon::SceneId::BulletHell;
            }
            if (input.edge_f4) {
                neon::sim_reset(sim, scfg, cli.seed);
                rng.seed(cli.seed);
                sim.scene = neon::SceneId::ScaleStorm;
            }
            if (input.edge_f5) {
                neon::sim_reset(sim, scfg, cli.seed);
                rng.seed(cli.seed);
                sim.scene = neon::SceneId::OverdrawStorm;
            }
            input.clear_edges();
        }

        const bool keys[4] = {input.up, input.down, input.left, input.right};
        neon::sim_step(sim, scfg, rng, keys, cli.headless);

        rec.begin_frame();
        neon::DrawOpts opts;
        opts.hud = true;
        opts.tech_hud = tech_hud || cli.headless;
        opts.xray = xray;
        opts.tile_mode = gpu.backend() != BackendKind::Immediate;
        opts.host_fps = host_fps;
        neon::render_frame(rec, assets, sim, scfg, opts);
        rec.present();

        if (!gpu.execute_frame(rec.commands())) {
            std::fprintf(stderr, "execute_frame failed fault=0x%X\n", gpu.last_fault());
            return 1;
        }

        // second pass for tech HUD using real telemetry (Immediate has no tile map)
        if (opts.tech_hud || opts.xray) {
            rec.begin_frame();
            const auto tel_view = gpu.telemetry().view();
            opts.tel = &tel_view;
            neon::render_frame(rec, assets, sim, scfg, opts);
            rec.present();
            if (!gpu.execute_frame(rec.commands())) {
                std::fprintf(stderr, "execute_frame(hud) failed fault=0x%X\n", gpu.last_fault());
                return 1;
            }
        }

        const u8* fb = gpu.framebuffer();
        if (!fb) {
            std::fprintf(stderr, "framebuffer missing\n");
            return 1;
        }
        pres.present(fb, gpu.fb_width(), gpu.fb_height(), gpu.fb_stride(),
                     gpu.fb_format() == gpu2d::PixelFormat::RGB565);

        ++frames_run;
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - t0).count();
        if (dt > 0.25) {
            host_fps = frames_run / dt;
            frames_run = 0;
            t0 = now;
        }

        if (frame_limit) {
            static u64 executed = 0;
            ++executed;
            if (executed >= frame_limit) {
                if (!cli.capture.empty()) {
                    const u8* f = gpu.framebuffer();
                    write_raw(cli.capture, f, gpu.fb_stride(), gpu.fb_height());
                    write_ppm(cli.capture, f, gpu.fb_width(), gpu.fb_height(), gpu.fb_stride());
                    std::printf("captured %s (+ppm)\n", cli.capture.c_str());
                }
                break;
            }
        } else if (cli.headless) {
            break;
        }
    }

    const auto& tel = gpu.telemetry();
    std::printf(
        "gpu2d_demo done backend=%s scene=%u cmds=%u sprites=%u tiles=%u/%u wrefs=%u "
        "max_od=%u\n",
        gpu.backend() == BackendKind::Immediate ? "immediate" : "tile",
        static_cast<u32>(sim.scene), tel.command_count, tel.sprite_count, tel.tiles_active,
        tel.tiles_total, tel.workref_count, tel.max_overdraw);
    return 0;
}
