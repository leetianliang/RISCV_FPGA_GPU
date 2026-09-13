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
// Descriptor capacity comes from the registered resource at draw_desc_base.
ExecResult execute_tile_frame(GoldenGPU& gpu, const GpuCmd64& tile_cmd);

// Convenience: default RT_STATE for tests (format + STORE_COLOR).
inline u32 tile_rt_state_store(PixelFormat fmt, bool strict = false) {
    u32 rt = static_cast<u32>(fmt) | kRtStoreColor;
    if (strict) {
        rt |= kRtStrictTargetMatch;
    }
    return rt;
}

struct TileStats {
    u32 tiles_total = 0;
    u32 tiles_active = 0;
    u32 draw_descriptor_count = 0;
    u32 workref_count = 0;
    u32 max_workrefs_per_tile = 0;
    u32 sum_workrefs = 0;
    u32 tile_load_pixels = 0;
    u32 tile_store_pixels = 0;
    u32 tile_load_bytes = 0;
    u32 tile_store_bytes = 0;
    u64 blend_ops = 0;
    u64 pixels_attempted = 0;
    u64 pixels_written = 0;
    u64 key_discards = 0;
    u64 clip_rejects = 0;
    u64 texture_samples = 0;
    u64 palette_reads = 0;
    u64 bilinear_samples = 0;
    u32 max_overdraw = 0;
    double avg_overdraw_touched = 0.0;
    std::vector<u32> overdraw_matrix;  // surface_h * surface_w, row-major
    u32 overdraw_w = 0;
    u32 overdraw_h = 0;
    u64 logical_pixel_writes = 0;  // alias pixels_written
    u64 estimated_external_load_bytes = 0;
    u64 estimated_external_store_bytes = 0;
};

// Average workrefs per active tile.
inline double avg_workrefs_per_active_tile(const TileStats& s) {
    return s.tiles_active ? static_cast<double>(s.workref_count) / s.tiles_active : 0.0;
}

TileStats last_tile_stats();

}  // namespace golden
