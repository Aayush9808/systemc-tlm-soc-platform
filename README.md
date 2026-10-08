# SystemC / TLM-2.0 SoC Platform

A reusable virtual SoC platform built with **SystemC 3.0.2** and **TLM-2.0**.

The project models a small firmware-visible RISC-V style SoC with memory, peripherals, interrupts, schema-driven register generation, DMI-enabled SRAM, timing modes and automated verification.

## Features

- SystemC / TLM-2.0 transaction modeling
- Memory-mapped interconnect
- 64 KiB read-only ROM
- 1 MiB SRAM with DMI support
- UART0 with TX/RX FIFOs and interrupts
- 32-bit GPIO with masked writes and edge interrupts
- Event-driven 64-bit timer
- Stateful SPI device with FIFO and interrupt
- PLIC-like interrupt controller
- Functional-fast and timed-LT simulation modes
- TLM transaction validation
- Optional transaction tracing
- Deterministic reset
- Schema-driven register and memory-map generation
- Automated CTest verification
- Performance benchmarking

## Architecture

```text
                    Scripted CPU
                      Initiator
                          |
                       TLM-2.0
                          |
                          v
                +-------------------+
                | Memory-Mapped     |
                | Interconnect      |
                +---------+---------+
                          |
        +---------+-------+-------+---------+---------+
        |         |       |       |         |         |
       ROM       SRAM    UART    GPIO     Timer      SPI
        |         |       |       |         |         |
        +---------+-------+-------+---------+---------+
                          |
                          v
                   IRQ Controller
                          |
                          v
                       CPU IRQ