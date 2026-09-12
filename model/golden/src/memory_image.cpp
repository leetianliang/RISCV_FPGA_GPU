#include "golden/memory_image.hpp"

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
    // Reject half-open ranges extending beyond 2^32.
    const u64 end = static_cast<u64>(base) + static_cast<u64>(byte_size);
    if (end > (1ull << 32)) {
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

const MemoryImage::Region* MemoryImage::resolve(u32 addr, u32 access_size,
                                                MemAccessStatus& status) const {
    status = MemAccessStatus::OK;
    if (access_size == 0) {
        status = MemAccessStatus::BAD_ARGUMENT;
        return nullptr;
    }
    auto it = regions_.upper_bound(addr);
    if (it == regions_.begin()) {
        status = MemAccessStatus::UNMAPPED;
        return nullptr;
    }
    --it;
    const Region& region = it->second;
    const u64 rel = static_cast<u64>(addr) - static_cast<u64>(region.base);
    if (rel >= static_cast<u64>(region.size)) {
        status = MemAccessStatus::UNMAPPED;
        return nullptr;
    }
    if (rel + static_cast<u64>(access_size) > static_cast<u64>(region.size)) {
        status = MemAccessStatus::OUT_OF_RANGE;
        return nullptr;
    }
    return &region;
}

MemoryImage::Region* MemoryImage::resolve(u32 addr, u32 access_size,
                                          MemAccessStatus& status) {
    return const_cast<Region*>(static_cast<const MemoryImage*>(this)->resolve(
        addr, access_size, status));
}

MemAccessResult MemoryImage::read8(u32 addr, u8& out) const {
    MemAccessStatus st = MemAccessStatus::OK;
    const Region* region = resolve(addr, 1, st);
    if (region == nullptr) {
        return MemAccessResult{st};
    }
    out = region->storage[static_cast<std::size_t>(addr - region->base)];
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::read16(u32 addr, u16& out) const {
    MemAccessStatus st = MemAccessStatus::OK;
    const Region* region = resolve(addr, 2, st);
    if (region == nullptr) {
        return MemAccessResult{st};
    }
    const u64 rel = static_cast<u64>(addr) - region->base;
    u8 bytes[2];
    std::memcpy(bytes, region->storage.data() + rel, 2);
    out = static_cast<u16>(static_cast<u16>(bytes[0]) | (static_cast<u16>(bytes[1]) << 8));
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::read32(u32 addr, u32& out) const {
    MemAccessStatus st = MemAccessStatus::OK;
    const Region* region = resolve(addr, 4, st);
    if (region == nullptr) {
        return MemAccessResult{st};
    }
    const u64 rel = static_cast<u64>(addr) - region->base;
    u8 bytes[4];
    std::memcpy(bytes, region->storage.data() + rel, 4);
    out = static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8) |
          (static_cast<u32>(bytes[2]) << 16) | (static_cast<u32>(bytes[3]) << 24);
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::write8(u32 addr, u8 value) {
    MemAccessStatus st = MemAccessStatus::OK;
    Region* region = resolve(addr, 1, st);
    if (region == nullptr) {
        return MemAccessResult{st};
    }
    region->storage[static_cast<std::size_t>(addr - region->base)] = value;
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::write16(u32 addr, u16 value) {
    MemAccessStatus st = MemAccessStatus::OK;
    Region* region = resolve(addr, 2, st);
    if (region == nullptr) {
        return MemAccessResult{st};
    }
    const u64 rel = static_cast<u64>(addr) - region->base;
    const u8 bytes[2] = {static_cast<u8>(value & 0xFFu),
                         static_cast<u8>((value >> 8) & 0xFFu)};
    std::memcpy(region->storage.data() + rel, bytes, 2);
    return MemAccessResult{MemAccessStatus::OK};
}

MemAccessResult MemoryImage::write32(u32 addr, u32 value) {
    MemAccessStatus st = MemAccessStatus::OK;
    Region* region = resolve(addr, 4, st);
    if (region == nullptr) {
        return MemAccessResult{st};
    }
    const u64 rel = static_cast<u64>(addr) - region->base;
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
    MemAccessStatus st = MemAccessStatus::OK;
    const Region* region = resolve(addr, static_cast<u32>(size), st);
    if (region == nullptr) {
        return MemAccessResult{st};
    }
    const u64 rel = static_cast<u64>(addr) - region->base;
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
    MemAccessStatus st = MemAccessStatus::OK;
    Region* region = resolve(addr, static_cast<u32>(size), st);
    if (region == nullptr) {
        return MemAccessResult{st};
    }
    const u64 rel = static_cast<u64>(addr) - region->base;
    std::memcpy(region->storage.data() + rel, data, size);
    return MemAccessResult{MemAccessStatus::OK};
}

const u8* MemoryImage::peek_contiguous(u32 addr, u32 size) const {
    MemAccessStatus st = MemAccessStatus::OK;
    const Region* region = resolve(addr, size, st);
    if (region == nullptr) {
        return nullptr;
    }
    return region->storage.data() + (addr - region->base);
}

u8* MemoryImage::poke_contiguous(u32 addr, u32 size) {
    MemAccessStatus st = MemAccessStatus::OK;
    Region* region = resolve(addr, size, st);
    if (region == nullptr) {
        return nullptr;
    }
    return region->storage.data() + (addr - region->base);
}

void MemoryImage::clear() { regions_.clear(); }

}  // namespace golden
