#pragma once

#include <cstdint>

namespace generated {

enum class RegisterAccess {
    RO,
    RW,
    WO,
    W1C
};

struct RegisterMetadata {
    const char* name;
    uint32_t offset;
    uint32_t reset;
    uint32_t mask;
    RegisterAccess access;
};

inline const RegisterMetadata* lookup_register(
    const RegisterMetadata* registers,
    unsigned int count,
    uint32_t offset) {
    for (unsigned int i = 0; i < count; ++i) {
        if (registers[i].offset == offset)
            return &registers[i];
    }
    return nullptr;
}

inline bool register_read_allowed(const RegisterMetadata& reg) {
    return reg.access == RegisterAccess::RO ||
           reg.access == RegisterAccess::RW ||
           reg.access == RegisterAccess::W1C;
}

inline bool register_write_allowed(const RegisterMetadata& reg) {
    return reg.access == RegisterAccess::RW ||
           reg.access == RegisterAccess::WO ||
           reg.access == RegisterAccess::W1C;
}

inline uint32_t apply_register_mask(
    const RegisterMetadata& reg, uint32_t value) {
    return value & reg.mask;
}

inline uint32_t load_register_value(const unsigned char* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

inline void store_register_value(unsigned char* data, uint32_t value) {
    data[0] = value & 0xff;
    data[1] = (value >> 8) & 0xff;
    data[2] = (value >> 16) & 0xff;
    data[3] = (value >> 24) & 0xff;
}

}
