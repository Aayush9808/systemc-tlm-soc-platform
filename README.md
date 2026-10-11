# SystemC / TLM-2.0 SoC Platform

A reusable virtual SoC platform built and CI-tested with SystemC 2.3.4 and 3.0.2 and TLM-2.0.

## Architecture

    Scripted CPU / Initiator
             |
          TLM-2.0
             v
    +----------------------+
    | Memory-Mapped Bus    |
    | Generated map/router |
    +----+----+----+----+--+
         |    |    |    |
        ROM  SRAM UART GPIO
                  |    |
                Timer SPI
                  \    /
                IRQ Controller
                     |
                   CPU IRQ

All firmware-visible accesses go through the memory-mapped interconnect. The interconnect uses the generated memory map, validates the complete address range, translates to target-local addresses, annotates traversal latency and serializes overlapping annotated arrivals with a fixed-latency contention approximation.

## Implemented SoC

| Block | Base | Size | Behavior |
|---|---:|---:|---|
| ROM | 0x00000000 | 64 KiB | Read-only boot image |
| SRAM | 0x10000000 | 1 MiB | Read/write + DMI |
| UART0 | 0x40000000 | 4 KiB | TX/RX FIFO, status/control, IRQ |
| GPIO | 0x40010000 | 4 KiB | 32-bit I/O, masked writes, edge IRQ |
| RV_TIMER | 0x40020000 | 4 KiB | 64-bit simulated-time counter/compare |
| SPI_DEVICE | 0x40030000 | 4 KiB | Stateful TX/RX FIFO + IRQ |
| IRQ | 0x40040000 | 64 KiB | Pending/enable/claim/complete |

## Assignment requirements covered

- TLM-2.0 b_transport with annotated delay.
- Alignment, length, streaming-width and byte-enable validation.
- Deterministic unsupported-command and unmapped-address errors.
- Data-driven memory-map routing.
- SRAM DMI with translated system address range.
- ROM and SRAM binary initialization images with size/error validation.
- Independent SRAM read/write latency and configurable interconnect/peripheral annotated latency.
- Optional transaction tracing and per-target counters.
- Functional-fast and timed-LT modes.
- Event-driven simulated-time timer.
- UART, GPIO, timer, SPI and IRQ behavior.
- Deterministic reset after simulated time advances.
- YAML source-of-truth schema and deterministic code generation.
- Generated register metadata, map constants, software headers and verification artifacts.
- Unit/negative/integration verification, generated register verification vectors, repeated-run determinism and performance measurement.

## Prerequisites

- C++17 compiler
- CMake 3.20+
- Python 3.10+
- SystemC 2.3.4 or SystemC 3.x
- PyYAML

Example:

    python3 -m venv .venv
    source .venv/bin/activate
    pip install -r requirements.txt

If SystemC is installed outside the Homebrew default:

    cmake -S . -B build -DSYSTEMC_ROOT=/path/to/systemc

## Verified toolchains

- GitHub Actions Linux CI: Ubuntu 24.04.5, GNU C++ 13.3.0, SystemC 2.3.4.
- Local development environment: macOS arm64, Apple Clang 21.0.0, SystemC 3.0.2.
- C++ standard: C++17.

## Build and generate

First verify the committed generated artifacts:

    python3 generator/generate.py --check

Configure with the installed SystemC prefix:

- Ubuntu / CI-style installation:

      cmake -S . -B build -DSYSTEMC_ROOT=/usr -DCMAKE_BUILD_TYPE=Release

- macOS with Homebrew:

      cmake -S . -B build -DSYSTEMC_ROOT="$(brew --prefix systemc)" -DCMAKE_BUILD_TYPE=Release

Then build:

    cmake --build build -j

To intentionally regenerate artifacts after editing YAML, run `python3 generator/generate.py`, review the diff, and rerun `python3 generator/generate.py --check`.

## Verification

    ctest --test-dir build --output-on-failure

    ./build/soc_sim --scenario boot --timing timed-lt
    ./build/soc_sim --scenario boot --timing functional-fast
    ./build/soc_sim --scenario boot --timing timed-lt --rom-image firmware.bin
    ./build/soc_sim --scenario boot --timing timed-lt --sram-image sram.bin
    ./build/soc_sim --scenario boot --timing timed-lt --sram-read-latency-ns 3 --sram-write-latency-ns 5 --interconnect-latency-ns 2
    ./build/soc_sim --scenario boot --timing timed-lt --peripheral-latency-ns 7 --uart-latency-ns 11 --rom-latency-ns 13
    ./build/soc_sim --scenario boot --timing timed-lt --sram-latency-ns 5
    ./build/tlm_demo

The integrated scenario checks ROM-to-SRAM copy, GPIO direct/masked access, invalid transactions, UART IRQ, GPIO edge IRQ, timer compare IRQ, SPI exchange/IRQ, reset and deterministic PASS/FAIL behavior. If `--rom-image` is omitted, the deterministic built-in ROM data table is used; when supplied, the binary image is loaded into ROM and rejected if it exceeds the generated ROM size. SRAM can be initialized from an optional binary image with `--sram-image`; oversized or unreadable images fail initialization. Read and write latency are independently configurable with `--sram-read-latency-ns` and `--sram-write-latency-ns` (both default to 10 ns). `--sram-latency-ns` remains a compatibility shortcut that sets both. ROM read latency is configurable with `--rom-latency-ns` (default: 10 ns). The interconnect contributes `--interconnect-latency-ns` (default: 2 ns). `--peripheral-latency-ns` sets the common default for UART/GPIO/timer/SPI/IRQ (10 ns), and each target can override it with `--uart-latency-ns`, `--gpio-latency-ns`, `--timer-latency-ns`, `--spi-latency-ns` or `--irq-latency-ns`. These annotated transport delays do not change the timer's counter tick semantics.

The TLM policy test covers valid access, unaligned access, invalid length, invalid streaming width, byte enables, unsupported commands and out-of-bounds access.

## Performance

    ./build/soc_bench 1000000
    ./build/soc_bench 1000000 --quantum-ns 100
    ./build/soc_bench 1000000 --dmi --quantum-ns 100
    ./build/soc_bench 5000 --trace

Historical local macOS measurements:
- Mixed 1M-iteration run: 2,006,000 bus transactions, 0.268628 s host runtime, 7,467,578 transactions/s, 20,060 us simulated time (before the explicit interconnect latency was added).
- Separate 1M-operation DMI experiment: approximately 253.6x speedup over the corresponding TLM workload.
- 5,000-iteration trace experiment: approximately 4.57x host-runtime overhead with tracing enabled.

Current Release Linux CI measurement (Ubuntu 24.04.5, GCC 13.3.0, SystemC 2.3.4, AMD EPYC 7763 runner): 2,006,000 bus transactions in 0.303990 s, 6.599 million operations/s and 24,072 us simulated time. In the paired run, the 100 ns quantum-keeper path measured 0.135665 s vs 0.296457 s baseline (~2.18x throughput), reducing synchronization count from 2,006,000 to 240,720 while preserving checksum and simulated time. The SRAM DMI+quantum path executed 2,000,000 SRAM operations with only 6,000 MMIO bus transactions, measured 62.311 million operations/s (~4.21x the quantum TLM path), and preserved total operation count/checksum. CI's trace on/off experiment processed 200,600 transactions in both modes, emitted 200,600 trace records when enabled, and measured a 6.85x runtime ratio on this runner. Host results vary by runner; see docs/benchmark_report.md for exact commands and evidence.

## Timing

timed-lt: target delays are annotated and the initiator waits for them.

functional-fast: the same functional scenario is exercised without waiting on each target delay.

The timer uses SystemC simulated time, not host wall-clock time.

## Schema and generation

specs/soc.yaml is the memory-map source of truth. Device YAML files define register metadata.

The generator validates duplicate names/offsets, alignment, access types, reset/mask ranges, field identifiers/access/ranges/overlaps, zero-sized/overlapping memory ranges and uint64 address overflow.

Register fields (for example UART CTRL TX/RX) generate C++ bit offsets/widths/masks, firmware header macros, and human-readable verification summaries. The generated per-device dispatch functions are linked into the simulator, demo and benchmark and are called by each peripheral model for register decoding and access-policy checks.

Generated output is kept under generated/. Handwritten behavioral models remain under model/. Existing-target base/size relocation is covered through the real TLM interconnect, and peripheral/IRQ target bounds use the generated region sizes; adding a new target still requires a C++ socket, binding and dispatch case.

## Reproducible submission sequence

    python3 -m venv .venv
    source .venv/bin/activate
    pip install -r requirements.txt

    cmake -S . -B build
    cmake --build build -j
    python generator/generate.py --check
    ctest --test-dir build --output-on-failure

    ./build/soc_sim --scenario boot --timing timed-lt
    ./build/soc_sim --scenario boot --timing functional-fast
    ./build/soc_bench 1000000

## Known scope

The CPU side is a scripted TLM initiator rather than a full RISC-V ISS. SPI and the IRQ controller are intentionally reduced functional models. The timing model is loosely timed; cycle-accurate behavior and an approximately-timed NoC are outside the baseline scope.

## Documentation

- docs/design_note.md — architecture, timing, schema, IRQ, DMI, reset, verification and scaling decisions.
- docs/benchmark_report.md — measured performance, DMI comparison and trace overhead.
- docs/requirements_traceability.md — assignment and feedback requirements mapped to implementation and automated evidence.

## License and third-party attribution

- Project source code and documentation: [MIT License](LICENSE).
- Third-party dependency and reference attribution: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- SystemC and PyYAML remain under their respective upstream licenses; they are external dependencies and are not bundled or relicensed by this project.
