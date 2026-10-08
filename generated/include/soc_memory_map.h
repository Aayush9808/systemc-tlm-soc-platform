#pragma once

#include <cstdint>

namespace generated {

enum class TargetId {
    ROM,
    SRAM,
    UART0,
    GPIO,
    RV_TIMER,
    SPI_DEVICE,
    IRQ,
};

struct MemoryRange {
    const char* name;
    uint64_t base;
    uint64_t size;
    TargetId target;
};

constexpr uint64_t ROM_BASE = 0x00000000ULL;
constexpr uint64_t ROM_SIZE = 0x00010000ULL;

constexpr uint64_t SRAM_BASE = 0x10000000ULL;
constexpr uint64_t SRAM_SIZE = 0x00100000ULL;

constexpr uint64_t UART0_BASE = 0x40000000ULL;
constexpr uint64_t UART0_SIZE = 0x00001000ULL;

constexpr uint64_t GPIO_BASE = 0x40010000ULL;
constexpr uint64_t GPIO_SIZE = 0x00001000ULL;

constexpr uint64_t RV_TIMER_BASE = 0x40020000ULL;
constexpr uint64_t RV_TIMER_SIZE = 0x00001000ULL;

constexpr uint64_t SPI_DEVICE_BASE = 0x40030000ULL;
constexpr uint64_t SPI_DEVICE_SIZE = 0x00001000ULL;

constexpr uint64_t IRQ_BASE = 0x40040000ULL;
constexpr uint64_t IRQ_SIZE = 0x00010000ULL;

constexpr MemoryRange MEMORY_MAP[] = {
    {"ROM", ROM_BASE, ROM_SIZE, TargetId::ROM},
    {"SRAM", SRAM_BASE, SRAM_SIZE, TargetId::SRAM},
    {"UART0", UART0_BASE, UART0_SIZE, TargetId::UART0},
    {"GPIO", GPIO_BASE, GPIO_SIZE, TargetId::GPIO},
    {"RV_TIMER", RV_TIMER_BASE, RV_TIMER_SIZE, TargetId::RV_TIMER},
    {"SPI_DEVICE", SPI_DEVICE_BASE, SPI_DEVICE_SIZE, TargetId::SPI_DEVICE},
    {"IRQ", IRQ_BASE, IRQ_SIZE, TargetId::IRQ},
};

constexpr unsigned int MEMORY_MAP_COUNT =
    sizeof(MEMORY_MAP) / sizeof(MEMORY_MAP[0]);

}
