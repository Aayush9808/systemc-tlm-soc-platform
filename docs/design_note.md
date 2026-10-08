# SystemC / TLM-2.0 SoC Platform Design Note

## 1. Overview

This project implements a reusable virtual platform for a small firmware-visible RISC-V style SoC using SystemC and TLM-2.0.

The platform is designed around a memory-mapped interconnect and modular peripheral models. A lightweight scripted initiator is used to generate firmware-like bus transactions instead of integrating a full RISC-V instruction set simulator.

The main goals are:

- Correct TLM-2.0 transaction handling
- Firmware-visible memory-mapped peripherals
- Data-driven SoC memory mapping
- Schema-driven register generation
- Interrupt aggregation
- SRAM DMI support
- Functional-fast and timed-LT simulation modes
- Deterministic reset and verification
- Performance measurement for large transaction counts


## 2. Architecture

The main platform contains the following components:

```text
                    +-------------------+
                    |  Scripted CPU     |
                    |    Initiator      |
                    +---------+---------+
                              |
                              | TLM-2.0
                              v
                    +-------------------+
                    | Memory-Mapped      |
                    | Interconnect       |
                    +---------+---------+
                              |
          +---------+---------+---------+---------+---------+
          |         |         |         |         |         |
          v         v         v         v         v         v
        ROM       SRAM      UART      GPIO      Timer      SPI
                                                          |
          |         |         |         |         |         |
          +---------+---------+---------+---------+---------+
                              |
                              v
                       IRQ Controller
                              |
                              v
                         CPU IRQ line