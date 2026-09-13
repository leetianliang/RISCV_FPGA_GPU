#include "golden/tile_buffer.hpp"

#include "golden/gpu_math.hpp"

namespace golden {

void TileColorBuffer::reset(u32 width, u32 height) {
    w_ = width;
    h_ = height;
    px_.assign(static_cast<size_t>(width) * height, Rgba8888{});
}

Rgba8888 TileColorBuffer::get(u32 x, u32 y) const {
    if (x >= w_ || y >= h_) {
        return Rgba8888{};
    }
    return px_[static_cast<size_t>(y) * w_ + x];
}

void TileColorBuffer::set(u32 x, u32 y, Rgba8888 c) {
    if (x >= w_ || y >= h_) {
        return;
    }
    px_[static_cast<size_t>(y) * w_ + x] = c;
}

u16 TileColorBuffer::quantize_rgb565(Rgba8888 c, bool dither, u32 gx, u32 gy) {
    return dither ? rgb565_encode_dither(c, gx, gy) : rgb565_encode(c);
}

void TileColorBuffer::store_rgba_le(Rgba8888 c, PixelFormat fmt, u8* out) {
    switch (fmt) {
        case PixelFormat::RGB565: {
            const u16 px = rgb565_encode(c);
            out[0] = static_cast<u8>(px & 0xFF);
            out[1] = static_cast<u8>(px >> 8);
            break;
        }
        case PixelFormat::ARGB8888:
            out[0] = c.b();
            out[1] = c.g();
            out[2] = c.r();
            out[3] = c.a();
            break;
        case PixelFormat::XRGB8888:
            out[0] = c.b();
            out[1] = c.g();
            out[2] = c.r();
            out[3] = 0xFF;
            break;
        default:
            break;
    }
}

}  // namespace golden
