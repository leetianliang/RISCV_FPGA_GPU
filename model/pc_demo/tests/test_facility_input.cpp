#include "../app/facility_input.hpp"
#include "facility/app.hpp"
#include <cstdio>
static int failures=0;
#define CHECK(x) do {if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);++failures;}}while(0)
int main() {
    for(int key=0;key<3;++key) {
        facility::AppState state;facility::sim_reset(state,1234);
        facility::generate_choices(state);state.level_up_pending=true;
        const auto weapon=state.gameplay.choices[key].kind;
        CHECK(weapon<4);const auto before=state.gameplay.weapons[weapon].level;
        const auto tick=state.frame;facility::sim_step(state,nullptr);CHECK(state.frame==tick);
        host::InputState in;in.edge_1=key==0;in.edge_2=key==1;in.edge_3=key==2;in.right=true;
        const int choice=facility_input::finish_input_frame(in,state.level_up_pending);
        CHECK(choice==key && !in.edge_1 && !in.edge_2 && !in.edge_3 && in.right);
        CHECK(facility::apply_upgrade(state,choice));CHECK(!state.level_up_pending);
        CHECK(state.gameplay.weapons[weapon].level==before+1 && state.upgrades_taken==1);
        CHECK(facility_input::finish_input_frame(in,true)==-1); // No replay next frame.
        facility::sim_step(state,nullptr);CHECK(state.frame==tick+1);
    }
    host::InputState in;in.edge_2=true;
    CHECK(facility_input::finish_input_frame(in,false)==-1);
    CHECK(facility_input::finish_input_frame(in,true)==-1); // Early key is not queued.
    in.edge_1=in.edge_2=in.edge_3=true;
    CHECK(facility_input::finish_input_frame(in,true)==0);
    facility::AppState state;facility::sim_reset(state,1234);
    in={};in.edge_pause=true;facility_input::controls(state,in,1234,640,360);
    CHECK(state.paused);const auto paused_hash=facility::hash_sim(state);
    facility::sim_step(state,nullptr);CHECK(facility::hash_sim(state)==paused_hash);
    facility_input::controls(state,in,1234,640,360);CHECK(!state.paused);
    facility::sim_step(state,nullptr);CHECK(state.frame==1);
    state.pending_levelups=2;state.level_up_pending=true;state.paused=true;
    in={};in.edge_reset=true;facility_input::controls(state,in,1234,640,360);
    CHECK(!state.paused && !state.level_up_pending && state.pending_levelups==0 && state.frame==0);
    CHECK(state.player.level==1 && state.view_w==640 && state.view_h==360);
    CHECK(state.cam.x==state.player.world_x-320 && state.cam.y==state.player.world_y-180);
    // Two queued choices require two distinct edges; a held/retired edge cannot spend both.
    state.pending_levelups=2;state.level_up_pending=true;facility::generate_choices(state);
    in={};in.edge_1=true;
    CHECK(facility::apply_upgrade(state,facility_input::finish_input_frame(in,true)));
    CHECK(state.pending_levelups==1 && state.level_up_pending);
    CHECK(facility_input::finish_input_frame(in,true)==-1);
    in.edge_2=true;CHECK(facility::apply_upgrade(state,facility_input::finish_input_frame(in,true)));
    CHECK(state.pending_levelups==0 && !state.level_up_pending);
    std::printf("facility interactive upgrade input: %s\n",failures?"FAIL":"PASS");
    return failures?1:0;
}
