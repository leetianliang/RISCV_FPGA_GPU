#pragma once
#include "facility/app.hpp"

// Capture-only authored snapshot. This is not a gameplay mode or simulated result.
namespace facility_capture {
inline void stage(facility::AppState& s) {
    facility::sim_reset(s,1234);
    s.player.world_x=2160;s.player.world_y=1980;
    s.player.dir=3;s.player.hp=88;s.player.kills=18;s.player.xp=3;
    s.frame=180;s.enemy_count_target=0;
    const facility::Bounds positions[] = {
        {-240,-100,0,0},{-175,-116,0,0},{-110,-92,0,0},{-30,-102,0,0},
        {45,-125,0,0},{125,-110,0,0},{210,-105,0,0},{-262,-20,0,0},
        {-187,5,0,0},{-100,15,0,0},{270,-18,0,0},{-230,94,0,0},
        {-156,120,0,0},{-80,92,0,0},{8,132,0,0},{90,110,0,0},
        {158,97,0,0},{242,122,0,0},{-20,60,0,0},{70,50,0,0}};
    for(int i=0;i<20;++i) {
        facility::Enemy enemy;
        enemy.world_x=s.player.world_x+positions[i].x;
        enemy.world_y=s.player.world_y+positions[i].y;
        enemy.kind=(i==2 || i==16)?facility::EnemyKind::Tank:
                   (i%2?facility::EnemyKind::Crawler:facility::EnemyKind::Drone);
        enemy.alive=true;enemy.hp=20;enemy.anim=i*3;
        s.enemies.push_back(enemy);
    }
    for(int i=0;i<9;++i) {
        facility::Bullet bullet;
        bullet.world_x=2160+24+i*11;bullet.world_y=1976-i*8;
        bullet.vx=6;bullet.vy=-4;bullet.alive=true;s.bullets.push_back(bullet);
    }
    const facility::Bounds drops[]={{2096,2008,0,0},{2113,2022,0,0},{2152,2012,0,0},
        {2181,2028,0,0},{2110,2049,0,0},{2126,2038,0,0},{2165,2046,0,0}};
    for(const auto& drop:drops) {
        facility::XpGem gem;
        gem.world_x=drop.x;gem.world_y=drop.y;
        gem.alive=true;s.xp_gems.push_back(gem);
    }
    s.effects.push_back({2080,2072,12,true});
    s.effects.push_back({2285,1870,5,false});
    s.environment.effects.push_back({"glow_large",2222,2024,false,110});
}
}
