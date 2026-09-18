#include "facility/app.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>

namespace facility {
namespace {
u32 root(u32 v) {u32 r=0,b=1u<<30;while(b>v)b>>=2;while(b){if(v>=r+b){v-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r?r:1;}
template<class T> void place(std::vector<T>& slots,const T& value,u32 cap) {
    for(auto& slot:slots)if(!slot.alive){slot=value;return;}
    if(slots.size()<cap)slots.push_back(value);
}
void effect(AppState& s,i32 x,i32 y,bool explosion) {
    Effect f{x,y,explosion?18u:7u,explosion};
    for(auto& old:s.effects)if(!old.life){old=f;return;}
    if(s.effects.size()<kEffectCapacity)s.effects.push_back(f);
}
void radius_damage(AppState& s,i32 x,i32 y,i32 r,i32 damage,WeaponState* nova=nullptr) {
    auto& q=s.gameplay.candidates;s.gameplay.enemies.query(x-r,y-r,x+r,y+r,q);
    for(int j=0;j<q.count;++j) {
        const auto id=static_cast<u32>(q.ids[j]);auto& e=s.enemies[id];
        const i64 dx=e.world_x-x,dy=e.world_y-y;
        if(e.alive && dx*dx+dy*dy<=i64(r)*r && (!nova || !nova->hit[id])) {
            if(nova)nova->hit[id]=true;
            damage_enemy(s,id,damage);
        }
    }
}
constexpr std::array<i32,32> sine={0,50,98,142,181,213,237,251,256,251,237,213,181,142,98,50,
    0,-50,-98,-142,-181,-213,-237,-251,-256,-251,-237,-213,-181,-142,-98,-50};
}
u32 phase_at(u64 tick) {u32 p=0;for(u32 i=1;i<4;++i)if(tick>=kDirectorPhases[i].start_tick)p=i;return p;}
void init_gameplay(AppState& s) {s.gameplay.weapons[0].level=1;}
void rebuild_enemy_grid(AppState& s) {
    s.gameplay.enemies.clear();
    for(u32 i=0;i<s.enemies.size();++i)if(s.enemies[i].alive)
        s.gameplay.enemies.insert(i,s.enemies[i].world_x,s.enemies[i].world_y);
}
void spawn_enemy(AppState& s,EnemyKind kind) {
    if(live_enemy_count(s)>=std::min(s.enemy_count_target,kEnemyCapacity))return;
    const auto& st=kEnemyStats[static_cast<u32>(kind)];
    const i32 vw=s.view_w,vh=s.view_h;
    const u32 first=s.rng.range(0,4);
    for(u32 attempt=0;attempt<128;++attempt) {
        const i32 margin=32+static_cast<i32>(s.rng.range(0,129));
        i32 x=s.cam.x+s.rng.irange(0,vw),y=s.cam.y+s.rng.irange(0,vh);
        switch((first+attempt)%4) {
            case 0:y=s.cam.y-margin;break;case 1:y=s.cam.y+vh+margin;break;
            case 2:x=s.cam.x-margin;break;default:x=s.cam.x+vw+margin;break;
        }
        if(x>=s.cam.x && x<s.cam.x+vw && y>=s.cam.y && y<s.cam.y+vh)continue;
        const i64 dx=x-s.player.world_x,dy=y-s.player.world_y;
        if(dx*dx+dy*dy<64*64 || !position_clear(s.environment,x,y,st.radius))continue;
        Enemy e;e.alive=true;e.kind=kind;e.hp=st.hp;e.world_x=x;e.world_y=y;
        place(s.enemies,e,kEnemyCapacity);++s.gameplay.spawned;return;
    }
}
void director_step(AppState& s) {
    s.gameplay.phase=phase_at(s.frame);const auto& phase=kDirectorPhases[s.gameplay.phase];
    if(++s.spawn_timer<phase.spawn_interval)return;
    s.spawn_timer=0;
    const auto cap=std::min(phase.max_active_enemies,s.enemy_count_target);
    const auto group=s.rng.range(phase.group_min,phase.group_max+1);
    u32 total=0;for(auto w:phase.weights)total+=w;
    for(u32 j=0;j<group && live_enemy_count(s)<cap;++j) {
        u32 n=s.rng.range(0,total),kind=0;
        while(kind<4 && n>=phase.weights[kind]){n-=phase.weights[kind];++kind;}
        spawn_enemy(s,static_cast<EnemyKind>(kind));
    }
    if(phase.elite_interval && s.frame-s.gameplay.elite_timer>=phase.elite_interval && live_enemy_count(s)<cap) {
        spawn_enemy(s,EnemyKind::Elite);s.gameplay.elite_timer=static_cast<u32>(s.frame);
    }
}
void spawn_xp(AppState& s,i32 x,i32 y,u32 value) {
    XpGem g;g.alive=true;g.world_x=std::clamp(x+s.rng.irange(-6,6),0,4095);
    g.world_y=std::clamp(y+s.rng.irange(-6,6),0,4095);g.value=value?value:1;
    place(s.xp_gems,g,kPickupCapacity);
}
void spawn_repair(AppState& s,i32 x,i32 y) {
    XpGem g;g.alive=true;g.repair=true;g.world_x=std::clamp(x,0,4095);g.world_y=std::clamp(y,0,4095);g.value=20;
    place(s.xp_gems,g,kPickupCapacity);
}
void damage_enemy(AppState& s,u32 id,i32 damage) {
    auto& e=s.enemies[id];if(!e.alive)return;
    e.hp-=damage;e.flash=6;effect(s,e.world_x,e.world_y,e.hp<=0);
    if(e.hp<=0) {
        e.alive=false;++s.player.kills;
        spawn_xp(s,e.world_x,e.world_y,kEnemyStats[static_cast<u32>(e.kind)].xp);
        if(s.rng.range(0,100)<s.gameplay.repair_percent)spawn_repair(s,e.world_x,e.world_y);
    }
}
void fire_pulse(AppState& s) {
    rebuild_enemy_grid(s);
    const auto tuning=weapon_tuning(WeaponId::Pulse,s.gameplay.weapons[0].level);
    const int id=s.gameplay.enemies.nearest(s.player.world_x,s.player.world_y,tuning.radius);
    i32 vx=0,vy=-static_cast<i32>(tuning.speed);
    if(id>=0) {
        const auto& e=s.enemies[id];const i32 dx=e.world_x-s.player.world_x,dy=e.world_y-s.player.world_y;
        const i32 length=root(dx*dx+dy*dy);vx=dx*static_cast<i32>(tuning.speed)/length;vy=dy*static_cast<i32>(tuning.speed)/length;
    }
    s.player.fire_cooldown=s.player.fire_period;
    const u32 count=std::max(1u,s.player.projectile_count);
    for(u32 i=0;i<count;++i) {
        Bullet b;b.alive=true;b.life=tuning.duration;b.world_x=s.player.world_x;b.world_y=s.player.world_y;
        const i32 off=count==1?0:static_cast<i32>(i)-static_cast<i32>(count/2);
        b.vx=vx-vy*off/6;b.vy=vy+vx*off/6;
        place(s.bullets,b,kProjectileCapacity);
    }
}
void orbit_position(const AppState& s,u32 drone,i32& x,i32& y) {
    const auto& w=s.gameplay.weapons[1];const auto t=weapon_tuning(WeaponId::Orbit,w.level);
    const u32 p=((w.phase>>2)+drone*32/t.count)%32;
    x=s.player.world_x+sine[(p+8)%32]*static_cast<i32>(t.radius)/256;
    y=s.player.world_y+sine[p]*static_cast<i32>(t.radius)/256;
}
void update_weapons(AppState& s) {
    auto& build=s.gameplay.weapons;
    if(s.player.fire_cooldown)--s.player.fire_cooldown;else fire_pulse(s);
    build[0].cooldown=s.player.fire_cooldown;
    for(u32 i=1;i<4;++i) {
        auto& w=build[i];if(!w.level)continue;
        const auto t=weapon_tuning(static_cast<WeaponId>(i),w.level);
        if(w.cooldown)--w.cooldown;
        if(i==1) {
            w.phase=(w.phase+t.speed)%128;
            if(!w.cooldown) {
                for(u32 n=0;n<t.count;++n){i32 x,y;orbit_position(s,n,x,y);radius_damage(s,x,y,20,t.damage);}
                w.cooldown=t.cooldown;
            }
        } else if(i==2) {
            if(!w.age && !w.cooldown){w.age=1;w.x=s.player.world_x;w.y=s.player.world_y;w.hit.reset();w.cooldown=t.cooldown;}
            if(w.age){w.radius=static_cast<i32>(t.radius*w.age/t.duration);radius_damage(s,w.x,w.y,w.radius,t.damage,&w);if(++w.age>t.duration){w.age=0;w.radius=0;}}
        } else {
            w.radius=t.radius;
            if(!w.cooldown){radius_damage(s,s.player.world_x,s.player.world_y,w.radius,t.damage);w.cooldown=t.cooldown;}
        }
    }
}
void generate_choices(AppState& s) {
    std::array<u32,9> pool{};u32 n=0;
    for(u32 i=0;i<4;++i)if(s.gameplay.weapons[i].level<5)pool[n++]=i;
    // Seeded Fisher-Yates of weapon candidates. Fallback stats are distinct and uncapped.
    for(u32 i=n;i>1;--i)std::swap(pool[i-1],pool[s.rng.range(0,i)]);
    for(u32 k:{4u,7u,8u})if(n<3)pool[n++]=k;
    for(u32 i=0;i<3;++i)s.gameplay.choices[i].kind=pool[i];
    s.gameplay.choices_generated=true;
}
const char* choice_name(const BuildChoice& c) {
    static constexpr const char* names[]={"PULSE SHOT","ORBIT DRONE","PLASMA NOVA","ENERGY FIELD","DAMAGE +1","FIRE RATE","MULTI SHOT","MAX HP +10","MAGNET +8"};
    return c.kind<9?names[c.kind]:"INVALID";
}
bool apply_upgrade(AppState& s,u32 choice) {
    if(!s.level_up_pending || choice>=3)return false;
    const auto kind=s.gameplay.choices[choice].kind;
    if(kind<4) {
        auto& w=s.gameplay.weapons[kind];if(w.level>=5)return false;++w.level;
        if(kind==0){++s.player.pulse_damage;s.player.fire_period=std::max(6u,s.player.fire_period-2);s.player.projectile_count=weapon_tuning(WeaponId::Pulse,w.level).count;}
    } else switch(kind) {
        case 4:++s.player.pulse_damage;break;
        case 5:if(s.player.fire_period>6)s.player.fire_period-=2;break;
        case 6:if(s.player.projectile_count<5)++s.player.projectile_count;break;
        case 7:s.player.max_hp+=10;s.player.hp=std::min(s.player.hp+10,s.player.max_hp);break;
        default:s.gameplay.magnet_radius+=8;break;
    }
    ++s.upgrades_taken;
    if(s.pending_levelups)--s.pending_levelups;
    s.level_up_pending=s.pending_levelups>0;
    if(s.level_up_pending)generate_choices(s);
    return true;
}
void player_hurt(AppState& s,i32 damage) {if(damage<=0)return;s.player.hp=std::max(0,s.player.hp-damage);s.player.hurt_timer=12;}
void set_viewport(AppState& s,u32 w,u32 h) {s.view_w=w;s.view_h=h;}
void sim_step(AppState& s,const bool* keys) {
    if(s.paused || s.level_up_pending)return;
    ++s.frame;for(auto& f:s.effects)if(f.life)--f.life;
    player_move(s,keys&&keys[0],keys&&keys[1],keys&&keys[2],keys&&keys[3]);
    camera_follow(s,s.view_w,s.view_h);
    if(s.player.hurt_timer)--s.player.hurt_timer;
    director_step(s);
    for(auto& e:s.enemies)if(e.alive) {
        const auto& st=kEnemyStats[static_cast<u32>(e.kind)];
        const i32 dx=s.player.world_x-e.world_x,dy=s.player.world_y-e.world_y,length=root(dx*dx+dy*dy);
        e.motion_x+=dx*st.speed*256/length;e.motion_y+=dy*st.speed*256/length;
        move_enemy_in_environment(s.environment,e.world_x,e.world_y,e.motion_x/256,e.motion_y/256,st.radius,s.player.world_x,s.player.world_y);
        e.motion_x%=256;e.motion_y%=256;++e.anim;if(e.flash)--e.flash;
        const i32 tx=e.world_x-s.player.world_x,ty=e.world_y-s.player.world_y;
        if(tx*tx+ty*ty<st.hit_radius*st.hit_radius && !s.player.hurt_timer)player_hurt(s,st.contact);
    }
    rebuild_enemy_grid(s);update_weapons(s);
    for(auto& b:s.bullets)if(b.alive) {
        if(!b.life){b.alive=false;continue;}--b.life;b.world_x+=b.vx;b.world_y+=b.vy;
        if(b.world_x<0||b.world_y<0||b.world_x>=4096||b.world_y>=4096){b.alive=false;continue;}
        if(b.enemy)continue;
        auto& q=s.gameplay.candidates;s.gameplay.enemies.query(b.world_x-18,b.world_y-18,b.world_x+18,b.world_y+18,q);
        for(int j=0;j<q.count;++j) {
            auto id=static_cast<u32>(q.ids[j]);const auto& e=s.enemies[id];
            const i32 dx=e.world_x-b.world_x,dy=e.world_y-b.world_y,r=kEnemyStats[static_cast<u32>(e.kind)].radius;
            if(e.alive && dx*dx+dy*dy<r*r){damage_enemy(s,id,s.player.pulse_damage);b.alive=false;break;}
        }
    }
    auto& grid=s.gameplay.pickups;grid.clear();
    for(u32 i=0;i<s.xp_gems.size();++i)if(s.xp_gems[i].alive)grid.insert(i,s.xp_gems[i].world_x,s.xp_gems[i].world_y);
    auto& q=s.gameplay.candidates;const i32 r=static_cast<i32>(s.gameplay.magnet_radius);
    grid.query(s.player.world_x-r,s.player.world_y-r,s.player.world_x+r,s.player.world_y+r,q);
    bool leveled=false;
    for(int j=0;j<q.count;++j) {
        auto& g=s.xp_gems[q.ids[j]];const i32 dx=s.player.world_x-g.world_x,dy=s.player.world_y-g.world_y;
        const i64 d=i64(dx)*dx+i64(dy)*dy;if(d>=i64(r)*r)continue;
        const i32 length=root(static_cast<u32>(std::max<i64>(1,d)));g.vx=dx*5/length;g.vy=dy*5/length;
        g.world_x+=g.vx;g.world_y+=g.vy;
        if(d<12*12) {
            g.alive=false;++s.gameplay.collected;
            if(g.repair){s.player.hp=std::min(s.player.max_hp,s.player.hp+static_cast<i32>(g.value));++s.gameplay.repairs;}
            else {s.player.xp+=g.value;while(s.player.xp>=s.player.xp_need){s.player.xp-=s.player.xp_need;++s.player.level;s.player.xp_need=xp_to_level(s.player.level);++s.pending_levelups;leveled=true;}}
            effect(s,s.player.world_x+14,s.player.world_y,false);
        }
    }
    if(leveled){s.level_up_pending=true;generate_choices(s);}
}
u32 hash_sim(const AppState& s) {
    u32 h=2166136261u;auto mix=[&](u32 v){h^=v;h*=16777619u;};
    mix(static_cast<u32>(s.frame));mix(static_cast<u32>(s.frame>>32));mix(s.rng.state());
    mix(s.player.world_x);mix(s.player.world_y);mix(s.player.hp);mix(s.player.max_hp);mix(s.player.xp);mix(s.player.xp_need);mix(s.player.level);mix(s.player.kills);
    mix(s.player.fire_cooldown);mix(s.player.fire_period);mix(s.player.pulse_damage);mix(s.player.projectile_count);mix(s.player.hurt_timer);mix(s.player.frame);mix(s.player.dir);
    mix(s.spawn_timer);mix(s.enemy_count_target);mix(s.upgrades_taken);mix(s.level_up_pending);
    mix(s.pending_levelups);mix(s.paused);
    mix(s.gameplay.phase);mix(s.gameplay.elite_timer);mix(s.gameplay.spawned);mix(s.gameplay.collected);mix(s.gameplay.repairs);mix(s.gameplay.magnet_radius);mix(s.gameplay.repair_percent);
    mix(s.gameplay.choices_generated);for(const auto& c:s.gameplay.choices)mix(c.kind);
    for(const auto& w:s.gameplay.weapons){mix(w.level);mix(w.cooldown);mix(w.phase);mix(w.age);mix(w.x);mix(w.y);mix(w.radius);for(u32 i=0;i<kEnemyCapacity;++i)mix(w.hit[i]);}
    mix(s.enemies.size());for(const auto& e:s.enemies){mix(e.alive);mix(e.world_x);mix(e.world_y);mix(e.hp);mix(static_cast<u32>(e.kind));mix(e.motion_x);mix(e.motion_y);mix(e.anim);mix(e.flash);}
    mix(s.bullets.size());for(const auto& b:s.bullets){mix(b.alive);mix(b.enemy);mix(b.world_x);mix(b.world_y);mix(b.vx);mix(b.vy);mix(b.life);}
    mix(s.xp_gems.size());for(const auto& p:s.xp_gems){mix(p.alive);mix(p.repair);mix(p.world_x);mix(p.world_y);mix(p.vx);mix(p.vy);mix(p.value);}
    mix(s.effects.size());for(const auto& f:s.effects){mix(f.life);mix(f.world_x);mix(f.world_y);mix(f.explosion);}
    return h;
}
}
