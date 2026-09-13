#pragma once

#include "golden/gpu_types.hpp"
#include "golden/surface_view.hpp"

#include <vector>

namespace golden {

// Tile-local color buffer: canonical RGBA8888 logical state after every write.
// Compatibility quantization happens when writing back to the RT format.
class TileColorBuffer {
public:
    void reset(u32 width, u32 height);
    u32 width() const noexcept { return w_; }
    u32 height() const noexcept { return h_; }

    Rgba8888 get(u32 x, u32 y) const;
    void set(u32 x, u32 y, Rgba8888 c);

    // Quantize a tile-local pixel to destination format (exact Immediate path).
    static u16 quantize_rgb565(Rgba8888 c, bool dither, u32 gx, u32 gy);
    static void store_rgba_le(Rgba8888 c, PixelFormat fmt, u8* out);

    const std::vector<Rgba8888>& data() const noexcept { return px_; }

private:
    std::vector<Rgba8888> px_;
    u32 w_ = 0;
    u32 h_ = 0;
};

}  // namespace golden
