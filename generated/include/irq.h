#pragma once

#include <cstdint>
#include "register_metadata.h"

namespace generated {

namespace irq {

constexpr uint32_t PENDING_OFFSET = 0x00000000;
constexpr uint32_t PENDING_RESET = 0x00000000;
constexpr uint32_t PENDING_MASK = 0xFFFFFFFF;
constexpr const char* PENDING_ACCESS = "W1C";

constexpr uint32_t ENABLE_OFFSET = 0x00000004;
constexpr uint32_t ENABLE_RESET = 0x00000000;
constexpr uint32_t ENABLE_MASK = 0xFFFFFFFF;
constexpr const char* ENABLE_ACCESS = "RW";

constexpr uint32_t CLAIM_OFFSET = 0x00000008;
constexpr uint32_t CLAIM_RESET = 0x00000000;
constexpr uint32_t CLAIM_MASK = 0xFFFFFFFF;
constexpr const char* CLAIM_ACCESS = "RO";

constexpr uint32_t COMPLETE_OFFSET = 0x0000000C;
constexpr uint32_t COMPLETE_RESET = 0x00000000;
constexpr uint32_t COMPLETE_MASK = 0xFFFFFFFF;
constexpr const char* COMPLETE_ACCESS = "WO";

constexpr RegisterMetadata REGISTERS[] = {
    {"PENDING", 0x00000000, 0x00000000, 0xFFFFFFFF, RegisterAccess::W1C},
    {"ENABLE", 0x00000004, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"CLAIM", 0x00000008, 0x00000000, 0xFFFFFFFF, RegisterAccess::RO},
    {"COMPLETE", 0x0000000C, 0x00000000, 0xFFFFFFFF, RegisterAccess::WO},
};

constexpr unsigned int REGISTER_COUNT =
    sizeof(REGISTERS) / sizeof(REGISTERS[0]);

}
}
