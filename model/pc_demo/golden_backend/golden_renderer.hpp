#pragma once

#include "gpu2d/graphics_api.hpp"
#include "gpu2d/renderer.hpp"
#include "gpu2d/resource.hpp"
#include "gpu2d/telemetry.hpp"
#include "gpu2d/types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace gpu2d {

// Owns GoldenGPU and executes the same recorded command stream either
// as Immediate draws or via the Stage-004 software Tile path.
class GoldenBackend {
public:
    GoldenBackend();
    ~GoldenBackend();

    GoldenBackend(const GoldenBackend&) = delete;
    GoldenBackend& operator=(const GoldenBackend&) = delete;

    bool init(const ProfileDesc& profile);
    void shutdown();

    TextureId create_texture(const TextureDesc& desc);
    bool texture_valid(TextureId id) const;

    BackendKind backend() const noexcept { return backend_; }
    void set_backend(BackendKind kind);

    // Execute a recorded frame. Returns false on Golden fault.
    bool execute_frame(const std::vector<RecCommand>& cmds);

    const u8* framebuffer() const;
    u32 fb_width() const noexcept { return profile_.width; }
    u32 fb_height() const noexcept { return profile_.height; }
    u32 fb_stride() const noexcept { return fb_stride_; }
    PixelFormat fb_format() const noexcept { return profile_.format; }

    const TelemetrySnapshot& telemetry() const noexcept { return tel_; }

    // Debug: last fault code from Golden.
    u32 last_fault() const noexcept { return last_fault_; }
    u32 last_fault_index() const noexcept { return last_fault_index_; }

private:
    struct TexSlot {
        u32 base = 0;
        u32 width = 0;
        u32 height = 0;
        u32 stride = 0;
        PixelFormat format = PixelFormat::RGB565;
        u32 palette_base = 0;
        bool used = false;
    };

    bool ensure_gpu();
    bool upload_texture_bytes(u32 base, u32 stride, u32 h, const void* pixels, size_t nbytes);
    bool run_immediate(const std::vector<RecCommand>& cmds);
    bool run_tile(const std::vector<RecCommand>& cmds);
    void reset_tile_scratch();

    ProfileDesc profile_{};
    u32 fb_stride_ = 0;
    u32 fb_base_ = 0x00010000;
    u32 tex_arena_base_ = 0x00200000;
    u32 tex_arena_cursor_ = 0;
    u32 tex_arena_limit_ = 0x00700000;
    u32 desc_base_ = 0x00800000;
    u32 hdr_base_ = 0x00900000;
    u32 work_base_ = 0x00A00000;
    u32 ext_arena_base_ = 0x00B00000;
    u32 ext_cursor_ = 0;

    BackendKind backend_ = BackendKind::Immediate;
    u32 tile_size_ = 32;
    bool inited_ = false;
    u32 last_fault_ = 0;
    u32 last_fault_index_ = 0;

    std::vector<TexSlot> texs_;
    TelemetrySnapshot tel_;
    std::vector<u16> workref_map_;

    // Opaque GoldenGPU lives in .cpp to keep headers free of Golden types.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace gpu2d
