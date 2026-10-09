#pragma once

#include <cstdint>
#include "register_metadata.h"

namespace generated {

namespace uart0 {

constexpr uint32_t CTRL_OFFSET = 0x00000000;
constexpr uint32_t CTRL_RESET = 0x00000000;
constexpr uint32_t CTRL_MASK = 0x00000003;
constexpr const char* CTRL_ACCESS = "RW";

constexpr uint32_t STATUS_OFFSET = 0x00000004;
constexpr uint32_t STATUS_RESET = 0x00000000;
constexpr uint32_t STATUS_MASK = 0xFFFFFFFF;
constexpr const char* STATUS_ACCESS = "RO";

constexpr uint32_t RDATA_OFFSET = 0x00000008;
constexpr uint32_t RDATA_RESET = 0x00000000;
constexpr uint32_t RDATA_MASK = 0xFFFFFFFF;
constexpr const char* RDATA_ACCESS = "RO";

constexpr uint32_t WDATA_OFFSET = 0x0000000C;
constexpr uint32_t WDATA_RESET = 0x00000000;
constexpr uint32_t WDATA_MASK = 0xFFFFFFFF;
constexpr const char* WDATA_ACCESS = "WO";

constexpr uint32_t FIFO_CTRL_OFFSET = 0x00000010;
constexpr uint32_t FIFO_CTRL_RESET = 0x00000000;
constexpr uint32_t FIFO_CTRL_MASK = 0xFFFFFFFF;
constexpr const char* FIFO_CTRL_ACCESS = "RW";

constexpr uint32_t FIFO_STATUS_OFFSET = 0x00000014;
constexpr uint32_t FIFO_STATUS_RESET = 0x00000000;
constexpr uint32_t FIFO_STATUS_MASK = 0xFFFFFFFF;
constexpr const char* FIFO_STATUS_ACCESS = "RO";

constexpr uint32_t INTR_STATE_OFFSET = 0x00000018;
constexpr uint32_t INTR_STATE_RESET = 0x00000000;
constexpr uint32_t INTR_STATE_MASK = 0xFFFFFFFF;
constexpr const char* INTR_STATE_ACCESS = "W1C";

constexpr uint32_t INTR_ENABLE_OFFSET = 0x0000001C;
constexpr uint32_t INTR_ENABLE_RESET = 0x00000000;
constexpr uint32_t INTR_ENABLE_MASK = 0xFFFFFFFF;
constexpr const char* INTR_ENABLE_ACCESS = "RW";

constexpr RegisterMetadata REGISTERS[] = {
    {"CTRL", 0x00000000, 0x00000000, 0x00000003, RegisterAccess::RW},
    {"STATUS", 0x00000004, 0x00000000, 0xFFFFFFFF, RegisterAccess::RO},
    {"RDATA", 0x00000008, 0x00000000, 0xFFFFFFFF, RegisterAccess::RO},
    {"WDATA", 0x0000000C, 0x00000000, 0xFFFFFFFF, RegisterAccess::WO},
    {"FIFO_CTRL", 0x00000010, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"FIFO_STATUS", 0x00000014, 0x00000000, 0xFFFFFFFF, RegisterAccess::RO},
    {"INTR_STATE", 0x00000018, 0x00000000, 0xFFFFFFFF, RegisterAccess::W1C},
    {"INTR_ENABLE", 0x0000001C, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
};

constexpr unsigned int REGISTER_COUNT =
    sizeof(REGISTERS) / sizeof(REGISTERS[0]);

}
}
