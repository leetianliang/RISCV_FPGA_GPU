#pragma once

#include "golden/gpu_types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace golden {

enum class MemAccessStatus : u32 {
    OK = 0,
    UNMAPPED = 1,
    OUT_OF_RANGE = 2,
    OVERLAP = 3,
    BAD_ARGUMENT = 4,
};

struct MemAccessResult {
    MemAccessStatus status = MemAccessStatus::OK;
};

// Region-based 32-bit physical memory model. Does not allocate 4 GiB.
class MemoryImage {
public:
    MemAccessResult register_region(std::string name, u32 base, u32 byte_size);

    bool has_region(u32 base) const;

    MemAccessResult read8(u32 addr, u8& out) const;
    MemAccessResult read16(u32 addr, u16& out) const;
    MemAccessResult read32(u32 addr, u32& out) const;

    MemAccessResult write8(u32 addr, u8 value);
    MemAccessResult write16(u32 addr, u16 value);
    MemAccessResult write32(u32 addr, u32 value);

    MemAccessResult read_block(u32 addr, std::size_t size, std::vector<u8>& out) const;
    MemAccessResult write_block(u32 addr, const u8* data, std::size_t size);

    void clear();

private:
    struct Region {
        std::string name;
        u32 base = 0;
        u32 size = 0;
        std::vector<u8> storage;
    };

    const Region* find_region(u32 addr, u32 access_size) const;
    Region* find_region(u32 addr, u32 access_size);

    std::map<u32, Region> regions_;
};

}  // namespace golden
