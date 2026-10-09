#pragma once

#include <cstdint>
#include "register_metadata.h"

namespace generated {

namespace spi_device {

constexpr uint32_t CONTROL_OFFSET = 0x00000000;
constexpr uint32_t CONTROL_RESET = 0x00000000;
constexpr uint32_t CONTROL_MASK = 0xFFFFFFFF;
constexpr const char* CONTROL_ACCESS = "RW";

constexpr uint32_t CFG_OFFSET = 0x00000004;
constexpr uint32_t CFG_RESET = 0x00000000;
constexpr uint32_t CFG_MASK = 0xFFFFFFFF;
constexpr const char* CFG_ACCESS = "RW";

constexpr uint32_t STATUS_OFFSET = 0x00000008;
constexpr uint32_t STATUS_RESET = 0x00000000;
constexpr uint32_t STATUS_MASK = 0xFFFFFFFF;
constexpr const char* STATUS_ACCESS = "RO";

constexpr uint32_t TX_OFFSET = 0x0000000C;
constexpr uint32_t TX_RESET = 0x00000000;
constexpr uint32_t TX_MASK = 0xFFFFFFFF;
constexpr const char* TX_ACCESS = "WO";

constexpr uint32_t RX_OFFSET = 0x00000010;
constexpr uint32_t RX_RESET = 0x00000000;
constexpr uint32_t RX_MASK = 0xFFFFFFFF;
constexpr const char* RX_ACCESS = "RO";

constexpr RegisterMetadata REGISTERS[] = {
    {"CONTROL", 0x00000000, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"CFG", 0x00000004, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"STATUS", 0x00000008, 0x00000000, 0xFFFFFFFF, RegisterAccess::RO},
    {"TX", 0x0000000C, 0x00000000, 0xFFFFFFFF, RegisterAccess::WO},
    {"RX", 0x00000010, 0x00000000, 0xFFFFFFFF, RegisterAccess::RO},
};

constexpr unsigned int REGISTER_COUNT =
    sizeof(REGISTERS) / sizeof(REGISTERS[0]);

}
}
