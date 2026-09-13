#pragma once

#include "gpu2d/types.hpp"

#include <string>
#include <vector>

namespace gpu2d {

struct TextureDesc {
    u32 width = 0;
    u32 height = 0;
    PixelFormat format = PixelFormat::RGB565;
    u32 stride = 0;  // 0 → packed
    const void* pixels = nullptr;
    u32 palette_entries = 0;  // 256 when INDEX8
    const u32* palette = nullptr;  // RGBA8888
};

class ResourceManager {
public:
    virtual ~ResourceManager() = default;
    virtual TextureId create_texture(const TextureDesc& desc) = 0;
    virtual bool texture_valid(TextureId id) const = 0;
    virtual u32 texture_width(TextureId id) const = 0;
    virtual u32 texture_height(TextureId id) const = 0;
};

}  // namespace gpu2d
