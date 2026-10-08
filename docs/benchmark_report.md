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

Host runtime is measured with std::chrono::steady_clock. Simulated time is measured with SystemC sc_time_stamp().

## 3. Mixed SoC benchmark

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

## 4. DMI comparison

A separate 1,000,000-operation SRAM experiment compared ordinary TLM transport with direct DMI pointer access.

| Metric | TLM | DMI |
|---|---:|---:|
| Operations | 1,000,000 | 1,000,000 |
| Host runtime | 0.113244 s | 0.000447 s |
| Approx. throughput | 8.83 M/s | 2.239 B/s |
| Simulated time | about 10.00001 ms | about 10.00001 ms |

Measured DMI speedup: approximately **253.6x**.

This result is workload- and host-dependent. It demonstrates the expected benefit of bypassing repeated TLM transport for high-volume SRAM access.

## 5. Trace overhead

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

## 7. Temporal-decoupling decision

The baseline does not use a quantum keeper.

The reason is deliberate: explicit LT synchronization is simple, deterministic and easy to verify for the assignment. A future temporal-decoupling experiment could batch local initiator time and synchronize at a chosen quantum.

The current benchmark therefore represents a straightforward b_transport baseline rather than an aggressively optimized temporal-decoupled model.

## 8. Bottleneck and optimization decision

The main bottleneck is normal TLM transport plus SystemC simulation/scheduling overhead.

One optimization intentionally not made is aggressive temporal-decoupling batching. It could improve throughput, but it would add synchronization complexity and could make MMIO observation less immediate.

DMI is the intentional optimization already enabled for SRAM and is particularly effective for repeated memory operations.

## 9. Reproduction

Build:

    cmake -S . -B build
    cmake --build build -j

Run:

    ./build/soc_bench 1000000
    ./build/soc_bench 5000 --trace
    ./build/tlm_demo

For comparable results, compare throughput, transaction counts, simulated time and checksum. Host runtime varies with machine load.

## 10. Conclusion

The platform provides a measurable baseline with millions of mixed TLM transactions, per-target counters, DMI acceleration, optional trace instrumentation and two timing modes.

The measurements identify normal TLM/SystemC transport as the main baseline bottleneck and justify DMI as the most useful existing fast path.
