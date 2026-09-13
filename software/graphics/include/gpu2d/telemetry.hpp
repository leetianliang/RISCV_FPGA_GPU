#pragma once

#include "gpu2d/types.hpp"

#include <vector>

namespace gpu2d {

struct TelemetrySnapshot {
    u32 command_count = 0;
    u32 sprite_count = 0;
    u32 workref_count = 0;
    u32 tiles_total = 0;
    u32 tiles_active = 0;
    u32 max_workrefs_per_tile = 0;
    u32 max_overdraw = 0;
    u32 grid_w = 0;
    u32 grid_h = 0;
    u32 tile_size = 0;
    bool has_overdraw = false;
    bool has_workref_map = false;
    std::vector<u32> overdraw;
    std::vector<u16> tile_workrefs;

    RendererTelemetry view() const {
        RendererTelemetry t;
        t.command_count = command_count;
        t.sprite_count = sprite_count;
        t.workref_count = workref_count;
        t.tiles_total = tiles_total;
        t.tiles_active = tiles_active;
        t.max_workrefs_per_tile = max_workrefs_per_tile;
        t.max_overdraw = max_overdraw;
        t.grid_w = grid_w;
        t.grid_h = grid_h;
        t.tile_size = tile_size;
        t.has_overdraw = has_overdraw;
        t.has_workref_map = has_workref_map;
        t.overdraw = has_overdraw && !overdraw.empty() ? overdraw.data() : nullptr;
        t.tile_workrefs =
            has_workref_map && !tile_workrefs.empty() ? tile_workrefs.data() : nullptr;
        return t;
    }
};

}  // namespace gpu2d
