#pragma once

#include "golden/gpu_types.hpp"
#include "golden/memory_image.hpp"

namespace golden {

struct SurfaceDesc {
    u32 base = 0;
    u32 stride = 0;  // bytes per row
    u32 width = 0;
    u32 height = 0;
    PixelFormat format = PixelFormat::RGB565;
};

class Surface {
public:
    Surface() = default;
    Surface(MemoryImage* memory, SurfaceDesc desc);

    bool valid() const noexcept { return memory_ != nullptr && desc_.width > 0 && desc_.height > 0; }
    const SurfaceDesc& desc() const noexcept { return desc_; }
    MemoryImage* memory() const noexcept { return memory_; }

    // Stage-001: RGB565 only.
    ExecResult write_pixel(u32 x, u32 y, Rgba8888 color) const;
    ExecResult read_pixel(u32 x, u32 y, Rgba8888& out) const;

    u32 byte_size() const noexcept;

private:
    bool coords_in_bounds(u32 x, u32 y) const noexcept;
    ExecResult write_rgb565(u32 x, u32 y, u16 px) const;
    ExecResult read_rgb565(u32 x, u32 y, u16& px) const;

    MemoryImage* memory_ = nullptr;
    SurfaceDesc desc_{};
};

}  // namespace golden
