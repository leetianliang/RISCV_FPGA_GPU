#include "facility/app.hpp"
#include "golden_renderer.hpp"
#include "gpu2d/renderer.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <queue>
#include <string>

using namespace facility;
static int failures=0;
#define CHECK(c) do { if(!(c)){std::printf("FAIL %d %s\n",__LINE__,#c);++failures;} } while(0)

int main(int argc,char** argv) {
    Environment fixture;
    fixture.collision={{100,100,80,80}};
    i32 x=80,y=120;
    move_in_environment(fixture,x,y,200,0,8);
    CHECK(x==92 && y==120); // Swept motion cannot tunnel through 80px obstruction.
    move_in_environment(fixture,x,y,20,30,8);
    CHECK(x==92 && y==150); // Slide down the left side.
    move_in_environment(fixture,x,y,0,60,8);
    move_in_environment(fixture,x,y,200,0,8);
    CHECK(x==292 && y==210);
    for(i32 sx:{-1,1}) for(i32 sy:{-1,1}) {
        x=sx<0?80:200; y=sy<0?80:200;
        move_in_environment(fixture,x,y,-sx*100,-sy*100,8);
        CHECK(position_clear(fixture,x,y,8));
    }
    // Chase through the obstacle's projection must go around, not stall or cross it.
    x=60;y=140;
    for(int frame=0;frame<500;++frame) {
        const i32 dx=(240>x)-(240<x),dy=(140>y)-(140<y);
        move_enemy_in_environment(fixture,x,y,dx,dy,10,240,140);
        CHECK(position_clear(fixture,x,y,10));
    }
    CHECK(x>=230 && std::abs(y-140)<12);

    AppState s;
    sim_reset(s,1234);
    CHECK(s.environment.zones.size()==4);
    CHECK(s.environment.collision.size()==s.environment.structures.size());
    // Production player input must use the authored collision layer and slide.
    s.player.world_x=1600;s.player.world_y=1680;
    player_move(s,false,false,false,true,100);
    CHECK(s.player.world_x==1616 && s.player.world_y==1680);
    player_move(s,false,true,false,true,20);
    CHECK(s.player.world_x==1616 && s.player.world_y==1700);
    sim_reset(s,1234);
    // Raster connectivity with the actual actor radius, not sprite coverage.
    for(i32 radius:{8,18}) {
        constexpr int n=64;
        bool clear[n*n]{},seen[n*n]{};
        int free=0,first=-1;
        for(int yy=0;yy<n;++yy) for(int xx=0;xx<n;++xx) {
            const int k=yy*n+xx;
            clear[k]=position_clear(s.environment,1536+xx*16+8,1536+yy*16+8,radius);
            if(clear[k]) { ++free;first=k; }
        }
        CHECK(free>n*n*70/100);
        std::queue<int> q;
        q.push(first);seen[first]=true;int reached=0;
        while(!q.empty()) {
            int k=q.front();q.pop();++reached;
            for(int dir:{-1,1,-n,n}) {
                int next=k+dir;
                if(next<0 || next>=n*n || (std::abs(dir)==1 && next/n!=k/n)) continue;
                if(clear[next] && !seen[next]) {seen[next]=true;q.push(next);}
            }
        }
        CHECK(reached==free);
        std::printf("hero radius=%d open=%d/%d connected=%d\n",radius,free,n*n,reached);
    }
    int max_groups=0;
    for(int cy=1536;cy<=2200;cy+=32) for(int cx=1536;cx<=1920;cx+=32) {
        int count=0;
        for(const auto& b:s.environment.collision)
            if(b.x<cx+640 && b.x+b.w>cx && b.y<cy+360 && b.y+b.h>cy) ++count;
        max_groups=std::max(max_groups,count);
    }
    CHECK(max_groups<=6);
    std::printf("maximum collidable groups per 640x360 view=%d\n",max_groups);
    for(int i=0;i<100;++i) {
        camera_follow(s,640,360);
        spawn_enemy(s,static_cast<EnemyKind>(i%3));
        const auto& e=s.enemies.back();
        CHECK(position_clear(s.environment,e.world_x,e.world_y,e.kind==EnemyKind::Tank?18:10));
    }
    // Decal traversal is unrestricted: clearance derives exclusively from collision layer.
    CHECK(position_clear(s.environment,2016,1952,8));

    gpu2d::GoldenBackend imm,tile;
    gpu2d::ProfileDesc profile; profile.width=640;profile.height=360;
    CHECK(imm.init(profile) && tile.init(profile));tile.set_backend(gpu2d::BackendKind::Tile32);
    std::vector<SpriteBlob> sprites,atlases;
    CHECK(load_runtime_sprites("assets/facility_omega/runtime",sprites));
    CHECK(load_runtime_atlases("assets/facility_omega/runtime",sprites,atlases));
    TexBank bank;
    for(const auto& a:atlases) {
        gpu2d::TextureDesc d; d.width=a.width;d.height=a.height;d.stride=a.stride;
        d.format=a.rgb565?gpu2d::PixelFormat::RGB565:gpu2d::PixelFormat::ARGB8888;d.pixels=a.pixels.data();
        auto id=imm.create_texture(d),other=tile.create_texture(d);
        CHECK(id.valid() && id.v==other.v);
        for(const auto& b:sprites) if(b.atlas_file==a.name)
            bank.put(b.name,{id,b.width,b.height,b.anchor_x,b.anchor_y,b.atlas_x,b.atlas_y});
    }
    auto capture=[&](const std::string& name) {
        if(argc!=2)return;
        std::ofstream f(std::string(argv[1])+"/"+name+".raw",std::ios::binary);
        f.write(reinterpret_cast<const char*>(imm.framebuffer()),imm.fb_stride()*360);
        CHECK(f.good());
    };
    gpu2d::CommandRecorder rec;
    // Three overlapping views across the authored area, same zoom and no entities.
    for(int view=0;view<3;++view) {
        rec.begin_frame();
        const i32 camera_x=1536+view*192,camera_y=1576+view*240;
        render_environment(rec,s.environment,bank,camera_x,camera_y,640,360);
        CHECK(rec.sprite_count()<80);
        CHECK(imm.execute_frame(rec.commands()) && tile.execute_frame(rec.commands()));
        CHECK(std::memcmp(imm.framebuffer(),tile.framebuffer(),imm.fb_stride()*360)==0);
        capture("map_view_"+std::to_string(view));
    }
    // Continuous scrolling uses the real map renderer, no image stitching.
    for(int frame=0;frame<48;++frame) {
        rec.begin_frame();
        render_environment(rec,s.environment,bank,1536+frame*8,1576+frame*12,640,360);
        CHECK(imm.execute_frame(rec.commands()) && tile.execute_frame(rec.commands()));
        CHECK(std::memcmp(imm.framebuffer(),tile.framebuffer(),imm.fb_stride()*360)==0);
        capture("scroll_"+std::to_string(frame));
    }
    // Four exact 512px captures form an overview without exceeding backend FB arena.
    gpu2d::GoldenBackend overview;
    profile.width=512;profile.height=512;CHECK(overview.init(profile));
    TexBank full;
    for(const auto& a:atlases) {
        gpu2d::TextureDesc d;d.width=a.width;d.height=a.height;d.stride=a.stride;
        d.format=a.rgb565?gpu2d::PixelFormat::RGB565:gpu2d::PixelFormat::ARGB8888;d.pixels=a.pixels.data();
        auto id=overview.create_texture(d);CHECK(id.valid());
        for(const auto& b:sprites) if(b.atlas_file==a.name)
            full.put(b.name,{id,b.width,b.height,b.anchor_x,b.anchor_y,b.atlas_x,b.atlas_y});
    }
    for(int debug=0;debug<2;++debug) for(int part=0;part<4;++part) {
        rec.begin_frame();render_environment(rec,s.environment,full,1536+(part%2)*512,1536+(part/2)*512,512,512,debug!=0);
        CHECK(overview.execute_frame(rec.commands()));
        if(argc==2) {
            std::ofstream f(std::string(argv[1])+(debug?"/collision_part_":"/hero_part_")+std::to_string(part)+".raw",std::ios::binary);
            f.write(reinterpret_cast<const char*>(overview.framebuffer()),overview.fb_stride()*512);CHECK(f.good());
        }
    }
    std::printf("APP2 map collision/connectivity/rendering: %s\n",failures?"FAIL":"PASS");
    return failures?1:0;
}
