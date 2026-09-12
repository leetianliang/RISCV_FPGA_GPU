#include "golden/memory_image.hpp"

#include <algorithm>
#include <cstring>

namespace golden {

namespace {

bool ranges_overlap(u32 a_base, u64 a_size, u32 b_base, u64 b_size) {
    if (a_size == 0 || b_size == 0) {
        return false;
    }
    const u64 a_end = static_cast<u64>(a_base) + a_size;
    const u64 b_end = static_cast<u64>(b_base) + b_size;
    return static_cast<u64>(a_base) < b_end && static_cast<u64>(b_base) < a_end;
}

}  // namespace

MemAccessResult MemoryImage::register_region(std::string name, u32 base, u32 byte_size) {
    if (byte_size == 0 || name.empty()) {
        return MemAccessResult{MemAccessStatus::BAD_ARGUMENT};
    }

    for (const auto& [reg_base, region] : regions_) {
        if (ranges_overlap(base, byte_size, reg_base, region.size)) {
            return MemAccessResult{MemAccessStatus::OVERLAP};
        }
    }

    Region region;
    region.name = std::move(name);
    region.base = base;
    region.size = byte_size;
    region.storage.assign(byte_size, 0u);
    regions_.emplace(base, std::move(region));
    return MemAccessResult{MemAccessStatus::OK};
}

bool MemoryImage::has_region(u32 base) const {
    return regions_.find(base) != regions_.end();
}

const MemoryImage::Region* MemoryImage::find_region(u32 addr, u32 access_size) const {
    if (access_size == 0) {
        return nullptr;
    }
    // Find last region with base <= addr
    auto it = regions_.upper_bound(addr);
    if (it == regions_.begin()) {
        return nullptr;
    }
    --it;
    const Region& region = it->second;
    const u64 rel = static_cast<u64>(addr) - static_cast<u64>(region.base);
    const u64 end = rel + static_cast<u64>(access_size);
    if (end > static_cast<u64>(region.size)) {
        return nullptr;
    }
    return &region;
}

MemoryImage::Region* MemoryImage::find_region(u32 addr, u32 access_size) {
    return const_cast<Region*>(static_cast<const MemoryImage*>(this)->find_region(addr, access_size));
}

MemAccessResult MemoryImage::read8(u32 addr, u8& out) const {
    const Region* region = find_region(addr, 1);
    if (region == nullptr) {
        return MemAccessResult{MemAccessStatus::UNMAPPED};
    }
    const u64 rel = static_cast<u64>(addr) - static_cast<u64>(region->base);
    out = region->storage[static_cast<std::size_t>(rel)];
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::read16(u32 addr, u16& out) const {
    const Region* region = find_region(addr, 2);
    if (region == nullptr) {
        return MemAccessResult{MemAccessStatus::UNMAPPED};
    }
    const u64 rel = static_cast<u64>(addr) - static_cast<u64>(region->base);
    u8 bytes[2];
    std::memcpy(bytes, region->storage.data() + rel, 2);
    out = static_cast<u16>(static_cast<u16>(bytes[0]) | (static_cast<u16>(bytes[1]) << 8));
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::read32(u32 addr, u32& out) const {
    const Region* region = find_region(addr, 4);
    if (region == nullptr) {
        return MemAccessResult{MemAccessStatus::UNMAPPED};
    }
    const u64 rel = static_cast<u64>(addr) - static_cast<u64>(region->base);
    u8 bytes[4];
    std::memcpy(bytes, region->storage.data() + rel, 4);
    out = static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8) |
          (static_cast<u32>(bytes[2]) << 16) | (static_cast<u32>(bytes[3]) << 24);
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::write8(u32 addr, u8 value) {
    Region* region = find_region(addr, 1);
    if (region == nullptr) {
        return MemAccessResult{MemAccessStatus::UNMAPPED};
    }
    const u64 rel = static_cast<u64>(addr) - static_cast<u64>(region->base);
    region->storage[static_cast<std::size_t>(rel)] = value;
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::write16(u32 addr, u16 value) {
    Region* region = find_region(addr, 2);
    if (region == nullptr) {
        return MemAccessResult{MemAccessStatus::UNMAPPED};
    }
    const u64 rel = static_cast<u64>(addr) - static_cast<u64>(region->base);
    const u8 bytes[2] = {static_cast<u8>(value & 0xFFu),
                         static_cast<u8>((value >> 8) & 0xFFu)};
    std::memcpy(region->storage.data() + rel, bytes, 2);
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::write32(u32 addr, u32 value) {
    Region* region = find_region(addr, 4);
    if (region == nullptr) {
        return MemAccessResult{MemAccessStatus::UNMAPPED};
    }
    const u64 rel = static_cast<u64>(addr) - static_cast<u64>(region->base);
    const u8 bytes[4] = {
        static_cast<u8>(value & 0xFFu), static_cast<u8>((value >> 8) & 0xFFu),
        static_cast<u8>((value >> 16) & 0xFFu), static_cast<u8>((value >> 24) & 0xFFu)};
    std::memcpy(region->storage.data() + rel, bytes, 4);
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::read_block(u32 addr, std::size_t size, std::vector<u8>& out) const {
    if (size == 0) {
        out.clear();
        return MemAccessResult{MemAccessStatus::OK};
    }
    if (size > 0xFFFFFFFFu) {
        return MemAccessResult{MemAccessStatus::BAD_ARGUMENT};
    }
    const Region* region = find_region(addr, static_cast<u32>(size));
    if (region == nullptr) {
        return MemAccessResult{MemAccessStatus::UNMAPPED};
    }
    const u64 rel = static_cast<u64>(addr) - static_cast<u64>(region->base);
    out.assign(region->storage.begin() + static_cast<std::ptrdiff_t>(rel),
               region->storage.begin() + static_cast<std::ptrdiff_t>(rel + size));
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::write_block(u32 addr, const u8* data, std::size_t size) {
    if (size == 0) {
        return MemAccessResult{MemAccessStatus::OK};
    }
    if (data == nullptr || size > 0xFFFFFFFFu) {
        return MemAccessResult{MemAccessStatus::BAD_ARGUMENT};
    }
    Region* region = find_region(addr, static_cast<u32>(size));
    if (region == nullptr) {
        return MemAccessResult{MemAccessStatus::UNMAPPED};
    }
    const u64 rel = static_cast<u64>(addr) - static_cast<u64>(region->base);
    std::memcpy(region->storage.data() + rel, data, size);
    return MemAccessResult{MemAccessStatus::OK};
}

void MemoryImage::clear() {
    regions_.clear();
}

}  // namespace golden
