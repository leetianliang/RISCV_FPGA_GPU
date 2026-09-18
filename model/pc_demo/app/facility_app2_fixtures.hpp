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
    if(dense)s.player.xp_need=100000; // Stress must advance, not stop on an incidental upgrade.
    const bool composed=name=="fx"||name=="elite"||name=="showcase";
    const u32 count=composed?0:dense?300:name=="mid"?110:30;
    for(u32 candidate=0;s.enemies.size()<count && candidate<20000;++candidate) {
        Enemy e;e.kind=static_cast<EnemyKind>(candidate%(name=="mid"?4:5));e.alive=true;e.hp=dense?100000:1000;
        const i32 width=name=="mid"?1000:570,height=name=="mid"?700:290;
        e.world_x=s.player.world_x-width/2+static_cast<i32>((candidate*137+31)%width);
        e.world_y=s.player.world_y-height/2+static_cast<i32>((candidate*83+17)%height);
        const i32 dx=e.world_x-s.player.world_x,dy=e.world_y-s.player.world_y;
        if(dx*dx+dy*dy<65*65 || !position_clear(s.environment,e.world_x,e.world_y,kEnemyStats[u32(e.kind)].radius))continue;
        s.enemies.push_back(e);
    }
    auto enemy=[&](EnemyKind kind,i32 dx,i32 dy) {
        Enemy e;e.kind=kind;e.alive=true;e.hp=1000;
        e.world_x=s.player.world_x+dx;e.world_y=s.player.world_y+dy;
        if(position_clear(s.environment,e.world_x,e.world_y,kEnemyStats[u32(kind)].radius))s.enemies.push_back(e);
    };
    if(name=="fx") {
        // An open central ring keeps the Nova boundary, Field and both drones legible.
        const i32 points[][2]={{-155,-40},{-125,-90},{-45,-110},{35,-105},{145,-30},{165,35},
                              {95,100},{20,120},{-60,110},{-145,70},{-235,30},{235,100}};
        for(u32 i=0;i<12;++i)enemy(static_cast<EnemyKind>(i==10?2:i==11?4:i%3==2?3:i%2),points[i][0],points[i][1]);
        s.gameplay.weapons[2].age=14;s.gameplay.weapons[2].radius=112;s.gameplay.weapons[1].phase=26;
    } else if(name=="elite") {
        // Exactly one Elite is the focal threat; small escorts leave its silhouette clear.
        enemy(EnemyKind::Elite,125,-10);enemy(EnemyKind::Tank,-155,70);
        const i32 points[][2]={{-180,-65},{-80,-100},{35,-95},{205,45},{175,100},{40,105},{-110,110}};
        for(u32 i=0;i<7;++i)enemy(static_cast<EnemyKind>(i%2),points[i][0],points[i][1]);
        s.gameplay.weapons[2].age=0;s.gameplay.weapons[2].radius=0;s.gameplay.weapons[2].cooldown=160;
        s.gameplay.weapons[1].phase=48;
    } else if(name=="showcase") {
        // Asymmetric flanks and a clear player center, using legal positions on the frozen map.
        enemy(EnemyKind::Elite,170,15);enemy(EnemyKind::Tank,-175,-45);enemy(EnemyKind::Tank,125,105);
        const i32 points[][2]={{-240,-90},{-210,-110},{-145,-110},{-100,-90},{-55,-115},{25,-105},
            {205,-35},{245,10},{240,75},{205,115},{165,125},{70,120},{10,125},{-45,115},
            {-100,95},{-140,125},{-195,95},{-235,55},{-260,5},{-205,10},{-130,40}};
        for(u32 i=0;i<21;++i)enemy(static_cast<EnemyKind>(i%3==2?3:i%2),points[i][0],points[i][1]);
        s.gameplay.weapons[2].age=18;s.gameplay.weapons[2].radius=90;s.gameplay.weapons[1].phase=8;
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
    if(name=="level") {s.pending_levelups=1;s.level_up_pending=true;generate_choices(s);}
}
}
