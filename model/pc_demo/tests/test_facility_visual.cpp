#include "facility/app.hpp"
#include "golden_renderer.hpp"
#include "gpu2d/renderer.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace gpu2d;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)

int main(int argc, char** argv) {
    GoldenBackend imm, tile;
    ProfileDesc profile;
    profile.width = 640; profile.height = 360;
    CHECK(imm.init(profile) && tile.init(profile));
    tile.set_backend(BackendKind::Tile32);
    // Independently specified BGRA bytes: opaque red/blue, transparent magenta,
    // and 128-alpha red. Expected RGB565 over pure green uses the frozen arithmetic.
    const u8 pixels[] = {0,0,255,255, 255,0,0,255, 255,0,255,0, 0,0,255,128};
    TextureDesc d;
    d.width=4; d.height=1; d.stride=16; d.format=PixelFormat::ARGB8888; d.pixels=pixels;
    auto ti=imm.create_texture(d), tt=tile.create_texture(d);
    CHECK(ti.valid() && tt.valid());
    CommandRecorder rec;
    rec.begin_frame(); rec.fill_rect(0,0,640,360,Color::rgb(0,255,0));
    SpriteParams sp;
    sp.tex=ti; sp.w=4; sp.h=1; sp.blend=BlendMode::StraightAlpha;
    rec.draw_sprite(sp);
    CHECK(imm.execute_frame(rec.commands()));
    sp.tex=tt;
    rec.begin_frame(); rec.fill_rect(0,0,640,360,Color::rgb(0,255,0)); rec.draw_sprite(sp);
    CHECK(tile.execute_frame(rec.commands()));
    const auto* fb=imm.framebuffer();
    auto pixel=[&](u32 x) { return static_cast<u16>(fb[x*2] | (fb[x*2+1]<<8)); };
    CHECK(pixel(0)==0xf800); CHECK(pixel(1)==0x001f);
    CHECK(pixel(2)==0x07e0); CHECK(pixel(3)==0x83e0);
    CHECK(std::memcmp(fb,tile.framebuffer(),imm.fb_stride()*360)==0);

    std::vector<facility::SpriteBlob> blobs;
    CHECK(facility::load_runtime_sprites("assets/facility_omega/runtime",blobs));
    if (blobs.size() < 60) return 1; // Missing runtime assets must not silently PASS.
    facility::TexBank bank;
    std::vector<facility::SpriteBlob> atlases;
    CHECK(facility::load_runtime_atlases("assets/facility_omega/runtime",blobs,atlases));
    CHECK(atlases.size()==2);
    for (const auto& b:atlases) {
        TextureDesc td;
        td.width=b.width; td.height=b.height; td.stride=b.stride;
        td.format=b.rgb565 ? PixelFormat::RGB565 : PixelFormat::ARGB8888;
        td.pixels=b.pixels.data();
        auto a=imm.create_texture(td), t=tile.create_texture(td);
        CHECK(a.valid() && t.valid() && a.v==t.v);
        for (const auto& sprite:blobs) if (sprite.atlas_file==b.name)
            bank.put(sprite.name,{a,sprite.width,sprite.height,sprite.anchor_x,sprite.anchor_y,
                                 sprite.atlas_x,sprite.atlas_y});
    }
    CHECK(bank.size()==blobs.size());
    facility::AppState state;
    facility::sim_reset(state,1234);
    state.enemy_count_target=0;
    for (int k=0;k<3;++k) {
        facility::Enemy e;
        e.kind=static_cast<facility::EnemyKind>(k); e.alive=true; e.hp=k==0 ? 1 : 20;
        e.world_x=2048+40+k*45; e.world_y=2048+12;
        state.enemies.push_back(e);
    }
    facility::Enemy extra;
    extra.kind=facility::EnemyKind::Drone; extra.alive=true; extra.hp=10;
    extra.world_x=1930; extra.world_y=2010;
    state.enemies.push_back(extra);
    bool seen_explosion=false, seen_spark=false;
    bool captured=false;
    for (int frame=0;frame<60;++frame) {
        facility::sim_step(state,nullptr);
        facility::camera_follow(state,640,360);
        rec.begin_frame(); facility::render_scene(rec,state,bank,640,360);
        for (const auto& cmd:rec.commands()) {
            if (cmd.op != RecOp::Sprite) continue;
            auto matches=[&](const char* name) {
                const auto* tex=bank.find(name);
                return cmd.sp.tex.v==tex->id.v && cmd.sp.src_x==tex->sx && cmd.sp.src_y==tex->sy;
            };
            if (matches("engineer_idle") || matches("crawler_0") || matches("console"))
                CHECK(cmd.sp.blend==BlendMode::StraightAlpha);
        }
        for (const auto& fx:state.effects) if (fx.life) {
            if (fx.explosion) seen_explosion=true; else seen_spark=true;
        }
        CHECK(imm.execute_frame(rec.commands()));
        CHECK(tile.execute_frame(rec.commands()));
        CHECK(std::memcmp(imm.framebuffer(),tile.framebuffer(),imm.fb_stride()*360)==0);
        // Explicit opt-in artifact: synthetic combat fixture, not normal gameplay.
        if (argc==2 && seen_explosion && !captured) {
            std::ofstream file(std::string(argv[1])+".raw",std::ios::binary);
            file.write(reinterpret_cast<const char*>(imm.framebuffer()),imm.fb_stride()*360);
            CHECK(file.good());
            std::printf("fixture capture frame=%d seed=1234 size=640x360 %s.raw\n",frame+1,argv[1]);
            captured=true;
        }
    }
    CHECK(seen_explosion && seen_spark);
    CHECK(state.player.kills>0);
    // K-05: include one Level-Up UI frame in the equality corpus
    {
        state.level_up_pending = true;
        const u32 dmg = state.player.pulse_damage;
        rec.begin_frame();
        facility::render_scene(rec, state, bank, 640, 360);
        CHECK(imm.execute_frame(rec.commands()));
        CHECK(tile.execute_frame(rec.commands()));
        CHECK(std::memcmp(imm.framebuffer(), tile.framebuffer(), imm.fb_stride() * 360) == 0);
        // still paused
        const u64 f0 = state.frame;
        facility::sim_step(state, nullptr);
        CHECK(state.frame == f0);
        CHECK(facility::apply_upgrade(state, 2));
        CHECK(state.player.projectile_count >= 2);
        CHECK(state.player.pulse_damage == dmg);  // did not take damage upgrade
        rec.begin_frame();
        facility::render_scene(rec, state, bank, 640, 360);
        CHECK(imm.execute_frame(rec.commands()));
        CHECK(tile.execute_frame(rec.commands()));
        CHECK(std::memcmp(imm.framebuffer(), tile.framebuffer(), imm.fb_stride() * 360) == 0);
    }
    std::printf("FACILITY visual pixels + transparent draws + 60 frame equality: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
