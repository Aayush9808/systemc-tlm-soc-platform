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

namespace gpio {

constexpr uint32_t DATA_IN_OFFSET = 0x00000000;
constexpr uint32_t DATA_IN_RESET = 0x00000000;
constexpr uint32_t DATA_IN_MASK = 0xFFFFFFFF;
constexpr const char* DATA_IN_ACCESS = "RO";

constexpr uint32_t DIRECT_OUT_OFFSET = 0x00000004;
constexpr uint32_t DIRECT_OUT_RESET = 0x00000000;
constexpr uint32_t DIRECT_OUT_MASK = 0xFFFFFFFF;
constexpr const char* DIRECT_OUT_ACCESS = "RW";

constexpr uint32_t MASKED_OUT_LOWER_OFFSET = 0x00000008;
constexpr uint32_t MASKED_OUT_LOWER_RESET = 0x00000000;
constexpr uint32_t MASKED_OUT_LOWER_MASK = 0xFFFFFFFF;
constexpr const char* MASKED_OUT_LOWER_ACCESS = "WO";

constexpr uint32_t MASKED_OUT_UPPER_OFFSET = 0x0000000C;
constexpr uint32_t MASKED_OUT_UPPER_RESET = 0x00000000;
constexpr uint32_t MASKED_OUT_UPPER_MASK = 0xFFFFFFFF;
constexpr const char* MASKED_OUT_UPPER_ACCESS = "WO";

constexpr uint32_t DIRECT_OE_OFFSET = 0x00000010;
constexpr uint32_t DIRECT_OE_RESET = 0x00000000;
constexpr uint32_t DIRECT_OE_MASK = 0xFFFFFFFF;
constexpr const char* DIRECT_OE_ACCESS = "RW";

constexpr uint32_t MASKED_OE_LOWER_OFFSET = 0x00000014;
constexpr uint32_t MASKED_OE_LOWER_RESET = 0x00000000;
constexpr uint32_t MASKED_OE_LOWER_MASK = 0xFFFFFFFF;
constexpr const char* MASKED_OE_LOWER_ACCESS = "WO";

constexpr uint32_t MASKED_OE_UPPER_OFFSET = 0x00000018;
constexpr uint32_t MASKED_OE_UPPER_RESET = 0x00000000;
constexpr uint32_t MASKED_OE_UPPER_MASK = 0xFFFFFFFF;
constexpr const char* MASKED_OE_UPPER_ACCESS = "WO";

constexpr uint32_t INTR_STATE_OFFSET = 0x0000001C;
constexpr uint32_t INTR_STATE_RESET = 0x00000000;
constexpr uint32_t INTR_STATE_MASK = 0xFFFFFFFF;
constexpr const char* INTR_STATE_ACCESS = "W1C";

constexpr uint32_t INTR_ENABLE_OFFSET = 0x00000020;
constexpr uint32_t INTR_ENABLE_RESET = 0x00000000;
constexpr uint32_t INTR_ENABLE_MASK = 0xFFFFFFFF;
constexpr const char* INTR_ENABLE_ACCESS = "RW";

constexpr uint32_t INTR_RISE_OFFSET = 0x00000024;
constexpr uint32_t INTR_RISE_RESET = 0x00000000;
constexpr uint32_t INTR_RISE_MASK = 0xFFFFFFFF;
constexpr const char* INTR_RISE_ACCESS = "RW";

constexpr uint32_t INTR_FALL_OFFSET = 0x00000028;
constexpr uint32_t INTR_FALL_RESET = 0x00000000;
constexpr uint32_t INTR_FALL_MASK = 0xFFFFFFFF;
constexpr const char* INTR_FALL_ACCESS = "RW";

constexpr RegisterMetadata REGISTERS[] = {
    {"DATA_IN", 0x00000000, 0x00000000, 0xFFFFFFFF, RegisterAccess::RO},
    {"DIRECT_OUT", 0x00000004, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"MASKED_OUT_LOWER", 0x00000008, 0x00000000, 0xFFFFFFFF, RegisterAccess::WO},
    {"MASKED_OUT_UPPER", 0x0000000C, 0x00000000, 0xFFFFFFFF, RegisterAccess::WO},
    {"DIRECT_OE", 0x00000010, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"MASKED_OE_LOWER", 0x00000014, 0x00000000, 0xFFFFFFFF, RegisterAccess::WO},
    {"MASKED_OE_UPPER", 0x00000018, 0x00000000, 0xFFFFFFFF, RegisterAccess::WO},
    {"INTR_STATE", 0x0000001C, 0x00000000, 0xFFFFFFFF, RegisterAccess::W1C},
    {"INTR_ENABLE", 0x00000020, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"INTR_RISE", 0x00000024, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
    {"INTR_FALL", 0x00000028, 0x00000000, 0xFFFFFFFF, RegisterAccess::RW},
};

constexpr unsigned int REGISTER_COUNT =
    sizeof(REGISTERS) / sizeof(REGISTERS[0]);

}
}
