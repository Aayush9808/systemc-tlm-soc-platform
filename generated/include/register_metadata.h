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

}
