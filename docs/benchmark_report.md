# SoC Platform Benchmark Report

## 1. Purpose

This report records the performance measurements of the SystemC/TLM-2.0 SoC platform.

The benchmark focuses on:

- Large transaction counts
- Mixed memory and MMIO activity
- Host execution time
- Simulated time
- Transaction throughput
- Per-target transaction distribution
- DMI performance
- Trace overhead


## 2. Test Environment

The benchmark was executed on:

- Platform: macOS
- Architecture: Apple Silicon / arm64
- Compiler: Apple Clang
- Build system: CMake
- SystemC: 3.0.2
- Build configuration: Release


## 3. Mixed SoC Benchmark

Command:

```bash
./build/soc_bench 1000000