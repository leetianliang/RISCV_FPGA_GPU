#include "facility/app.hpp"
#include "golden_renderer.hpp"
#include "gpu2d/renderer.hpp"
#include "../app/facility_app2_fixtures.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
using namespace facility;
using namespace gpu2d;
static int failures=0;
#define CHECK(c) do {if(!(c)){std::printf("FAIL %d %s\n",__LINE__,#c);++failures;}}while(0)
int main(int argc,char** argv) {
    const bool dense=argc==2 && std::strcmp(argv[1],"density")==0;
    GoldenBackend imm,tile;ProfileDesc p;p.width=640;p.height=360;
    CHECK(imm.init(p)&&tile.init(p));tile.set_backend(BackendKind::Tile32);
    std::vector<SpriteBlob> sprites,atlases;
    CHECK(load_runtime_sprites("assets/facility_omega/runtime",sprites));
    CHECK(load_runtime_atlases("assets/facility_omega/runtime",sprites,atlases));
    TexBank bank;
    for(const auto& a:atlases) {
        TextureDesc d;d.width=a.width;d.height=a.height;d.stride=a.stride;
        d.format=a.rgb565?PixelFormat::RGB565:PixelFormat::ARGB8888;d.pixels=a.pixels.data();
        auto id=imm.create_texture(d),other=tile.create_texture(d);CHECK(id.valid()&&id.v==other.v);
        for(const auto& b:sprites)if(b.atlas_file==a.name)bank.put(b.name,{id,b.width,b.height,b.anchor_x,b.anchor_y,b.atlas_x,b.atlas_y});
    }
    AppState a,b;facility_capture::app2_fixture(a,dense?"density":"fx");
    facility_capture::app2_fixture(b,dense?"density":"fx");
    CommandRecorder rec;bool nova=false,field=false,level=false;u32 rolling=2166136261u;
    u32 min_enemies=1024,min_objects=4096,max_sprites=0,max_commands=0,max_refs=0,max_od=0;
    for(int f=0;f<(dense?60:120);++f) {
        if(!dense && f==40){a.level_up_pending=b.level_up_pending=true;generate_choices(a);generate_choices(b);}
        if(!dense && f==42){CHECK(apply_upgrade(a,0));CHECK(apply_upgrade(b,0));}
        sim_step(a,nullptr);sim_step(b,nullptr);CHECK(hash_sim(a)==hash_sim(b));CHECK(a.player.hp>0);
        u32 enemies=0,objects=0;
        for(const auto& e:a.enemies)if(e.alive){++enemies;CHECK(position_clear(a.environment,e.world_x,e.world_y,kEnemyStats[u32(e.kind)].radius));}
        for(const auto& x:a.bullets)objects+=x.alive;
        for(const auto& x:a.xp_gems)objects+=x.alive;
        for(const auto& x:a.effects)objects+=x.life>0;
        min_enemies=std::min(min_enemies,enemies);min_objects=std::min(min_objects,objects);
        if(dense){CHECK(enemies>=300);CHECK(objects>=700);}
        rec.begin_frame();render_scene(rec,a,bank,640,360);level|=a.level_up_pending;
        for(const auto& c:rec.commands())if(c.op==RecOp::Sprite){
            nova|=c.sp.blend==BlendMode::AddSat && c.sp.filter==FilterMode::Bilinear && c.sp.global_alpha<255 && c.sp.scale_w>100;
            field|=c.sp.blend==BlendMode::StraightAlpha && c.sp.global_alpha<100 && c.sp.scale_w>100;
        }
        CHECK(imm.execute_frame(rec.commands()));CHECK(tile.execute_frame(rec.commands()));
        const auto* pixels=imm.framebuffer();const auto* other_pixels=tile.framebuffer();
        if(std::memcmp(pixels,other_pixels,imm.fb_stride()*360)!=0){std::printf("BLOCKER: backend divergence frame=%d\n",f);return 1;}
        for(u32 i=0;i<imm.fb_stride()*360;++i)rolling=(rolling^pixels[i])*16777619u;
        const auto& t=tile.telemetry();max_sprites=std::max(max_sprites,t.sprite_count);max_commands=std::max(max_commands,t.command_count);
        max_refs=std::max(max_refs,t.workref_count);max_od=std::max(max_od,t.max_overdraw);
    }
    CHECK(nova&&field);if(!dense)CHECK(level);
    std::printf("APP2 %s frames=%d min_enemies=%u min_objects=%u max_sprites=%u commands=%u workrefs=%u OD=%u sim=%08x pixels=%08x %s\n",dense?"density":"corpus",dense?60:120,min_enemies,min_objects,max_sprites,max_commands,max_refs,max_od,hash_sim(a),rolling,failures?"FAIL":"PASS");
    return failures?1:0;
}
