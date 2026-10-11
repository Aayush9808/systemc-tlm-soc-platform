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
| Host runtime | 0.296112 s | 0.136040 s |
| Throughput | 6,774,456 transactions/s | 14,745,662 transactions/s |
| Simulated time | 24,072 us | 24,072 us |
| Synchronization count | 2,006,000 | 240,720 |
| Checksum | 0xe0380 | 0xe0380 |
| Result | PASS | PASS |

On this workload, the quantum-keeper path delivered approximately **2.18x host-throughput** and reduced synchronization calls by approximately **8.34x**. CI explicitly verifies that transaction counts, simulated time and checksum match between modes, and that the quantum path performs fewer synchronizations.

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

Recorded by GitHub Actions run #248 on commit `10389fb06209ff18c95f168692eb326735ab18ed`. The workflow's subsequent runs also re-execute the benchmark and temporal-decoupling comparison after code changes.

- Host: GitHub-hosted Ubuntu 24.04.5 VM, AMD EPYC 7763 64-Core Processor (4 vCPUs assigned).
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
| Host runtime | 0.297945 s |
| Throughput | 6,732,789 transactions/s |
| Simulated time | 24,072 us |
| Synchronization count | 2,006,000 |
| Checksum | 0xe0380 |
| Result | PASS |

The paired comparison below uses the same workflow run and runner for all three modes; separate baseline runs vary with runner load.

## 11. Reproducible quantum-keeper and DMI comparison

The CI comparison runs all three modes on the same Release build:

    ./build/soc_bench 1000000
    ./build/soc_bench 1000000 --quantum-ns 100
    ./build/soc_bench 1000000 --dmi --quantum-ns 100

| Metric | TLM baseline | TLM + 100 ns quantum | SRAM DMI + 100 ns quantum |
|---|---:|---:|---:|
| Host runtime | 0.296112 s | 0.136040 s | 0.032845 s |
| Throughput | 6,774,456 ops/s | 14,745,662 ops/s | 61,075,264 ops/s |
| Bus transactions | 2,006,000 | 2,006,000 | 6,000 |
| DMI SRAM operations | 0 | 0 | 2,000,000 |
| Total operations | 2,006,000 | 2,006,000 | 2,006,000 |
| Simulated time | 24,072 us | 24,072 us | 20,072 us |
| Synchronization count | 2,006,000 | 240,720 | 200,720 |
| Checksum | 0xe0380 | 0xe0380 | 0xe0380 |
| Result | PASS | PASS | PASS |

The quantum TLM path is about **2.18x faster** than the per-transaction baseline and reduces synchronization count by about **8.34x**. The DMI+quantum path is about **4.14x faster** than quantum TLM transport for this SRAM-heavy workload. DMI reduces bus transactions because the 2,000,000 SRAM accesses bypass the interconnect; the 6,000 remaining transactions are MMIO. Its simulated time is 4 ms lower because DMI access uses the SRAM's 10 ns latency and bypasses the 2 ns interconnect traversal. The checksum and total operation count remain identical.

The workflow fails if baseline and quantum modes disagree on bus transaction count, total operations, simulated time or checksum; if the quantum mode does not reduce synchronization count; or if DMI does not execute exactly 2,000,000 SRAM operations, preserve total operations/checksum, reduce bus transactions and simulated time, and improve throughput over quantum TLM. This is a benchmark-specific equivalence check, not proof that arbitrary asynchronous peripheral workloads can be quantum-decoupled without additional synchronization.

## 12. Conclusion

The platform reports a reproducible TLM baseline, SRAM DMI comparison, trace-overhead historical measurement and quantum-keeper experiment. CI verifies generated artifacts, unit/negative/integration tests, a million-iteration workload, deterministic repeated firmware runs, and matched checksum/transaction semantics across baseline, quantum and DMI benchmark modes.
