#pragma once

#include "golden/gpu_types.hpp"
#include "golden/memory_image.hpp"

namespace golden {

struct SurfaceDesc {
    u32 base = 0;
    u32 stride = 0;
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

    u32 byte_size() const noexcept;

    ExecResult write_pixel(u32 x, u32 y, Rgba8888 color) const;
    ExecResult read_pixel(u32 x, u32 y, Rgba8888& out) const;
    ExecResult write_raw_pixel(u32 x, u32 y, const u8* bytes, u32 bpp) const;
    ExecResult read_raw_pixel(u32 x, u32 y, u8* bytes, u32 bpp) const;

    u64 pixel_offset(u32 x, u32 y) const noexcept;

private:
    bool coords_in_bounds(u32 x, u32 y) const noexcept;

    MemoryImage* memory_ = nullptr;
    SurfaceDesc desc_{};
};

}  // namespace golden
