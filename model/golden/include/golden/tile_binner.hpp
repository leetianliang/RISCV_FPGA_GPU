#pragma once

#include "golden/command_decoder.hpp"
#include "golden/golden_gpu.hpp"
#include "golden/tile_types.hpp"

namespace golden {

// CPU software binner: ordered draws → descriptor array + tile headers + workrefs.
// Uses the approved raster rule: dest ∩ optional clip ∩ target bounds.
TileBinResult bin_draws(const std::vector<GpuCmd64>& draws, const MemoryImage& mem,
                        u32 surface_w, u32 surface_h, u32 tile_size);

// Serialize WorkRef array as LE u32s.
std::vector<u8> serialize_workrefs(const std::vector<WorkRef>& refs);

// Execute TILE_FRAME through GoldenGPU memory model (shared pixel backend).
ExecResult execute_tile_frame(GoldenGPU& gpu, const GpuCmd64& tile_cmd,
                              u32 desc_count_hint);

struct TileStats {
    u32 tiles_total = 0;
    u32 tiles_active = 0;
    u32 draw_descriptor_count = 0;
    u32 workref_count = 0;
    u32 max_workrefs_per_tile = 0;
    u32 sum_workrefs = 0;
    u32 tile_load_bytes = 0;
    u32 tile_store_bytes = 0;
};

TileStats last_tile_stats();

}  // namespace golden
