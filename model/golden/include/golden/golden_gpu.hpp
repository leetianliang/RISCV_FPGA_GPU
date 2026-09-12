#pragma once

#include "golden/command_decoder.hpp"
#include "golden/gpu_isa.hpp"
#include "golden/gpu_types.hpp"
#include "golden/memory_image.hpp"
#include "golden/surface.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace golden {

class GoldenGPU {
public:
    MemoryImage& memory() noexcept { return memory_; }
    const MemoryImage& memory() const noexcept { return memory_; }

    ExecResult register_surface(const SurfaceDesc& desc, const std::string& name);

    std::optional<SurfaceDesc> surface_desc(u32 base) const;

    ExecResult execute_command(const GpuCmd64& cmd);
    ExecResult execute_stream(const std::vector<GpuCmd64>& cmds);

    void reset();

private:
    ExecResult execute_draw(const DecodedDraw& decoded);
    ExecResult write_dst_pixel(const Surface& dst, u32 x, u32 y, Rgba8888 src,
                               const Draw2DState& st);

    MemoryImage memory_;
    std::map<u32, SurfaceDesc> surfaces_;
};

}  // namespace golden
