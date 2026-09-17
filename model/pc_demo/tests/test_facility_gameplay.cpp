#include "facility/app.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
using namespace facility;
static int failures=0;
#define CHECK(x) do{if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);++failures;}}while(0)
static void add(AppState& s,EnemyKind k,i32 x,i32 y,i32 hp=1000){Enemy e;e.alive=true;e.kind=k;e.world_x=x;e.world_y=y;e.hp=hp;s.enemies.push_back(e);}
static void pick(AppState& s) {
    if(!s.level_up_pending)return;
    u32 choice=0;
    for(u32 i=0;i<3;++i){auto k=s.gameplay.choices[i].kind;if(k<4 && !s.gameplay.weapons[k].level){choice=i;break;}}
    CHECK(apply_upgrade(s,choice));
}
int main(int argc,char** argv){
    const std::string mode=argc>1?argv[1]:"director";
    AppState s;sim_reset(s,1234);camera_follow(s,640,360);
    if(mode=="director") {
        CHECK(phase_at(7199)==0 && phase_at(7200)==1 && phase_at(18000)==2 && phase_at(28800)==3);
        for(u32 p=0;p<4;++p) {
            AppState a,b;sim_reset(a,77);sim_reset(b,77);camera_follow(a,640,360);camera_follow(b,640,360);
            a.frame=b.frame=kDirectorPhases[p].start_tick;
            for(int i=0;i<800;++i){++a.frame;++b.frame;director_step(a);director_step(b);}
            CHECK(hash_sim(a)==hash_sim(b));CHECK(live_enemy_count(a)<=kDirectorPhases[p].max_active_enemies);
            bool runner=false,elite=false;
            for(const auto& e:a.enemies){runner|=e.kind==EnemyKind::Runner;elite|=e.kind==EnemyKind::Elite;
                CHECK(!in_view(a,e.world_x,e.world_y,0));CHECK(position_clear(a.environment,e.world_x,e.world_y,kEnemyStats[u32(e.kind)].radius));}
            if(p==0)CHECK(!runner && !elite);
            if(p==1)CHECK(runner && !elite);
            if(p>=2)CHECK(runner && elite);
            std::printf("phase %u count %u hash %08x\n",p,live_enemy_count(a),hash_sim(a));
        }
        for(auto edge:{0,4095}) {s.player.world_x=edge;s.player.world_y=edge;camera_follow(s,640,360);
            s.enemies.clear();for(int i=0;i<100;++i)spawn_enemy(s,EnemyKind::Elite);
            for(const auto& e:s.enemies){CHECK(!in_view(s,e.world_x,e.world_y,0));CHECK(position_clear(s.environment,e.world_x,e.world_y,18));}}
    } else if(mode=="enemies") {
        CHECK(kEnemyStats[3].speed>kEnemyStats[0].speed && kEnemyStats[3].hp<kEnemyStats[2].hp);
        CHECK(kEnemyStats[4].hp>kEnemyStats[2].hp && kEnemyStats[4].contact>kEnemyStats[0].contact);
        CHECK(kEnemyStats[2].xp>kEnemyStats[0].xp && kEnemyStats[4].xp>kEnemyStats[2].xp);
        for(u32 k=0;k<5;++k) {
            sim_reset(s,1234);s.enemy_count_target=0;s.player.fire_cooldown=10000;
            s.player.world_x=1850;s.player.world_y=1700;add(s,static_cast<EnemyKind>(k),1590,1700);
            CHECK(position_clear(s.environment,1590,1700,kEnemyStats[k].radius));
            for(int i=0;i<180;++i){sim_step(s,nullptr);CHECK(position_clear(s.environment,s.enemies[0].world_x,s.enemies[0].world_y,kEnemyStats[k].radius));}
            CHECK(enemy_sprite_name(static_cast<EnemyKind>(k),0)!=nullptr);
        }
        sim_reset(s,1234);s.gameplay.repair_percent=100;add(s,EnemyKind::Elite,2048,2048,1);damage_enemy(s,0,1);
        CHECK(!s.enemies[0].alive && s.player.kills==1);CHECK(s.xp_gems.size()==2);
        s.player.hp=95;sim_step(s,nullptr);CHECK(s.player.hp==100 && s.gameplay.repairs==1);
        sim_reset(s,1234);s.enemy_count_target=0;s.player.world_x=2040;s.player.hp=80;
        spawn_repair(s,2050,2048); // Across adjacent 128px cells.
        for(int i=0;i<4;++i)sim_step(s,nullptr);
        CHECK(s.player.hp==100 && s.gameplay.repairs==1);
    } else if(mode=="weapons") {
        for(u32 id=0;id<4;++id)for(u32 level=1;level<5;++level){auto a=weapon_tuning(WeaponId(id),level),b=weapon_tuning(WeaponId(id),level+1);CHECK(a.damage!=b.damage || a.cooldown!=b.cooldown || a.radius!=b.radius || a.count!=b.count);}
        s.enemy_count_target=0;add(s,EnemyKind::Drone,2148,2048);add(s,EnemyKind::Drone,2048,2148);
        fire_pulse(s);CHECK(s.bullets[0].vx>0 && s.bullets[0].vy==0); // nearest tie -> slot 0
        s.player.projectile_count=3;s.bullets.clear();fire_pulse(s);CHECK(live_bullet_count(s)==3);
        for(auto& b:s.bullets)b.life=0;
        s.player.fire_cooldown=1000;sim_step(s,nullptr);CHECK(live_bullet_count(s)==0);
        sim_reset(s,1234);s.enemy_count_target=0;s.player.fire_cooldown=10000;
        s.gameplay.weapons[1].level=3;i32 x,y;orbit_position(s,0,x,y);add(s,EnemyKind::Drone,x,y);
        rebuild_enemy_grid(s);update_weapons(s);CHECK(s.enemies[0].hp<1000);CHECK(s.gameplay.weapons[1].phase!=0);
        sim_reset(s,1234);s.enemy_count_target=0;s.player.fire_cooldown=10000;s.gameplay.weapons[2].level=1;
        add(s,EnemyKind::Drone,2098,2048);rebuild_enemy_grid(s);
        for(int i=0;i<36;++i)update_weapons(s);
        CHECK(s.enemies[0].hp==997);CHECK(s.gameplay.weapons[2].age==0); // once per wave
        sim_reset(s,1234);s.enemy_count_target=0;s.player.fire_cooldown=10000;s.gameplay.weapons[3].level=1;
        add(s,EnemyKind::Drone,2098,2048);add(s,EnemyKind::Drone,2099,2048);rebuild_enemy_grid(s);update_weapons(s);
        CHECK(s.enemies[0].hp==999 && s.enemies[1].hp==1000); // radius 50 inclusive
        for(auto& w:s.gameplay.weapons)w.level=5;
        for(int i=0;i<120;++i){pick(s);sim_step(s,nullptr);}
        CHECK(s.gameplay.weapons[1].phase!=0);
        sim_reset(s,1234);s.enemy_count_target=0;s.player.fire_cooldown=10000;
        add(s,EnemyKind::Drone,2052,2048,1);
        Bullet cross;cross.alive=true;cross.world_x=2044;cross.world_y=2048;cross.vx=6;s.bullets.push_back(cross);
        sim_step(s,nullptr);CHECK(!s.enemies[0].alive && !s.bullets[0].alive);
    } else if(mode=="upgrades") {
        AppState b;sim_reset(b,1234);
        for(int event=0;event<40;++event) {
            generate_choices(s);generate_choices(b);s.level_up_pending=b.level_up_pending=true;
            for(u32 i=0;i<3;++i){auto k=s.gameplay.choices[i].kind;CHECK(k==b.gameplay.choices[i].kind);if(k<4)CHECK(s.gameplay.weapons[k].level<5);for(u32 j=0;j<i;++j)CHECK(k!=s.gameplay.choices[j].kind);}
            auto f=s.frame;sim_step(s,nullptr);CHECK(s.frame==f);CHECK(!apply_upgrade(s,3));
            CHECK(apply_upgrade(s,0) && apply_upgrade(b,0));CHECK(hash_sim(s)==hash_sim(b));
        }
        for(const auto& w:s.gameplay.weapons)CHECK(w.level==5);
    } else if(mode=="determinism") {
        AppState b;sim_reset(b,1234);
        u32 max_enemies=0;
        for(int i=0;i<600;++i) {
            // Real movement inputs, no enemies/XP/weapons injected. Follow nearby XP.
            bool keys[4]{};
            int closest=-1;i64 best=INT64_MAX;
            for(u32 j=0;j<s.xp_gems.size();++j)if(s.xp_gems[j].alive){auto& g=s.xp_gems[j];i64 dx=g.world_x-s.player.world_x,dy=g.world_y-s.player.world_y,d=dx*dx+dy*dy;if(d<best){best=d;closest=j;}}
            if(closest>=0){auto& g=s.xp_gems[closest];keys[0]=g.world_y<s.player.world_y-5;keys[1]=g.world_y>s.player.world_y+5;keys[2]=g.world_x<s.player.world_x-5;keys[3]=g.world_x>s.player.world_x+5;}
            pick(s);pick(b);sim_step(s,keys);sim_step(b,keys);CHECK(hash_sim(s)==hash_sim(b));max_enemies=std::max(max_enemies,live_enemy_count(s));
        }
        std::printf("normal600 level=%u kills=%u spawned=%u collected=%u upgrades=%u hp=%d max_enemies=%u hash=%08x\n",s.player.level,s.player.kills,s.gameplay.spawned,s.gameplay.collected,s.upgrades_taken,s.player.hp,max_enemies,hash_sim(s));
        CHECK(s.gameplay.spawned>0 && s.player.kills>0 && s.gameplay.collected>0 && s.player.level>=2 && s.upgrades_taken>=1);
        b.player.fire_period++;CHECK(hash_sim(s)!=hash_sim(b));
    } else if(mode=="capacity") {
        for(int i=0;i<1100;++i)spawn_enemy(s,EnemyKind::Drone);
        CHECK(s.enemies.size()==1024);
        for(int i=0;i<2200;++i)spawn_xp(s,2048,2048,1);
        CHECK(s.xp_gems.size()==2048);
        for(int i=0;i<2100;++i)fire_pulse(s);
        CHECK(s.bullets.size()==2048);
        const auto ec=s.enemies.capacity(),bc=s.bullets.capacity(),pc=s.xp_gems.capacity();
        for(int i=0;i<60;++i){pick(s);sim_step(s,nullptr);}
        CHECK(s.enemies.capacity()==ec && s.bullets.capacity()==bc && s.xp_gems.capacity()==pc);
        std::printf("capacity enemy=1024 projectile=2048 pickup=2048 hash=%08x\n",hash_sim(s));
    } else return 2;
    std::printf("APP2_002 %s %s\n",mode.c_str(),failures?"FAIL":"PASS");return failures?1:0;
}
