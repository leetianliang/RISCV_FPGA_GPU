#pragma once

#include "golden/gpu_isa.hpp"
#include "golden/gpu_types.hpp"

namespace golden {

struct DecodedHeader {
    CmdHeader header{};
    bool ok = true;
    FaultCode fault = FaultCode::NONE;
    u32 fault_detail = 0;
};

struct DecodedFill {
    CmdHeader header{};
    FillPayload payload{};
    bool ok = true;
    FaultCode fault = FaultCode::NONE;
    u32 fault_detail = 0;
};

DecodedHeader decode_cmd_header(const GpuCmd64& cmd) noexcept;

// Stage-001: decode + validate header and FILL payload for supported profile.
DecodedFill decode_fill_rect(const GpuCmd64& cmd) noexcept;

// Build a valid Stage-001 FILL command (DST_FORMAT=RGB565, BLEND=COPY).
GpuCmd64 make_fill_rect_cmd(u32 dst_base, u32 dst_stride, i32 dst_x, i32 dst_y,
                            u32 dst_w, u32 dst_h, Rgba8888 color) noexcept;

}  // namespace golden
