#include "golden/surface.hpp"

#include "golden/gpu_math.hpp"

#include <vector>

namespace golden {

Surface::Surface(MemoryImage* memory, SurfaceDesc desc) : memory_(memory), desc_(desc) {}

u32 Surface::byte_size() const noexcept {
    if (desc_.height == 0) {
        return 0;
    }
    const u64 total = static_cast<u64>(desc_.stride) * static_cast<u64>(desc_.height);
    if (total > 0xFFFFFFFFull) {
        return 0;
    }
    return static_cast<u32>(total);
}

bool Surface::coords_in_bounds(u32 x, u32 y) const noexcept {
    return x < desc_.width && y < desc_.height;
}

u64 Surface::pixel_offset(u32 x, u32 y) const noexcept {
    const u32 bpp = bytes_per_pixel(desc_.format);
    return static_cast<u64>(desc_.base) + static_cast<u64>(y) * desc_.stride +
           static_cast<u64>(x) * bpp;
}

ExecResult Surface::write_raw_pixel(u32 x, u32 y, const u8* bytes, u32 bpp) const {
    if (memory_ == nullptr || bytes == nullptr) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR);
    }
    if (bytes_per_pixel(desc_.format) != bpp) {
        return ExecResult::failure(FaultCode::BAD_FORMAT,
                                   static_cast<u32>(desc_.format));
    }
    if (!coords_in_bounds(x, y)) {
        return ExecResult::failure(FaultCode::BAD_RECT, (y << 16) | (x & 0xFFFFu));
    }
    const u64 off = pixel_offset(x, y);
    if (off > 0xFFFFFFFFull) {
        return ExecResult::failure(FaultCode::BAD_ADDRESS);
    }
    const auto st = memory_->write_block(static_cast<u32>(off), bytes, bpp);
    if (st.status != MemAccessStatus::OK) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, static_cast<u32>(st.status));
    }
    return ExecResult::success();
}

ExecResult Surface::read_raw_pixel(u32 x, u32 y, u8* bytes, u32 bpp) const {
    if (memory_ == nullptr || bytes == nullptr) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR);
    }
    if (bytes_per_pixel(desc_.format) != bpp) {
        return ExecResult::failure(FaultCode::BAD_FORMAT,
                                   static_cast<u32>(desc_.format));
    }
    if (!coords_in_bounds(x, y)) {
        return ExecResult::failure(FaultCode::BAD_RECT, (y << 16) | (x & 0xFFFFu));
    }
    const u64 off = pixel_offset(x, y);
    if (off > 0xFFFFFFFFull) {
        return ExecResult::failure(FaultCode::BAD_ADDRESS);
    }
    std::vector<u8> tmp;
    const auto st = memory_->read_block(static_cast<u32>(off), bpp, tmp);
    if (st.status != MemAccessStatus::OK) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, static_cast<u32>(st.status));
    }
    for (u32 i = 0; i < bpp; ++i) {
        bytes[i] = tmp[i];
    }
    return ExecResult::success();
}

ExecResult Surface::write_pixel(u32 x, u32 y, Rgba8888 color) const {
    if (desc_.format == PixelFormat::RGB565) {
        const u16 px = rgb565_encode(color);
        const u8 bytes[2] = {static_cast<u8>(px & 0xFFu),
                             static_cast<u8>((px >> 8) & 0xFFu)};
        return write_raw_pixel(x, y, bytes, 2);
    }
    if (desc_.format == PixelFormat::ARGB8888) {
        const u8 bytes[4] = {color.b(), color.g(), color.r(), color.a()};
        return write_raw_pixel(x, y, bytes, 4);
    }
    if (desc_.format == PixelFormat::XRGB8888) {
        const u8 bytes[4] = {color.b(), color.g(), color.r(), 0xFFu};
        return write_raw_pixel(x, y, bytes, 4);
    }
    return ExecResult::failure(FaultCode::BAD_FORMAT, static_cast<u32>(desc_.format));
}

ExecResult Surface::read_pixel(u32 x, u32 y, Rgba8888& out) const {
    const u32 bpp = bytes_per_pixel(desc_.format);
    if (bpp == 0 || desc_.format == PixelFormat::INDEX8) {
        return ExecResult::failure(FaultCode::BAD_FORMAT, static_cast<u32>(desc_.format));
    }
    u8 bytes[4] = {0, 0, 0, 0};
    const auto st = read_raw_pixel(x, y, bytes, bpp);
    if (!st.ok) {
        return st;
    }
    out = decode_source_pixel(desc_.format, bytes);
    return ExecResult::success();
}

}  // namespace golden
