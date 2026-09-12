#pragma once

#include "golden/gpu_types.hpp"
#include "golden/memory_image.hpp"

namespace golden {

struct RegisteredResource {
    u32 base = 0;
    u32 size = 0;
    u32 width = 0;
    u32 height = 0;
    std::string name;
};

// Command-authoritative surface view: stride/format from command, bounds from resource.
class SurfaceView {
public:
    SurfaceView() = default;
    SurfaceView(MemoryImage* mem, RegisteredResource res, u32 stride, PixelFormat fmt);

    bool valid() const noexcept { return mem_ != nullptr && res_.width > 0; }
    const RegisteredResource& resource() const noexcept { return res_; }
    u32 stride() const noexcept { return stride_; }
    PixelFormat format() const noexcept { return fmt_; }
    MemoryImage* memory() const noexcept { return mem_; }

    // Bounds: only width/height from resource constrain logical pixels.
    bool in_bounds(i32 x, i32 y) const noexcept;

    ExecResult write_rgba(i32 x, i32 y, Rgba8888 c, bool dither, u32 dither_x,
                          u32 dither_y) const;
    ExecResult read_rgba(i32 x, i32 y, Rgba8888& out) const;
    ExecResult write_raw(i32 x, i32 y, const u8* bytes, u32 bpp) const;
    ExecResult read_raw(i32 x, i32 y, u8* bytes, u32 bpp) const;

    // Does a pixel at (x,y) fit in the registered allocation given stride/fmt?
    bool pixel_fits_allocation(i32 x, i32 y) const noexcept;

private:
    u64 pixel_offset(i32 x, i32 y) const noexcept;

    MemoryImage* mem_ = nullptr;
    RegisteredResource res_{};
    u32 stride_ = 0;
    PixelFormat fmt_ = PixelFormat::RGB565;
};

}  // namespace golden
