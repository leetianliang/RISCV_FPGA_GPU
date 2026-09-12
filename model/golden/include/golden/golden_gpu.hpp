#pragma once

#include "golden/command_decoder.hpp"
#include "golden/gpu_isa.hpp"
#include "golden/gpu_math.hpp"
#include "golden/gpu_types.hpp"
#include "golden/memory_image.hpp"
#include "golden/surface_view.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace golden {

struct SurfaceDesc {
    u32 base = 0;
    u32 stride = 0;
    u32 width = 0;
    u32 height = 0;
    PixelFormat format = PixelFormat::RGB565;
};

class GoldenGPU {
public:
    MemoryImage& memory() noexcept { return memory_; }
    const MemoryImage& memory() const noexcept { return memory_; }

    ExecResult register_resource(const RegisteredResource& res);
    ExecResult register_surface(const SurfaceDesc& desc, const std::string& name);

    std::optional<RegisteredResource> resource(u32 base) const;

    ExecResult execute_command(const GpuCmd64& cmd);
    ExecResult execute_stream(const std::vector<GpuCmd64>& cmds);
    void reset();

private:
    struct Sampled {
        ExecResult status{};
        Rgba8888 color{};
    };
    ExecResult execute_draw(const DecodedDraw& d);
    ExecResult sample_and_blend(const DecodedDraw& d, SurfaceView& src_view,
                                SurfaceView& dst_view, i32 dx, i32 dy, i32 sx, i32 sy);
    Sampled sample_color(const DecodedDraw& d, SurfaceView& src_view, i32 sx, i32 sy);

    MemoryImage memory_;
    std::map<u32, RegisteredResource> resources_;
};

// Helper for pixel-center scale coefficients (frozen formula).
inline void compute_axis_aligned_uv(u32 src_x, u32 src_w, u32 dst_w, i32& u0,
                                    i32& du_dx) {
    du_dx = static_cast<i32>(round_div_signed(static_cast<i64>(src_w) * 65536, dst_w));
    u0 = static_cast<i32>((static_cast<i64>(src_x) << 16) +
                          round_div_signed(static_cast<i64>(src_w - dst_w) * 32768,
                                           dst_w));
}

inline void compute_axis_aligned_uv_v(u32 src_y, u32 src_h, u32 dst_h, i32& v0,
                                      i32& dv_dy) {
    dv_dy = static_cast<i32>(round_div_signed(static_cast<i64>(src_h) * 65536, dst_h));
    v0 = static_cast<i32>((static_cast<i64>(src_y) << 16) +
                          round_div_signed(static_cast<i64>(src_h - dst_h) * 32768,
                                           dst_h));
}

}  // namespace golden
