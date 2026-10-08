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

namespace rv_timer {

constexpr uint32_t CTRL_OFFSET = 0x00000000;
constexpr uint32_t CTRL_RESET = 0x00000000;
constexpr uint32_t CTRL_MASK = 0xFFFFFFFF;
constexpr const char* CTRL_ACCESS = "RW";

constexpr uint32_t CFG0_OFFSET = 0x00000004;
constexpr uint32_t CFG0_RESET = 0x00000000;
constexpr uint32_t CFG0_MASK = 0xFFFFFFFF;
constexpr const char* CFG0_ACCESS = "RW";

constexpr uint32_t TIMER_V_LOWER_OFFSET = 0x00000008;
constexpr uint32_t TIMER_V_LOWER_RESET = 0x00000000;
constexpr uint32_t TIMER_V_LOWER_MASK = 0xFFFFFFFF;
constexpr const char* TIMER_V_LOWER_ACCESS = "RO";

constexpr uint32_t TIMER_V_UPPER_OFFSET = 0x0000000C;
constexpr uint32_t TIMER_V_UPPER_RESET = 0x00000000;
constexpr uint32_t TIMER_V_UPPER_MASK = 0xFFFFFFFF;
constexpr const char* TIMER_V_UPPER_ACCESS = "RO";

constexpr uint32_t COMPARE_LOWER_OFFSET = 0x00000010;
constexpr uint32_t COMPARE_LOWER_RESET = 0x00000000;
constexpr uint32_t COMPARE_LOWER_MASK = 0xFFFFFFFF;
constexpr const char* COMPARE_LOWER_ACCESS = "RW";

constexpr uint32_t COMPARE_UPPER_OFFSET = 0x00000014;
constexpr uint32_t COMPARE_UPPER_RESET = 0x00000000;
constexpr uint32_t COMPARE_UPPER_MASK = 0xFFFFFFFF;
constexpr const char* COMPARE_UPPER_ACCESS = "RW";

constexpr uint32_t INTR_STATE_OFFSET = 0x00000018;
constexpr uint32_t INTR_STATE_RESET = 0x00000000;
constexpr uint32_t INTR_STATE_MASK = 0xFFFFFFFF;
constexpr const char* INTR_STATE_ACCESS = "W1C";

constexpr uint32_t INTR_ENABLE_OFFSET = 0x0000001C;
constexpr uint32_t INTR_ENABLE_RESET = 0x00000000;
constexpr uint32_t INTR_ENABLE_MASK = 0xFFFFFFFF;
constexpr const char* INTR_ENABLE_ACCESS = "RW";

constexpr RegisterMetadata REGISTERS[] = {
    {"CTRL", 0x00000000, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"CFG0", 0x00000004, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"TIMER_V_LOWER", 0x00000008, 0x00000000, 0xFFFFFFFF, RegisterAccess::RO},
    {"TIMER_V_UPPER", 0x0000000C, 0x00000000, 0xFFFFFFFF, RegisterAccess::RO},
    {"COMPARE_LOWER", 0x00000010, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"COMPARE_UPPER", 0x00000014, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"INTR_STATE", 0x00000018, 0x00000000, 0xFFFFFFFF, RegisterAccess::W1C},
    {"INTR_ENABLE", 0x0000001C, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
};

constexpr unsigned int REGISTER_COUNT =
    sizeof(REGISTERS) / sizeof(REGISTERS[0]);

}
}
