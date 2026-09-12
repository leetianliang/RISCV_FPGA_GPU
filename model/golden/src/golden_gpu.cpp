#include "golden/golden_gpu.hpp"

namespace golden {

ExecResult GoldenGPU::register_surface(const SurfaceDesc& desc, const std::string& name) {
    if (desc.width == 0 || desc.height == 0 || desc.stride == 0) {
        return ExecResult::failure(FaultCode::BAD_RECT);
    }
    if (desc.format != PixelFormat::RGB565) {
        return ExecResult::failure(FaultCode::BAD_FORMAT,
                                   static_cast<u32>(desc.format));
    }
    const u64 total = static_cast<u64>(desc.stride) * static_cast<u64>(desc.height);
    if (total == 0 || total > 0xFFFFFFFFull) {
        return ExecResult::failure(FaultCode::BAD_RECT);
    }

    if (surfaces_.find(desc.base) != surfaces_.end()) {
        return ExecResult::failure(FaultCode::BAD_ALIGNMENT, desc.base);
    }

    if (!memory_.has_region(desc.base)) {
        const auto st = memory_.register_region(name, desc.base, static_cast<u32>(total));
        if (st.status != MemAccessStatus::OK) {
            return ExecResult::failure(FaultCode::MEMORY, static_cast<u32>(st.status));
        }
    }

    surfaces_.emplace(desc.base, desc);
    return ExecResult::success();
}

std::optional<SurfaceDesc> GoldenGPU::surface_desc(u32 base) const {
    const auto it = surfaces_.find(base);
    if (it == surfaces_.end()) {
        return std::nullopt;
    }
    return it->second;
}

ExecResult GoldenGPU::execute_command(const GpuCmd64& cmd) {
    const DecodedHeader hdr = decode_cmd_header(cmd);
    if (!hdr.ok) {
        return ExecResult::failure(hdr.fault, hdr.fault_detail);
    }
    if (hdr.header.opcode != kOpcodeFillRect) {
        return ExecResult::failure(FaultCode::UNSUPPORTED_FEATURE, hdr.header.opcode);
    }

    const DecodedFill decoded = decode_fill_rect(cmd);
    if (!decoded.ok) {
        return ExecResult::failure(decoded.fault, decoded.fault_detail);
    }
    return execute_fill(decoded);
}

ExecResult GoldenGPU::execute_fill(const DecodedFill& decoded) {
    const auto it = surfaces_.find(decoded.payload.dst_base);
    if (it == surfaces_.end()) {
        // Surface metadata not registered in harness.
        return ExecResult::failure(FaultCode::MEMORY, decoded.payload.dst_base);
    }

    const SurfaceDesc& desc = it->second;
    const Surface surface(&memory_, desc);

    const u32 w = decoded.payload.dst_w;
    const u32 h = decoded.payload.dst_h;
    if (w == 0 || h == 0) {
        return ExecResult::success();  // legal no-op
    }

    const i64 x0 = decoded.payload.dst_x;
    const i64 y0 = decoded.payload.dst_y;
    const i64 x1 = x0 + static_cast<i64>(w);
    const i64 y1 = y0 + static_cast<i64>(h);

    if (x0 < 0 || y0 < 0 || x1 > static_cast<i64>(desc.width) ||
        y1 > static_cast<i64>(desc.height)) {
        return ExecResult::failure(FaultCode::BAD_RECT, decoded.payload.draw_state);
    }

    for (i64 y = y0; y < y1; ++y) {
        for (i64 x = x0; x < x1; ++x) {
            const ExecResult st = surface.write_pixel(static_cast<u32>(x),
                                                      static_cast<u32>(y),
                                                      decoded.payload.primary_color);
            if (!st.ok) {
                return st;
            }
        }
    }
    return ExecResult::success();
}

void GoldenGPU::reset() {
    memory_.clear();
    surfaces_.clear();
}

}  // namespace golden
