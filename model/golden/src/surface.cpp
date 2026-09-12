#include "golden/surface.hpp"

#include "golden/gpu_math.hpp"

namespace golden {

Surface::Surface(MemoryImage* memory, SurfaceDesc desc)
    : memory_(memory), desc_(desc) {}

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

ExecResult Surface::write_rgb565(u32 x, u32 y, u16 px) const {
    if (memory_ == nullptr) {
        return ExecResult::failure(FaultCode::MEMORY);
    }
    if (!coords_in_bounds(x, y)) {
        return ExecResult::failure(FaultCode::BAD_RECT, (y << 16) | (x & 0xFFFFu));
    }
    const u64 offset = static_cast<u64>(desc_.base) +
                       static_cast<u64>(y) * static_cast<u64>(desc_.stride) +
                       static_cast<u64>(x) * 2ull;
    if (offset > 0xFFFFFFFFull) {
        return ExecResult::failure(FaultCode::MEMORY);
    }
    const auto st = memory_->write16(static_cast<u32>(offset), px);
    if (st.status != MemAccessStatus::OK) {
        return ExecResult::failure(FaultCode::MEMORY, static_cast<u32>(st.status));
    }
    return ExecResult::success();
}

ExecResult Surface::read_rgb565(u32 x, u32 y, u16& px) const {
    if (memory_ == nullptr) {
        return ExecResult::failure(FaultCode::MEMORY);
    }
    if (!coords_in_bounds(x, y)) {
        return ExecResult::failure(FaultCode::BAD_RECT, (y << 16) | (x & 0xFFFFu));
    }
    const u64 offset = static_cast<u64>(desc_.base) +
                       static_cast<u64>(y) * static_cast<u64>(desc_.stride) +
                       static_cast<u64>(x) * 2ull;
    if (offset > 0xFFFFFFFFull) {
        return ExecResult::failure(FaultCode::MEMORY);
    }
    const auto st = memory_->read16(static_cast<u32>(offset), px);
    if (st.status != MemAccessStatus::OK) {
        return ExecResult::failure(FaultCode::MEMORY, static_cast<u32>(st.status));
    }
    return ExecResult::success();
}

ExecResult Surface::write_pixel(u32 x, u32 y, Rgba8888 color) const {
    if (desc_.format != PixelFormat::RGB565) {
        return ExecResult::failure(FaultCode::BAD_FORMAT,
                                   static_cast<u32>(desc_.format));
    }
    return write_rgb565(x, y, rgb565_encode(color));
}

ExecResult Surface::read_pixel(u32 x, u32 y, Rgba8888& out) const {
    if (desc_.format != PixelFormat::RGB565) {
        return ExecResult::failure(FaultCode::BAD_FORMAT,
                                   static_cast<u32>(desc_.format));
    }
    u16 px = 0;
    const auto st = read_rgb565(x, y, px);
    if (!st.ok) {
        return st;
    }
    out = rgb565_decode(px);
    return ExecResult::success();
}

}  // namespace golden
