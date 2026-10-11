# Assignment Requirements Traceability

This matrix maps the original SystemC/TLM-2.0 assignment and Khushal's follow-up feedback to the current implementation and automated evidence. “Verified” means covered by the repository's CI workflow; it does not imply cycle-accurate hardware fidelity.

| Requirement / feedback | Implementation | Verification evidence | Status / scope note |
|---|---|---|---|
| SystemC/TLM-2.0 initiator → interconnect → targets | `src/main.cpp`, `model/interconnect/simple_interconnect.h` | `soc_boot_timed_lt`, `soc_boot_functional_fast` | Implemented; initiator is scripted, not a full RISC-V ISS. |
| ROM reads, writes rejected, binary image loading | `model/memory/simple_rom.h`, `src/main.cpp --rom-image` | Relocated-schema integration test loads a custom image and verifies ROM→SRAM copy in both timing modes | Implemented; built-in deterministic image remains the default. |
| SRAM read/write, configurable latency, DMI | `model/memory/simple_memory.h`, `src/main.cpp --sram-latency-ns` | `tlm_demo_dmi`, million-iteration benchmark, interconnect DMI boundary probe | Implemented; backing store is static during a run, so runtime DMI invalidation is not needed in this model. Any future remap/permission change must invalidate grants. |
| YAML is the integration source of truth | `specs/soc.yaml`, `generator/generate.py`, generated memory map | `generator/generate.py --check`; test moves UART and confirms old address is unmapped | Implemented for existing target map entries. Adding a brand-new target still requires a C++ socket, binding and dispatch case. |
| Generated register offsets, reset values, masks and access policy | `specs/*.yaml`, `generated/include/`, `generated/sw/` | Generator mutation tests and generated-artifact check | Implemented. UART CTRL TX/RX fields demonstrate generated C++ and firmware field masks. |
| Generated register dispatch is used by models | `generated/src/*_registers.cpp`; peripheral and IRQ models | Full build and relocated-map integration test | Implemented: models call per-device generated lookup/access functions; dispatch sources are linked into simulator, demo and benchmark. Handwritten code still implements stateful behavior. |
| Schema rejects malformed ranges/registers/fields | `generator/generate.py` | `tests/generator_test.py`: overlap, overflow, duplicate names/offsets, invalid access, alignment, reset/mask and field-range checks | Implemented for the currently defined schema. |
| TLM payload contract and deterministic errors | `model/interconnect/simple_interconnect.h`, target models | `tlm_policy_test`; unwired-target and address-relocation probes | Implemented for the documented 32-bit, aligned, 4-byte, no-byte-enable policy. |
| SRAM DMI range cannot exceed generated map | `model/interconnect/simple_interconnect.h` | Reduced-SRAM DMI probe in `tests/generator_test.py`; `tlm_demo_dmi` | Implemented and regression-tested. |
| UART FIFO/status/interrupt/host console | `model/peripherals/uart.h` | Firmware integration scenario, including TX IRQ claim/source-clear/complete | Implemented reduced UART subset; RX is injected through the model stimulus API. |
| GPIO input/output, masked writes and edge IRQ | `model/peripherals/gpio.h` | Firmware integration scenario and GPIO IRQ clear-before-complete regression | Implemented rising/falling edge subset; not a complete OpenTitan GPIO implementation. |
| Timer simulated-time counter, compare, split-read coherency and reset | `model/peripherals/rv_timer.h` | Timer compare/IRQ, rollover latch and post-time-zero reset checks in integration scenario | Implemented with event-driven compare scheduling; timing is loosely timed. |
| SPI stateful TX/RX FIFO and IRQ | `model/peripherals/spi_device.h` | Integration exchange and repeated writes beyond FIFO capacity | Deliberately reduced synchronous transfer model; not a complete physical SPI controller. |
| IRQ pending/enable/claim/complete | `model/irq/irq_controller.h` | UART/GPIO/timer/SPI integration scenarios, including source clear before completion | Implemented reduced PLIC-like model. Priority is fixed by source order; programmable priority/threshold are not implemented. |
| Deterministic firmware-like workload | `src/main.cpp` | ROM copy, UART banner, GPIO direct/masked, invalid access, UART/GPIO/timer/SPI IRQs and reset; both timing modes | Implemented; returns nonzero on scenario failure. |
| Reset after simulation time advances | `src/main.cpp`, all stateful models | Reset scenario verifies memory, peripheral registers and IRQ lines | Implemented and exercised by both timing-mode tests. |
| Performance measurement and mixed-MMIO workload ≥1,000,000 iterations | `benchmarks/soc_bench.cpp`, `docs/benchmark_report.md` | CI runs `./build/soc_bench 1000000`; checks PASS, transaction counts, simulated time and checksum in logs | Implemented; host timings vary by runner. Historical DMI and tracing measurements are environment-specific. |
| Trace/counters and target byte accounting | `model/interconnect/simple_interconnect.h`, benchmark `--trace` | Benchmark trace experiment and per-target counters | Implemented; tracing is disabled by default. |
| Temporal decoupling / quantum keeper | `docs/design_note.md`, `docs/benchmark_report.md` | Measured baseline benchmark is recorded | Quantum keeper is intentionally not implemented; no comparative speedup is claimed. A dedicated quantum experiment remains future work. |
| Reproducible setup and attribution | `README.md`, `CMakeLists.txt`, `requirements.txt`, `LICENSE`, `THIRD_PARTY_NOTICES.md` | CI installs dependencies, checks generated artifacts, configures, builds, tests and runs benchmark | Implemented for the documented Ubuntu CI environment; tested SystemC version is stated in the README/design note. |

## Automated verification sequence

The CI workflow performs, in order:

1. `python3 generator/generate.py --check`
2. CMake configuration and build
3. `ctest --test-dir build --output-on-failure`
4. `./build/soc_bench 1000000`

The generator test additionally compiles the real SoC against mutated YAML (UART base moved to `0x50000000`, WDATA offset moved to `0x20`) and runs the firmware-like scenario in timed-LT and functional-fast modes, with a custom ROM image and configurable SRAM latency. It also compiles and runs the mixed benchmark against that relocated map.

## Explicit fidelity limits

- No full RISC-V instruction-set simulator or ELF execution.
- No cycle-accurate CPU, bus arbitration, or approximately-timed four-phase transport.
- SPI and IRQ controller are reduced functional models.
- New target classes still need handwritten socket/binding/dispatch integration.
- No quantum keeper or measured quantum-vs-no-quantum comparison.
