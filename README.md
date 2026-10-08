# SystemC / TLM-2.0 SoC Platform

A reusable virtual SoC platform built with SystemC 3.0.2 and TLM-2.0.

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

All firmware-visible accesses go through the memory-mapped interconnect. The interconnect uses the generated memory map, validates the complete address range, translates to target-local addresses, and forwards TLM transactions.

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
- Optional transaction tracing and per-target counters.
- Functional-fast and timed-LT modes.
- Event-driven simulated-time timer.
- UART, GPIO, timer, SPI and IRQ behavior.
- Deterministic reset after simulated time advances.
- YAML source-of-truth schema and deterministic code generation.
- Generated register metadata, map constants, software headers and verification artifacts.
- Unit/negative/integration verification and performance measurement.

## Prerequisites

- C++17 compiler
- CMake 3.20+
- Python 3.10+
- SystemC 3.0.2
- PyYAML

Example:

    python3 -m venv .venv
    source .venv/bin/activate
    pip install -r requirements.txt

If SystemC is installed outside the Homebrew default:

    cmake -S . -B build -DSYSTEMC_ROOT=/path/to/systemc

## Build and generate

    cmake -S . -B build
    cmake --build build -j
    python generator/generate.py
    python generator/generate.py --check

## Verification

    ctest --test-dir build --output-on-failure

    ./build/soc_sim --scenario boot --timing timed-lt
    ./build/soc_sim --scenario boot --timing functional-fast
    ./build/tlm_demo

The integrated scenario checks ROM-to-SRAM copy, GPIO direct/masked access, invalid transactions, UART IRQ, GPIO edge IRQ, timer compare IRQ, SPI exchange/IRQ, reset and deterministic PASS/FAIL behavior.

The TLM policy test covers valid access, unaligned access, invalid length, invalid streaming width, byte enables, unsupported commands and out-of-bounds access.

## Performance

    ./build/soc_bench 1000000
    ./build/soc_bench 5000 --trace

Recorded measurements:
- Mixed 1M-iteration run: 2,006,000 bus transactions, 0.268628 s host runtime, 7,467,578 transactions/s, 20,060 us simulated time.
- Separate 1M-operation DMI experiment: approximately 253.6x speedup over the corresponding TLM workload.
- 5,000-iteration trace experiment: approximately 4.57x host-runtime overhead with tracing enabled.

See docs/benchmark_report.md.

## Timing

timed-lt: target delays are annotated and the initiator waits for them.

functional-fast: the same functional scenario is exercised without waiting on each target delay.

The timer uses SystemC simulated time, not host wall-clock time.

## Schema and generation

specs/soc.yaml is the memory-map source of truth. Device YAML files define register metadata.

The generator validates duplicate names/offsets, alignment, access types, reset/mask ranges, overlapping memory ranges and uint64 address overflow.

Generated output is kept under generated/. Handwritten behavioral models remain under model/.

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

## License and third-party attribution

- Project source code and documentation: [MIT License](LICENSE).
- Third-party dependency and reference attribution: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- SystemC and PyYAML remain under their respective upstream licenses; they are external dependencies and are not bundled or relicensed by this project.
