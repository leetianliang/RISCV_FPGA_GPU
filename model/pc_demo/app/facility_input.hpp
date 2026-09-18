#pragma once
#include "../host/presenter.hpp"
#include "facility/app.hpp"

namespace facility_input {
inline void controls(facility::AppState& state,const host::InputState& input,
                     facility::u32 seed,facility::u32 width,facility::u32 height) {
    if(input.edge_reset) {
        facility::sim_reset(state,seed);
        facility::set_viewport(state,width,height);
        facility::camera_follow(state,width,height);
    } else if(input.edge_pause) state.paused=!state.paused;
}
// Resolve the current game's choice BEFORE retiring this frame's input edges.
// Menu transitions consume their own edges and never enter this path.
inline int finish_input_frame(host::InputState& input, bool upgrade_pending) {
    const int choice = !upgrade_pending ? -1 : input.edge_1 ? 0 : input.edge_2 ? 1 : input.edge_3 ? 2 : -1;
    input.clear_edges();
    return choice;
}
} // namespace facility_input
