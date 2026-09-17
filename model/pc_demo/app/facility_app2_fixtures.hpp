#pragma once
#include "facility/app.hpp"
#include <string_view>
namespace facility_capture {
inline bool valid_scene(std::string_view name) {
    return name=="mid"||name=="fx"||name=="elite"||name=="level"||name=="density"||name=="technical"||name=="showcase";
}
inline void app2_fixture(facility::AppState& s,std::string_view name) {
    using namespace facility;
    sim_reset(s,1234);s.enemy_count_target=0;s.frame=name=="mid"?7200:18000;
    s.player.world_x=2160;s.player.world_y=2080;s.player.max_hp=s.player.hp=100000;
    s.player.kills=120;s.player.xp=3;
    for(u32 i=0;i<4;++i)s.gameplay.weapons[i].level=(name=="mid" && i>=2)?0:3;
    s.gameplay.weapons[1].phase=12;s.gameplay.weapons[2].age=18;
    s.gameplay.weapons[2].radius=100;s.gameplay.weapons[2].x=s.player.world_x;s.gameplay.weapons[2].y=s.player.world_y;
    s.player.projectile_count=3;s.player.pulse_damage=3;
    camera_follow(s,640,360);
    const bool dense=name=="density"||name=="technical";
    const u32 count=dense?300:name=="mid"?110:30;
    for(u32 candidate=0;s.enemies.size()<count && candidate<20000;++candidate) {
        Enemy e;e.kind=static_cast<EnemyKind>(candidate%5);e.alive=true;e.hp=dense?100000:1000;
        const i32 width=name=="mid"?1000:570,height=name=="mid"?700:290;
        e.world_x=s.player.world_x-width/2+static_cast<i32>((candidate*137+31)%width);
        e.world_y=s.player.world_y-height/2+static_cast<i32>((candidate*83+17)%height);
        const i32 dx=e.world_x-s.player.world_x,dy=e.world_y-s.player.world_y;
        if(dx*dx+dy*dy<65*65 || !position_clear(s.environment,e.world_x,e.world_y,kEnemyStats[u32(e.kind)].radius))continue;
        s.enemies.push_back(e);
    }
    for(u32 i=0;i<(dense?700u:12u);++i) {
        Bullet b;b.alive=true;b.enemy=dense;b.life=180;
        b.world_x=dense?s.cam.x+30+(i%35)*16:s.player.world_x+20+i*8;
        b.world_y=dense?s.cam.y+48+(i/35)*14:s.player.world_y-12-i*6;
        s.bullets.push_back(b);
    }
    for(int i=0;i<12;++i)spawn_xp(s,s.player.world_x-95+(i%4)*18,s.player.world_y+25+(i/4)*18,1);
    s.effects.push_back({s.player.world_x+120,s.player.world_y+50,16,true});
    spawn_repair(s,s.player.world_x-180,s.player.world_y+50);
    if(name=="level") {s.level_up_pending=true;generate_choices(s);}
}
}
