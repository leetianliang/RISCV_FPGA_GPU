#pragma once
#include "gpu2d/types.hpp"
#include "facility/spatial.hpp"
#include <array>
#include <bitset>
namespace facility {
inline constexpr gpu2d::u32 kEnemyCapacity=1024,kProjectileCapacity=2048,kPickupCapacity=2048,kEffectCapacity=1024;
inline constexpr gpu2d::u32 kTicksPerSecond=60;
struct DirectorPhase {
    gpu2d::u32 start_tick,spawn_interval,max_active_enemies,group_min,group_max;
    std::array<gpu2d::u32,5> weights;
    gpu2d::u32 elite_interval;
};
inline constexpr std::array<DirectorPhase,4> kDirectorPhases={{
    {0,24,48,1,2,{65,35,0,0,0},0},
    {7200,15,160,2,4,{30,25,15,30,0},0},
    {18000,10,360,3,6,{25,25,15,30,5},300},
    {28800,8,700,4,8,{20,25,20,28,7},180}}};
struct EnemyStats {gpu2d::i32 hp,speed,radius,hit_radius,contact;gpu2d::u32 xp;};
inline constexpr std::array<EnemyStats,5> kEnemyStats={{{2,2,10,14,2,1},{3,1,10,16,2,2},
    {10,1,18,22,6,3},{3,4,10,14,3,2},{40,2,18,24,10,12}}};
enum class WeaponId : gpu2d::u8 {Pulse,Orbit,Nova,Field};
struct WeaponState {
    gpu2d::u32 level=0,cooldown=0,phase=0,age=0;
    gpu2d::i32 x=0,y=0,radius=0;
    std::bitset<kEnemyCapacity> hit;
};
struct WeaponTuning {gpu2d::u32 cooldown,damage,count,radius,duration,speed;};
inline WeaponTuning weapon_tuning(WeaponId id,gpu2d::u32 level) {
    const auto l=std::clamp(level,1u,5u);
    switch(id) {
        case WeaponId::Pulse:return {20-2*l,l,1+(l-1)/2,220,90,6+(l-1)/2};
        case WeaponId::Orbit:return {14-l,l,1+(l-1)/2,42+6*l,0,1u+(l>=3)};
        case WeaponId::Nova:return {240-20*l,2+l,1,80+20*l,36,0};
        default:return {36-4*l,l,1,42+8*l,0,0};
    }
}
struct BuildChoice { // 0..3 weapon unlock/level; 4 damage, 5 fire rate, 6 shots, 7 health, 8 magnet
    gpu2d::u32 kind=4;
};
struct GameplayState {
    std::array<WeaponState,4> weapons{};
    std::array<BuildChoice,3> choices{{{4},{5},{6}}};
    bool choices_generated=false;
    gpu2d::u32 phase=0,elite_timer=0,spawned=0,collected=0,repairs=0;
    gpu2d::u32 magnet_radius=160,repair_percent=6;
    GameplaySpatialGrid enemies,pickups;
    GameplaySpatialGrid::Result candidates;
};
struct AppState;
void init_gameplay(AppState&);
void rebuild_enemy_grid(AppState&);
void generate_choices(AppState&);
void damage_enemy(AppState&,gpu2d::u32,gpu2d::i32);
void spawn_repair(AppState&,gpu2d::i32,gpu2d::i32);
void update_weapons(AppState&);
void director_step(AppState&);
gpu2d::u32 phase_at(gpu2d::u64 tick);
void orbit_position(const AppState&,gpu2d::u32 drone,gpu2d::i32&,gpu2d::i32&);
const char* choice_name(const BuildChoice&);
}
