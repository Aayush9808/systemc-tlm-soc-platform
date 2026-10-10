# SystemC / TLM-2.0 SoC Platform Design Note

## 1. Overview

This project implements a reusable virtual platform for a small firmware-visible RISC-V-style SoC using SystemC 3.0.2 and TLM-2.0.

The baseline uses a scripted TLM initiator instead of a full RISC-V ISS so the assignment remains focused on platform modeling, register behavior, routing, timing, interrupts, generation, verification and performance.

## 2. Architecture

    +--------------------+
    | Scripted CPU       |
    | TLM initiator      |
    +---------+----------+
              |
          b_transport
              v
    +--------------------+
    | Memory-Mapped      |
    | Interconnect       |
    | Generated map      |
    +---+---+---+---+----+
        |   |   |   |
       ROM SRAM UART GPIO
              |    |
            Timer SPI
              \    /
           IRQ Controller
                 |
               CPU IRQ

All firmware-visible accesses go through the interconnect. The router finds a generated memory-map range, verifies the complete transaction fits in that range, translates the system address to a target-local address, forwards the transaction, and restores the original system address.

The platform therefore has a clear initiator -> interconnect -> target structure and a separate peripheral -> IRQ controller -> CPU IRQ path.

## 3. Memory map and targets

The source of truth is specs/soc.yaml.

| Target | Base | Size | Main responsibility |
|---|---:|---:|---|
| ROM | 0x00000000 | 64 KiB | Read-only boot image |
| SRAM | 0x10000000 | 1 MiB | Read/write RAM and DMI |
| UART0 | 0x40000000 | 4 KiB | FIFO-backed serial I/O and IRQ |
| GPIO | 0x40010000 | 4 KiB | 32-bit I/O and edge IRQ |
| RV_TIMER | 0x40020000 | 4 KiB | 64-bit timer and compare IRQ |
| SPI_DEVICE | 0x40030000 | 4 KiB | Reduced stateful SPI FIFO |
| IRQ | 0x40040000 | 64 KiB | Pending/enable/claim/complete |

## 4. TLM transport

Targets implement b_transport with annotated SystemC delays.

The baseline transaction contract is:

- 32-bit accesses;
- 4-byte aligned addresses;
- streaming width 4;
- no byte enables;
- supported read/write commands only;
- complete transaction must fit in the target range;
- deterministic response status for invalid transactions.

The negative TLM test covers valid access, unaligned address, invalid length, invalid streaming width, byte enable, unsupported command and out-of-bounds access.

The targets do not call wait() inside b_transport. Instead, they add modeled delay to the delay argument and the initiator waits on that annotation.

## 5. Timing model

The platform exposes two modes.

### Timed-LT

The initiator receives target delays and waits for them. This advances SystemC simulated time according to the modeled target latency.

### Functional-fast

The same architectural transactions and checks are executed, but the scenario avoids waiting on target delays. This gives a faster functional run while keeping the same bus and device structure.

The timer is independent of host wall-clock time. Its counter is derived from sc_time_stamp(), prescaler and step configuration. Compare interrupts are scheduled with SystemC events.

A temporal-decoupling/quantum optimization is intentionally not part of the baseline. Explicit synchronization keeps the model simple and deterministic. In TLM-2.0, a quantum keeper tracks local simulated time while an initiator performs several transactions without synchronizing on every call; it synchronizes when its local time reaches the configured global quantum. That can reduce kernel scheduling overhead, but it changes when other processes observe transactions and therefore needs explicit timing/regression tests. The current scripted initiator instead waits on each `b_transport` delay annotation in timed-LT mode. Functional-fast mode skips annotated delay waits by design; it is a functional comparison mode, not a timing-equivalent optimization.

## 6. Schema-driven generation

YAML is the source of truth.

specs/soc.yaml defines the memory map. Device schemas (`uart.yaml`, `gpio.yaml`, `rv_timer.yaml`, `spi_device.yaml` and `irq.yaml`) define register names, offsets, reset values, masks and access types. Firmware scenarios and peripheral register enums consume generated offset constants, so relocating a declared register updates the software-side transaction address as well as generated metadata.

The generator validates:

- required sections;
- duplicate target names;
- positive sizes;
- non-negative bases;
- uint64 address overflow;
- overlapping ranges;
- duplicate register names and offsets;
- 4-byte alignment;
- RO/RW/WO/W1C access types;
- reset and mask ranges.

It generates memory-map constants, register metadata, dispatch helpers, software headers, register documentation and verification artifacts.

Generated code is kept separate from handwritten model behavior. Static metadata and register offsets are generated; FIFO behavior, timer scheduling, GPIO edge detection, SPI exchange and IRQ behavior remain handwritten. Generated reset constants are consumed by peripheral and IRQ state initialization/reset paths. All register-backed peripheral and IRQ targets look up the generated metadata before accepting a transaction: unknown offsets are rejected, RO/WO access rules are enforced, and write values are masked according to YAML. W1C behavior remains implemented in each target's state-transition logic and is covered by integration checks.

The interconnect's address-range selection is data-driven, and the regression suite compiles the real interconnect against a test-generated map to prove that relocating UART0 in YAML routes a TLM transaction at the new base to the UART socket with a target-local offset; the old address becomes unmapped. The socket inventory and `TargetId` dispatch switch are still handwritten for the seven supported targets. Therefore changing an existing target's base/size is schema-driven, but introducing a brand-new target still requires adding its socket, binding and dispatch case in C++.

The generator also has a --check mode that generates into a temporary directory and compares the result with committed generated output, so stale generated files are detected without modifying the repository.

## 7. Peripheral and host behavior

UART provides control/status/data/FIFO/interrupt registers, TX host-console output, RX injection and interrupt state.

GPIO provides input/output/output-enable state, direct and masked writes, rising/falling edge configuration and interrupt state. Output changes can be observed through the model callback.

The timer exposes a 64-bit value and compare through 32-bit registers. A lower-half read captures a coherent sample and the following upper-half read returns the matching upper half. The integration regression seeds the counter near the 32-bit boundary, captures the lower half, advances simulated time across rollover, and checks both the latched upper half and the subsequent live upper half. Reset verification also checks both counter halves, both compare halves, interrupt enable/state, and control/configuration registers.

SPI provides reduced control/config/status/TX/RX FIFO behavior. Configuration affects the modeled exchange and RX availability can raise an interrupt. Full flash/TPM functionality is intentionally outside the reduced model.

## 8. Interrupt architecture

UART, GPIO, timer and SPI drive interrupt signals into a minimal PLIC-like controller.

The controller exposes pending, enable, claim and complete registers. A pending and enabled source becomes claimable. Claim returns a deterministic source ID and completion releases the claimed source.

The timer uses event-driven compare scheduling. GPIO interrupts are generated from input transitions. UART and SPI interrupts are generated from FIFO/event conditions.

This keeps interrupt behavior event-driven instead of using host wall-clock polling.

## 9. DMI

SRAM grants DMI over its memory. The interconnect translates the DMI target-local range back into the system address map and clamps the advertised DMI window to the SRAM range declared by the generated memory map. This prevents a target's broader local DMI grant from exposing addresses outside the firmware-visible SRAM mapping.

The standalone TLM/DMI demonstration verifies that a direct DMI write is visible through a later TLM read. The benchmark also compares repeated TLM accesses with direct DMI accesses.

Because the SRAM mapping and backing storage are static for the lifetime of this simulation, runtime DMI invalidation is not required by the current model. If a target changes a granted DMI pointer, access permissions, or mapped range while the simulation is running, it must invalidate affected DMI regions with the standard TLM invalidation path so initiators do not continue using stale pointers. Any future remapping or dynamic permission changes must add an invalidation regression test.

## 10. Reset

Reset is explicit and deterministic. The integrated scenario first advances simulated time, creates state in the devices, then calls reset on SRAM, UART, GPIO, timer, SPI and IRQ controller.

After zero-time synchronization it checks that memory, peripheral state and interrupt signals are cleared. This also verifies that reset is exercised after simulation time is non-zero.

There is no global mutable singleton reset state.

## 11. Verification

Verification has three levels.

### Unit / negative

TLM transaction-policy checks validate width, alignment, streaming width, byte enables, command type and bounds.

The generator validates malformed schema conditions before producing output. Register offsets for UART, GPIO, RV_TIMER, SPI_DEVICE and the IRQ controller are declared in YAML and emitted as C++ constants; the scripted firmware uses those generated offsets instead of numeric register offsets.

### Integration

The firmware-style scenario checks:

- ROM constant read and ROM-to-SRAM copy;
- SRAM verification;
- GPIO direct/masked output and output-enable behavior;
- invalid address handling;
- UART IRQ claim/clear/complete;
- GPIO edge IRQ;
- timer compare IRQ;
- SPI exchange and IRQ;
- reset and post-reset state.

The same scenario runs in timed-LT and functional-fast modes.

### Determinism

The scenario returns an explicit PASS/FAIL result. Benchmark checksums provide an additional deterministic sanity check.

## 12. Performance

The interconnect counts transactions and bytes per target. Tracing is optional and disabled by default so normal benchmark traffic does not pay formatting/printing cost.

The mixed benchmark measures host runtime, simulated time, transaction throughput, per-target traffic and checksum.

The separate DMI experiment measures the direct-memory path against TLM transport. The trace experiment measures the cost of detailed transaction output.

The main baseline bottleneck is normal TLM/SystemC transport and scheduling. A possible future optimization is temporal-decoupling/quantum batching, but it is intentionally not enabled because explicit LT synchronization is easier to verify.

## 13. Scaling and future extensions

For 100+ IP blocks, the same schema can remain the source of truth while generated registries/factories reduce handwritten integration work. Schema versioning should be introduced before incompatible schema changes.

A real RISC-V ISS can replace the scripted initiator without changing the target models or memory-map architecture. An approximately-timed interconnect or NoC can be inserted later at the transaction boundary.

Device-tree/SVD generation, checkpointing, visualization and differential testing are possible extensions but are outside the baseline scope.

## 14. Fidelity gaps and trade-offs

This is a virtual platform, not a cycle-accurate implementation.

The CPU is scripted rather than a full ISS. SPI and the IRQ controller are reduced functional models. Target timing is loosely timed rather than a detailed pipeline, arbitration or NoC timing model.

These trade-offs are deliberate: they preserve the required TLM architecture, firmware-visible behavior, deterministic testing, generation flow and measurable performance without adding unnecessary implementation risk.

## 15. Conclusion

The platform separates generated structure from handwritten behavior, routes firmware-visible accesses through a reusable TLM interconnect, provides real peripheral state and IRQ behavior, supports SRAM DMI and two timing modes, and includes automated verification and performance measurement.

The design therefore provides a strong baseline virtual-platform submission with clear extension points for a real RISC-V ISS and richer timing models.
