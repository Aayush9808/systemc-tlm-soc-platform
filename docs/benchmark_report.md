# SoC Platform Benchmark Report

## 1. Purpose

This report records performance measurements for the SystemC/TLM-2.0 SoC platform.

Measured areas are:

- mixed memory/MMIO workload;
- host runtime;
- simulated time;
- transaction throughput;
- per-target traffic;
- SRAM DMI benefit;
- optional trace overhead;
- timing-mode behavior.

## 2. Test environment

- Platform: macOS Apple Silicon / arm64
- Compiler: Apple Clang
- Build: CMake
- SystemC: 3.0.2
- Main workload: 1,000,000 iterations

Host runtime is measured with std::chrono::steady_clock. Simulated time is measured with SystemC sc_time_stamp(). Sections 3–6 retain historical local measurements from the earlier macOS build; the current Linux Release measurements and quantum comparison are recorded in Sections 7 and 10–11.

## 3. Historical local mixed SoC benchmark

Command:

    ./build/soc_bench 1000000

Recorded result:

| Metric | Result |
|---|---:|
| Requested iterations | 1,000,000 |
| Bus transactions | 2,006,000 |
| SRAM transactions | 2,000,000 |
| UART transactions | 2,000 |
| GPIO transactions | 2,000 |
| SPI transactions | 2,000 |
| Host runtime | 0.268628 s |
| Throughput | 7,467,578 transactions/s |
| Simulated time | 20,060 us |
| Checksum | 0xe0380 |
| Result | PASS |

The workload performs one SRAM write and one SRAM read per iteration. Every 1,000 iterations it adds representative GPIO, UART and SPI traffic.

## 4. Historical DMI comparison

A separate 1,000,000-operation SRAM experiment compared ordinary TLM transport with direct DMI pointer access.

| Metric | TLM | DMI |
|---|---:|---:|
| Operations | 1,000,000 | 1,000,000 |
| Host runtime | 0.113244 s | 0.000447 s |
| Approx. throughput | 8.83 million ops/s | 2.239 billion ops/s |
| Simulated time | about 10.00001 ms | about 10.00001 ms |

Measured DMI speedup: approximately **253.6x**.

This result is workload- and host-dependent. It demonstrates the expected benefit of bypassing repeated TLM transport for high-volume SRAM access.

## 5. Historical trace overhead

A 5,000-iteration run was measured with tracing disabled and enabled.

| Mode | Host runtime | Approx. throughput |
|---|---:|---:|
| Trace OFF | 0.003228 s | 3.106 M/s |
| Trace ON | 0.014767 s | 0.679 M/s |

Trace-enabled runtime was approximately **4.57x** the trace-disabled runtime in this run.

The overhead is expected because the trace path formats and prints transaction details for every routed access. Tracing is therefore disabled by default.

## 6. Timing modes

The firmware scenario was run in both modes.

- timed-LT: approximately 1340 ns simulated time in the recorded boot run.
- functional-fast: approximately 160 ns simulated time in the recorded boot run.

The exact values depend on the scenario, but timed-LT preserves target delay waiting while functional-fast suppresses that waiting for faster functional execution.

The timer is driven by SystemC simulated time, never by host wall-clock time.

## 7. Temporal-decoupling experiment

The benchmark initiator supports both per-transaction synchronization and a TLM quantum keeper with a 100 ns global quantum. GitHub Actions run #229 executed both modes in the same Release build on the same runner.

| Metric | Per-transaction baseline | Quantum keeper (100 ns) |
|---|---:|---:|
| Bus transactions | 2,006,000 | 2,006,000 |
| Host runtime | 0.286907 s | 0.122872 s |
| Throughput | 6,991,809 transactions/s | 16,325,973 transactions/s |
| Simulated time | 24,072 us | 24,072 us |
| Synchronization count | 2,006,000 | 240,720 |
| Checksum | 0xe0380 | 0xe0380 |
| Result | PASS | PASS |

On this workload, the quantum-keeper path delivered approximately **2.33x host-throughput** and reduced synchronization calls by approximately **8.34x**. CI explicitly verifies that transaction counts, simulated time and checksum match between modes, and that the quantum path performs fewer synchronizations.

This is a benchmark-specific result: the workload contains SRAM and periodic MMIO traffic but does not exercise timer/IRQ observation during the quantum window. It demonstrates safe batching for this workload, not universal equivalence for arbitrary asynchronous peripheral scenarios.

## 8. Bottleneck and optimization decision

The baseline bottleneck is the cost of repeated blocking transport and per-transaction synchronization. The measured quantum experiment shows that synchronization frequency is a meaningful optimization target, while the matching checksum and simulated time provide basic equivalence evidence.

An optimization deliberately not made is cycle-accurate, beat-level bus arbitration. The platform instead uses a documented fixed-latency serialization approximation; a detailed arbitration model would add complexity and false precision without a multi-master architecture workload to validate it. DMI remains the intentional fast path for SRAM-heavy accesses.

## 9. Reproduction

Build the Release configuration used by CI:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j

Run:

    ./build/soc_bench 1000000
    ./build/soc_bench 1000000 --quantum-ns 100
    ./build/soc_bench 1000000 --dmi --quantum-ns 100
    ./build/soc_bench 5000 --trace
    ./build/tlm_demo

For comparable results, compare throughput, transaction counts, simulated time, synchronization count and checksum. Host runtime varies with machine load.

## 10. Current Linux CI baseline (2026-10-11)

Recorded by GitHub Actions run #230 on commit `9e8733da702395f8ba5c7abca10d9ae46b76bc1c`. The workflow's subsequent runs also re-execute the benchmark and temporal-decoupling comparison after code changes.

- Host: GitHub-hosted Ubuntu 24.04.5 VM, AMD EPYC 9V74 80-Core Processor (4 vCPUs assigned).
- Compiler: GNU C++ 13.3.0.
- SystemC: 2.3.4 (Ubuntu package).
- Build: CMake Release.
- Trace: OFF.
- Command: `./build/soc_bench 1000000`.

| Metric | CI result |
|---|---:|
| Requested iterations | 1,000,000 |
| Bus transactions | 2,006,000 |
| SRAM transactions | 2,000,000 |
| UART transactions | 2,000 |
| GPIO transactions | 2,000 |
| SPI transactions | 2,000 |
| Host runtime | 0.279942 s |
| Throughput | 7,165,761 transactions/s |
| Simulated time | 24,072 us |
| Synchronization count | 2,006,000 |
| Checksum | 0xe0380 |
| Result | PASS |

A paired baseline run in the quantum comparison step measured 0.286907 s; small differences between baseline runs reflect runner load. Use the paired measurements in Section 7 for the temporal-decoupling comparison.

## 11. Reproducible quantum-keeper comparison

The CI comparison command runs:

    ./build/soc_bench 1000000
    ./build/soc_bench 1000000 --quantum-ns 100

The comparison fails if either workload does not report PASS, or if bus transaction count, simulated time or checksum differs. It also fails unless the quantum keeper reduces synchronization count. The recorded pair passed all checks.

## 12. Conclusion

The platform now reports measured baseline throughput, SRAM DMI benefit, trace overhead and a quantum-keeper experiment. It also verifies deterministic transaction counts, simulated time and checksum across baseline and temporal-decoupled benchmark modes.
