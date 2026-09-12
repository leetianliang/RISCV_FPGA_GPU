#pragma once

#include "golden/command_decoder.hpp"
#include "golden/gpu_isa.hpp"
#include "golden/gpu_types.hpp"
#include "golden/memory_image.hpp"
#include "golden/surface.hpp"

#include <map>
#include <optional>
#include <string>

namespace golden {

// Minimal Stage-001 facade. Surfaces are harness-registered (width/height are
// not encoded in the binary command).
class GoldenGPU {
public:
    MemoryImage& memory() noexcept { return memory_; }
    const MemoryImage& memory() const noexcept { return memory_; }

    // Registers a surface descriptor and ensures a backing memory region exists.
    ExecResult register_surface(const SurfaceDesc& desc, const std::string& name);

    std::optional<SurfaceDesc> surface_desc(u32 base) const;

    ExecResult execute_command(const GpuCmd64& cmd);

    void reset();

private:
    ExecResult execute_fill(const DecodedFill& decoded);

    MemoryImage memory_;
    std::map<u32, SurfaceDesc> surfaces_;
};

}  // namespace golden
