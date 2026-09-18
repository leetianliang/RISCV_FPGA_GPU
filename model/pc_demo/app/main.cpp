#include "gpu2d/renderer.hpp"
#include "gpu2d/types.hpp"
#include "facility/app.hpp"
#include "facility_showcase.hpp"
#include "facility_app2_fixtures.hpp"
#include "facility_input.hpp"
#include "golden_renderer.hpp"
#include "neon/assets.hpp"
#include "neon/sim.hpp"
#include "presenter.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

using gpu2d::BackendKind;
using gpu2d::Color;
using gpu2d::PixelFormat;
using gpu2d::ProfileDesc;
using gpu2d::i32;
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
    bool facility_route = false;
    bool facility_showcase = false;
    u32 facility_start_seconds=0;
    std::string facility_capture_scene;
    std::string app = "neon";  // neon | facility
};

const char* usage() {
    return R"(gpu2d_demo — PC Golden Interactive Application
Usage:
  gpu2d_demo [--app neon|facility] [--headless] [--frames N] [--seed N]
             [--backend immediate|tile|tile16|tile64]
             [--profile interactive|showcase]
             [--scene game|sprite|alpha|bullet|scale|overdraw]
             [--capture <path.raw>] [--xray] [--facility-route] [--facility-showcase] [--help]

  --facility-showcase: headless facility capture of an explicitly staged snapshot.
  --facility-start-seconds N: headless verification/demo game-time offset.
  --facility-capture-scene mid|fx|elite|level|density|technical|showcase: authored fixture.

Controls:
  WASD move | F1-F5 stress (neon) | F6 Immediate | F7 Tile32
  F8 tech HUD | F9 normal | F10 X-Ray | P pause | R reset | ESC menu/quit
  Launcher: 1 NEON SURVIVOR  2 FACILITY-Ω  3 help  4 stress
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
        } else if (a == "--facility-route") {
            cli.facility_route = true;
        } else if (a == "--facility-showcase") {
            cli.facility_showcase = true;
        } else if (a == "--facility-start-seconds") {
            const std::string value=need("--facility-start-seconds");
            if(value.empty() || value.size()>5 || value.find_first_not_of("0123456789")!=std::string::npos)cli.valid=false;
            else cli.facility_start_seconds=static_cast<u32>(std::stoul(value));
        } else if (a == "--facility-capture-scene") {
            cli.facility_capture_scene=need("--facility-capture-scene");
            if(!facility_capture::valid_scene(cli.facility_capture_scene))cli.valid=false;
        } else if (a == "--app") {
            cli.app = need("--app");
            if (cli.app != "neon" && cli.app != "facility") {
                std::fprintf(stderr, "unknown app %s\n", cli.app.c_str());
                cli.valid = false;
            }
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
    if (cli.facility_showcase && (!cli.headless || cli.app!="facility" || cli.facility_route)) {
        std::fprintf(stderr,"--facility-showcase requires --headless --app facility and no route\n");
        cli.valid=false;
    }
    if((cli.facility_start_seconds || !cli.facility_capture_scene.empty()) &&
       (!cli.headless || cli.app!="facility" || cli.facility_showcase || cli.facility_route))cli.valid=false;
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
            // PWA-04: bit-replication match Win32 presenter RGB565 expansion.
            const u32 r5 = (p >> 11) & 0x1F;
            const u32 g6 = (p >> 5) & 0x3F;
            const u32 b5 = p & 0x1F;
            const u8 r = static_cast<u8>((r5 << 3) | (r5 >> 2));
            const u8 g = static_cast<u8>((g6 << 2) | (g6 >> 4));
            const u8 b = static_cast<u8>((b5 << 3) | (b5 >> 2));
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

    // FACILITY-Ω runtime assets (processed offline; no PNG at runtime).
    facility::TexBank fo_tex;
    facility::AppState fo;
    auto resolve_fo_rt = []() -> std::string {
        const char* cands[] = {
            "assets/facility_omega/runtime",
            "../assets/facility_omega/runtime",
            "../../assets/facility_omega/runtime",
            "../../../assets/facility_omega/runtime",
            "../../../../assets/facility_omega/runtime",
        };
        for (const char* c : cands) {
            std::ifstream probe(std::string(c) + "/facility_omega_assets.json");
            if (probe.good()) {
                return c;
            }
        }
        return "assets/facility_omega/runtime";
    };
    const std::string fo_rt = resolve_fo_rt();
    if (cli.app == "facility" || !cli.headless) {
        std::vector<facility::SpriteBlob> blobs_fo;
        if (facility::load_runtime_sprites(fo_rt, blobs_fo)) {
            std::vector<facility::SpriteBlob> atlases;
            if (!facility::load_runtime_atlases(fo_rt, blobs_fo, atlases)) {
                std::fprintf(stderr, "failed to load FACILITY runtime atlases\n");
                return 1;
            }
            for (const auto& a : atlases) {
                gpu2d::TextureDesc d;
                d.width=a.width; d.height=a.height; d.stride=a.stride;
                d.format=a.rgb565 ? gpu2d::PixelFormat::RGB565 : gpu2d::PixelFormat::ARGB8888;
                d.pixels=a.pixels.data();
                auto id=gpu.create_texture(d);
                if (!id.valid()) {
                    std::fprintf(stderr,"FACILITY atlas upload failed: %s\n",a.name.c_str());
                    return 1;
                }
                for (const auto& b : blobs_fo) {
                    if (b.atlas_file != a.name) continue;
                    fo_tex.put(b.name,{id,b.width,b.height,b.anchor_x,b.anchor_y,b.atlas_x,b.atlas_y});
                }
            }
            facility::sim_reset(fo, cli.seed);
            fo.frame=static_cast<u64>(cli.facility_start_seconds)*facility::kTicksPerSecond;
            facility::set_viewport(fo, cli.profile.width, cli.profile.height);
        } else if (cli.app == "facility") {
            std::fprintf(stderr, "failed to load facility assets from %s\n", fo_rt.c_str());
            return 1;
        } else {
            std::fprintf(stderr,
                         "[warn] FACILITY-O assets not found under %s — menu [2] disabled. "
                         "Run from repo root or build/stage0045.\n",
                         fo_rt.c_str());
        }
    }
    bool use_facility = (cli.app == "facility");
    if (use_facility && !fo.ready) {
        std::fprintf(stderr, "facility assets not ready\n");
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
    bool tech_hud = !use_facility;
    bool xray = cli.xray;
    u32 frame_limit = cli.frames;
    u64 frames_run = 0;
    double host_fps = 0.0;
    auto t0 = std::chrono::steady_clock::now();
    gpu2d::TelemetrySnapshot base_tel;

    // I-02: GPU-rendered launcher (interactive only; headless skips to scene).
    enum class AppScreen : int { Menu = 0, Game = 1, Help = 2, Stress = 3 };
    AppScreen screen = cli.headless ? AppScreen::Game : AppScreen::Menu;
    // CLI --scene already selects content; interactive default starts at menu.

    auto draw_launcher = [&](gpu2d::GraphicsApi& api, AppScreen sc) {
        api.fill_rect(0, 0, cli.profile.width, cli.profile.height,
                      Color::rgb(6, 8, 20));
        for (u32 x = 20; x + 20 < cli.profile.width; x += 40) {
            api.fill_rect(static_cast<i32>(x), 40, 2, cli.profile.height - 80,
                          Color::rgb(20, 40, 70));
        }
        neon::draw_text(api, assets, 24, 28, "RISC-V + FPGA 2D GPU DEMO", Color::rgb(80, 220, 255));
        if (sc == AppScreen::Menu) {
            neon::draw_text(api, assets, 40, 80, "1  NEON SURVIVOR", Color::rgb(180, 255, 180));
            if (fo.ready) {
                neon::draw_text(api, assets, 40, 100, "2  FACILITY-O",
                                Color::rgb(255, 200, 120));
            } else {
                neon::draw_text(api, assets, 40, 100, "2  FACILITY-O (NO ASSETS)",
                                Color::rgb(120, 80, 80));
            }
            neon::draw_text(api, assets, 40, 120, "3  ARCHITECTURE X-RAY / HELP",
                            Color::rgb(180, 255, 180));
            neon::draw_text(api, assets, 40, 140, "4  BENCHMARK / STRESS",
                            Color::rgb(180, 255, 180));
            neon::draw_text(api, assets, 40, 180, "ESC QUIT", Color::rgb(160, 160, 160));
            neon::draw_text(api, assets, 40, 200, "GPU: TILE-BASED 2D / CPU: RISC-V (PC GOLDEN)",
                            Color::rgb(100, 140, 180));
        } else if (sc == AppScreen::Help) {
            neon::draw_text(api, assets, 40, 70, "ARCHITECTURE X-RAY", Color::rgb(0, 255, 255));
            neon::draw_text(api, assets, 40, 95, "F6 IMMEDIATE  F7 TILE32", Color::rgb(200, 200, 200));
            neon::draw_text(api, assets, 40, 110, "F8 TECH HUD   F10 X-RAY", Color::rgb(200, 200, 200));
            neon::draw_text(api, assets, 40, 125, "F1-F5 STRESS SCENES", Color::rgb(200, 200, 200));
            neon::draw_text(api, assets, 40, 140, "WASD MOVE  P PAUSE  R RESET", Color::rgb(200, 200, 200));
            neon::draw_text(api, assets, 40, 165, "TILE GRID SHOWS WORKREF / OVERDRAW WHEN TILE MODE",
                            Color::rgb(160, 200, 255));
            neon::draw_text(api, assets, 40, 200, "1 OR 4 BACK / ENTER GAME  ESC MENU",
                            Color::rgb(160, 160, 160));
        } else if (sc == AppScreen::Stress) {
            neon::draw_text(api, assets, 40, 70, "BENCHMARK / STRESS (PC GOLDEN)", Color::rgb(255, 200, 80));
            neon::draw_text(api, assets, 40, 95, "F1 SPRITE STORM", Color::rgb(200, 200, 200));
            neon::draw_text(api, assets, 40, 110, "F2 ALPHA STORM", Color::rgb(200, 200, 200));
            neon::draw_text(api, assets, 40, 125, "F3 BULLET HELL", Color::rgb(200, 200, 200));
            neon::draw_text(api, assets, 40, 140, "F4 SCALE STORM", Color::rgb(200, 200, 200));
            neon::draw_text(api, assets, 40, 155, "F5 OVERDRAW STORM", Color::rgb(200, 200, 200));
            neon::draw_text(api, assets, 40, 185, "1 PLAY  0/ESC MENU", Color::rgb(160, 160, 160));
            neon::draw_text(api, assets, 40, 205, "NOT FPGA PERFORMANCE", Color::rgb(255, 100, 100));
        }
    };

    while (true) {
        int upgrade_choice = -1;
        if (!cli.headless) {
            if (!pres.pump(input)) {
                break;
            }
            if (screen == AppScreen::Menu) {
                if (input.edge_1) {
                    use_facility = false;
                    screen = AppScreen::Game;
                    neon::sim_reset(sim, scfg, cli.seed);
                    rng.seed(cli.seed);
                    sim.scene = neon::SceneId::Game;
                    cli.scene = neon::SceneId::Game;
                } else if (input.edge_2) {
                    if (fo.ready) {
                        use_facility = true;
                        tech_hud = false;
                        screen = AppScreen::Game;
                        facility::set_viewport(fo, cli.profile.width, cli.profile.height);
                    } else {
                        std::fprintf(stderr,
                                     "FACILITY-O not available (assets not loaded).\n"
                                     "Launch from repository root:\n"
                                     "  .\\build\\stage0045\\model\\pc_demo\\gpu2d_demo.exe\n"
                                     "or:  --app facility\n");
                    }
                } else if (input.edge_3) {
                    screen = AppScreen::Help;
                    xray = true;
                } else if (input.edge_4) {
                    screen = AppScreen::Stress;
                }
                input.clear_edges();
                rec.begin_frame();
                draw_launcher(rec, screen);
                rec.present();
                if (!gpu.execute_frame(rec.commands())) {
                    std::fprintf(stderr, "launcher fault=0x%X\n", gpu.last_fault());
                    return 1;
                }
                const u8* fbm = gpu.framebuffer();
                pres.present(fbm, gpu.fb_width(), gpu.fb_height(), gpu.fb_stride(),
                             gpu.fb_format() == PixelFormat::RGB565);
                continue;
            }
            if (screen == AppScreen::Help) {
                if (input.edge_1 || input.edge_4) {
                    screen = AppScreen::Game;
                    neon::sim_reset(sim, scfg, cli.seed);
                    rng.seed(cli.seed);
                    sim.scene = neon::SceneId::Game;
                } else if (input.edge_quit || input.edge_3) {
                    screen = AppScreen::Menu;
                    xray = cli.xray;
                }
                input.clear_edges();
                rec.begin_frame();
                draw_launcher(rec, screen);
                rec.present();
                if (!gpu.execute_frame(rec.commands())) {
                    std::fprintf(stderr, "help fault=0x%X\n", gpu.last_fault());
                    return 1;
                }
                const u8* fbh = gpu.framebuffer();
                pres.present(fbh, gpu.fb_width(), gpu.fb_height(), gpu.fb_stride(),
                             gpu.fb_format() == PixelFormat::RGB565);
                continue;
            }
            if (screen == AppScreen::Stress) {
                if (input.edge_1 || input.edge_5) {
                    screen = AppScreen::Game;
                } else if (input.edge_quit || input.edge_4) {
                    screen = AppScreen::Menu;
                }
                // F1-F5 still switch stress while showing menu then jump in
                if (input.edge_f1 || input.edge_f2 || input.edge_f3 || input.edge_f4 ||
                    input.edge_f5) {
                    screen = AppScreen::Game;
                }
                if (screen == AppScreen::Stress) {
                    input.clear_edges();
                    rec.begin_frame();
                    draw_launcher(rec, screen);
                    rec.present();
                    if (!gpu.execute_frame(rec.commands())) {
                        std::fprintf(stderr, "stress menu fault=0x%X\n", gpu.last_fault());
                        return 1;
                    }
                    const u8* fbs = gpu.framebuffer();
                    pres.present(fbs, gpu.fb_width(), gpu.fb_height(), gpu.fb_stride(),
                                 gpu.fb_format() == PixelFormat::RGB565);
                    continue;
                }
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
            if (use_facility) facility_input::controls(fo,input,cli.seed,cli.profile.width,cli.profile.height);
            if (!use_facility && input.edge_pause) {
                sim.paused = !sim.paused;
            }
            if (!use_facility && input.edge_reset) {
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
            upgrade_choice = facility_input::finish_input_frame(input, use_facility && fo.level_up_pending);
        }

        bool keys[4] = {input.up, input.down, input.left, input.right};
        // Deterministic review route: 768 px right, then 768 px down, then idle.
        if (cli.headless && use_facility && cli.facility_route) {
            keys[0] = false; keys[2] = false;
            keys[3] = frames_run < 256;
            keys[1] = frames_run >= 256 && frames_run < 512;
        }
        rec.begin_frame();
        if (use_facility) {
            facility::set_viewport(fo, cli.profile.width, cli.profile.height);
            // Level-up: 1/2/3 choose upgrade; no movement keys while pending.
            if (fo.level_up_pending) {
                if (cli.headless) {
                    facility::apply_upgrade(fo, 0);  // deterministic auto-pick for captures
                } else if (upgrade_choice >= 0) {
                    facility::apply_upgrade(fo, static_cast<u32>(upgrade_choice));
                }
                input.clear_edges();
                for (int i = 0; i < 4; ++i) keys[i] = false;
            }
            if (!cli.facility_capture_scene.empty()) facility_capture::app2_fixture(fo,cli.facility_capture_scene);
            else if (cli.facility_showcase) facility_capture::stage(fo);
            else facility::sim_step(fo, keys);
            facility::camera_follow(fo, cli.profile.width, cli.profile.height);
            facility::render_scene(rec, fo, fo_tex, cli.profile.width, cli.profile.height);
            char hud[128];
            if(fo.paused)neon::draw_text_pal(rec,assets,280,62,"PAUSED / P",Color::rgb(245,200,80));
            for(u32 i=0;i<4;++i)if(fo.gameplay.weapons[i].level) {
                std::snprintf(hud,sizeof(hud),"L%u",fo.gameplay.weapons[i].level);
                neon::draw_text_pal(rec,assets,320+i*72,39,hud,Color::rgb(160,211,220));
            }
            if(!cli.facility_capture_scene.empty())neon::draw_text_pal(rec,assets,480,cli.profile.height-14,"STAGED APP2_002",Color::rgb(120,160,174));
            if (cli.facility_showcase)
                neon::draw_text_pal(rec, assets, 20, cli.profile.height-14, "STAGED SHOWCASE / 20 ENEMIES", Color::rgb(115,152,164));
            neon::draw_text_pal(rec, assets, 20, 7, "FACILITY-O", Color::rgb(211, 229, 234));
            neon::draw_text_pal(rec, assets, 20, 19, "POWER TEST / A-3", Color::rgb(85, 146, 170));
            neon::draw_text_pal(rec, assets, 134, 13, "HP", Color::rgb(197, 220, 225));
            std::snprintf(hud, sizeof(hud), "LV%u XP %u/%u", fo.player.level, fo.player.xp,
                          fo.player.xp_need);
            neon::draw_text_pal(rec, assets, 20, 36, hud, Color::rgb(120, 220, 170));
            std::snprintf(hud, sizeof(hud), "%02u:%02u", static_cast<u32>(fo.frame / 3600),
                          static_cast<u32>((fo.frame / 60) % 60));
            neon::draw_text_pal(rec, assets, 294, 13, hud, Color::rgb(188, 230, 244));
            std::snprintf(hud, sizeof(hud), "KILLS %u", fo.player.kills);
            neon::draw_text_pal(rec, assets, 378, 13, hud, Color::rgb(218, 231, 237));
            neon::draw_text_pal(rec, assets, cli.profile.width - 113, 13, "PULSE SHOT", Color::rgb(92, 199, 236));
            if (fo.level_up_pending) {
                const i32 cx = static_cast<i32>(cli.profile.width) / 2;
                const i32 cy = static_cast<i32>(cli.profile.height) / 2;
                neon::draw_text_pal(rec, assets, cx - 40, cy - 48, "LEVEL UP",
                                    Color::rgb(245, 200, 80));
                for(u32 i=0;i<3;++i) {
                    const auto choice=fo.gameplay.choices[i];const i32 x=cx-136+static_cast<i32>(i)*96;
                    const char* label=choice.kind==3?"ENERGY FLD":facility::choice_name(choice);
                    neon::draw_text_pal(rec,assets,x,cy+1,label,Color::rgb(200,230,240));
                    if(choice.kind<4)std::snprintf(hud,sizeof(hud),"[%u] %s L%u",i+1,fo.gameplay.weapons[choice.kind].level?"UP":"NEW",fo.gameplay.weapons[choice.kind].level+1);
                    else std::snprintf(hud,sizeof(hud),"[%u] BOOST",i+1);
                    neon::draw_text_pal(rec,assets,x+23,cy-14,hud,Color::rgb(220,197,130));
                }
            }
            if (tech_hud || cli.facility_capture_scene=="technical") {
                std::snprintf(hud, sizeof(hud), "CAM %d,%d EN %u BL %u", fo.cam.x, fo.cam.y,
                              facility::live_enemy_count(fo), facility::live_bullet_count(fo));
                neon::draw_text_pal(rec, assets, 5, cli.profile.height - 12, hud, Color::rgb(147, 190, 199));
                u32 alpha=0,add=0,scale=0,bilinear=0,pickups=0;
                for(const auto& g:fo.xp_gems)pickups+=g.alive;
                for(const auto& c:rec.commands())if(c.op==gpu2d::RecOp::Sprite){alpha+=c.sp.blend==gpu2d::BlendMode::StraightAlpha;add+=c.sp.blend==gpu2d::BlendMode::AddSat;scale+=c.sp.scale_w!=0;bilinear+=c.sp.filter==gpu2d::FilterMode::Bilinear;}
                std::snprintf(hud,sizeof(hud),"CMD %u SP %u A %u ADD %u SC %u BI %u PK %u",static_cast<u32>(rec.commands().size()),rec.sprite_count(),alpha,add,scale,bilinear,pickups);
                neon::draw_text_pal(rec,assets,5,cli.profile.height-36,hud,Color::rgb(147,190,199));
                const auto& telemetry=gpu.telemetry();
                std::snprintf(hud,sizeof(hud),"PREV TILE %u WR %u OD %u",telemetry.tiles_active,telemetry.workref_count,telemetry.max_overdraw);
                neon::draw_text_pal(rec,assets,5,cli.profile.height-24,hud,Color::rgb(147,190,199));
            }
            rec.present();
            if (!gpu.execute_frame(rec.commands())) {
                std::fprintf(stderr, "facility execute fault=0x%X\n", gpu.last_fault());
                return 1;
            }
            base_tel = gpu.telemetry();
        } else {
        neon::sim_step(sim, scfg, rng, keys, cli.headless);

        // R2-01: measure BASE scene only; overlay never feeds telemetry it draws.
        neon::render_scene_base(rec, assets, sim, scfg);
        rec.present();
        if (!gpu.execute_frame(rec.commands())) {
            std::fprintf(stderr, "execute_frame(base) failed fault=0x%X\n", gpu.last_fault());
            return 1;
        }
        base_tel = gpu.telemetry();

        const bool want_overlay = true;
        if (want_overlay) {
            neon::DrawOpts opts;
            opts.hud = true;
            opts.tech_hud = tech_hud || cli.headless;
            opts.xray = xray;
            opts.tile_mode = gpu.backend() != BackendKind::Immediate;
            opts.tile_size = cli.profile.tile_size ? cli.profile.tile_size : 32;
            opts.host_fps = cli.headless ? 0.0 : host_fps;
            const auto base_view = base_tel.view();
            opts.tel = &base_view;
            rec.begin_frame();
            neon::render_debug_overlay(rec, assets, sim, scfg, opts);
            rec.present();
            if (!gpu.execute_frame(rec.commands())) {
                std::fprintf(stderr, "execute_frame(overlay) failed fault=0x%X\n",
                             gpu.last_fault());
                return 1;
            }
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

    // Report authoritative BASE telemetry (not overlay-contaminated).
    const auto& tel = base_tel;
    std::printf(
        "gpu2d_demo done backend=%s scene=%u base_cmds=%u base_sprites=%u tiles=%u/%u wrefs=%u "
        "max_od=%u\n",
        gpu.backend() == BackendKind::Immediate ? "immediate" : "tile",
        static_cast<u32>(sim.scene), tel.command_count, tel.sprite_count, tel.tiles_active,
        tel.tiles_total, tel.workref_count, tel.max_overdraw);
    return 0;
}
