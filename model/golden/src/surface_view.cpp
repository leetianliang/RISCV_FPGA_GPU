#include "golden/surface_view.hpp"

#include "golden/gpu_math.hpp"

#include <vector>

namespace golden {

SurfaceView::SurfaceView(MemoryImage* mem, RegisteredResource res, u32 stride,
                         PixelFormat fmt)
    : mem_(mem), res_(std::move(res)), stride_(stride), fmt_(fmt) {}

bool SurfaceView::in_bounds(i32 x, i32 y) const noexcept {
    return x >= 0 && y >= 0 && static_cast<u32>(x) < res_.width &&
           static_cast<u32>(y) < res_.height;
}

bool SurfaceView::pixel_fits_allocation(i32 x, i32 y) const noexcept {
    const u32 bpp = bytes_per_pixel(fmt_);
    if (bpp == 0 || !mem_) {
        return false;
    }
    const u64 off = pixel_offset(x, y);
    if (off > 0xFFFFFFFFull) {
        return false;
    }
    MemAccessStatus st = MemAccessStatus::OK;
    // 1-byte resolve check via peek size
    const u8* p = mem_->peek_contiguous(static_cast<u32>(off), bpp);
    return p != nullptr;
}

u64 SurfaceView::pixel_offset(i32 x, i32 y) const noexcept {
    const u32 bpp = bytes_per_pixel(fmt_);
    return static_cast<u64>(res_.base) + static_cast<u64>(y) * stride_ +
           static_cast<u64>(x) * bpp;
}

ExecResult SurfaceView::write_raw(i32 x, i32 y, const u8* bytes, u32 bpp) const {
    if (!valid() || bytes == nullptr) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR);
    }
    if (bytes_per_pixel(fmt_) != bpp) {
        return ExecResult::failure(FaultCode::BAD_FORMAT, static_cast<u32>(fmt_));
    }
    if (!in_bounds(x, y)) {
        return ExecResult::failure(FaultCode::BAD_RECT);
    }
    const u64 off = pixel_offset(x, y);
    if (off > 0xFFFFFFFFull) {
        return ExecResult::failure(FaultCode::BAD_ADDRESS);
    }
    const auto st = mem_->write_block(static_cast<u32>(off), bytes, bpp);
    if (st.status != MemAccessStatus::OK) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, static_cast<u32>(st.status));
    }
    return ExecResult::success();
}

ExecResult SurfaceView::read_raw(i32 x, i32 y, u8* bytes, u32 bpp) const {
    if (!valid() || bytes == nullptr) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR);
    }
    if (bytes_per_pixel(fmt_) != bpp) {
        return ExecResult::failure(FaultCode::BAD_FORMAT, static_cast<u32>(fmt_));
    }
    if (!in_bounds(x, y)) {
        return ExecResult::failure(FaultCode::BAD_RECT);
    }
    const u64 off = pixel_offset(x, y);
    if (off > 0xFFFFFFFFull) {
        return ExecResult::failure(FaultCode::BAD_ADDRESS);
    }
    std::vector<u8> tmp;
    const auto st = mem_->read_block(static_cast<u32>(off), bpp, tmp);
    if (st.status != MemAccessStatus::OK) {
        return ExecResult::failure(FaultCode::MEMORY_ERROR, static_cast<u32>(st.status));
    }
    for (u32 i = 0; i < bpp; ++i) {
        bytes[i] = tmp[i];
    }
    return ExecResult::success();
}

ExecResult SurfaceView::write_rgba(i32 x, i32 y, Rgba8888 c, bool dither, u32 dx,
                                   u32 dy) const {
    if (fmt_ == PixelFormat::RGB565) {
        u16 px = dither ? rgb565_encode_dither(c, dx, dy) : rgb565_encode(c);
        const u8 bytes[2] = {static_cast<u8>(px & 0xFFu),
                             static_cast<u8>((px >> 8) & 0xFFu)};
        return write_raw(x, y, bytes, 2);
    }
    if (fmt_ == PixelFormat::ARGB8888) {
        const u8 bytes[4] = {c.b(), c.g(), c.r(), c.a()};
        return write_raw(x, y, bytes, 4);
    }
    if (fmt_ == PixelFormat::XRGB8888) {
        const u8 bytes[4] = {c.b(), c.g(), c.r(), 0xFFu};
        return write_raw(x, y, bytes, 4);
    }
    return ExecResult::failure(FaultCode::BAD_FORMAT, static_cast<u32>(fmt_));
}

ExecResult SurfaceView::read_rgba(i32 x, i32 y, Rgba8888& out) const {
    const u32 bpp = bytes_per_pixel(fmt_);
    if (bpp == 0 || fmt_ == PixelFormat::INDEX8) {
        return ExecResult::failure(FaultCode::BAD_FORMAT, static_cast<u32>(fmt_));
    }
    u8 bytes[4] = {0, 0, 0, 0};
    const auto st = read_raw(x, y, bytes, bpp);
    if (!st.ok) {
        return st;
    }
    out = decode_source_pixel(fmt_, bytes);
    return ExecResult::success();
}

}  // namespace golden
