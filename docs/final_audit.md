# Final Audit Notes

Date: 2026-10-11

## Changes made

- GPIO registers declared in YAML but without bespoke GPIO side effects now have reset-initialized runtime backing storage. The fallback path applies the generated register mask, supports read/write policy enforcement, implements generic write-one-to-clear behavior, and restores YAML reset values on GPIO reset.
- Generator validation now rejects unknown memory-map region types, peripheral specifications without a matching peripheral memory-map entry, and 32-bit registers that do not fit entirely inside their mapped region.
- Added a SystemC runtime regression probe to the generator test suite that mutates GPIO YAML to add RW and W1C registers and checks reset, mask, read/write and reset behavior. This test is committed in the deliverable but could not be executed in this sandbox because SystemC is not installed here.
- The standalone TLM demo now uses generated map/register constants instead of hardcoded peripheral addresses, and its DMI check determines the process exit status so the CTest entry cannot report success after a failed DMI coherence check.
- CMake now searches common SystemC install paths and `SYSTEMC_HOME`, while preserving the explicit `SYSTEMC_ROOT` override.
- README schema/source-of-truth notes updated.

## Tests actually executed in this environment

- `python3 generator/generate.py --check` — PASS; checked-in generated artifacts match the YAML schemas.
- Python syntax compilation for generator and test scripts — PASS.
- 15 focused pure-Python / generated-C++ checks — PASS. These include invalid memory-map type rejection, peripheral register bounds, overlapping range rejection, invalid access rejection, register relocation, added-register artifact generation, field generation, compiling and running the generated C++ metadata contract, memory-map relocation, generated address use in firmware, generated memory-range/register-policy/reset usage in models, and IRQ W1C metadata.
- CMake configure — BLOCKED/FAIL in this environment because no SystemC header/library is installed (`systemc` include not found). No C++ SoC targets or full CTest suite could be built or run here.
- The full `tests/generator_test.py` and `tests/image_error_test.py` suites require `SYSTEMC_INCLUDE_DIR` and `SYSTEMC_LIBRARY`; those integration/runtime tests therefore remain unverified in this sandbox.

## Follow-up verification on the project owner's Mac (2026-10-11)

After the deliverable audit, the owner built the project with Homebrew SystemC 3.0.2. The Release CMake configure and build passed, all 6 CTest tests passed, and both `soc_sim --scenario boot --timing timed-lt` and `soc_sim --scenario boot --timing functional-fast` completed with PASS. The one-million-iteration `soc_bench` run also passed. The GPIO YAML runtime regression is part of `generator_validation`, so it was exercised as part of that 6/6 CTest run. `python3 generator/generate.py --check` also passed. The earlier sandbox-only limitation below describes the separate audit container, not the owner's Mac verification.

## Verification status and remaining scope

The audit container itself could not build or run the SystemC-dependent targets because SystemC was not installed there. The separate owner-Mac verification recorded above reports a successful Release build, all 6 CTest tests, both boot scenarios, the one-million-iteration benchmark, and generated-artifact consistency using Homebrew SystemC 3.0.2. These are distinct environments; do not interpret the sandbox limitation as a failure of the Mac run or claim that the sandbox reproduced it.

Known project scope limitations remain documented in README/design notes: scripted initiator rather than a RISC-V ISS, simplified SPI/IRQ models, loosely timed rather than cycle-accurate behavior, and hand-written socket/binding integration when adding an entirely new target.
