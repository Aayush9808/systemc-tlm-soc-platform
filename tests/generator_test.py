from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import yaml


ROOT = Path(__file__).resolve().parent.parent
SOURCE_SPECS = ROOT / "specs"

if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import generator.generate as generate


def expect_failure(name, mutate):
    with tempfile.TemporaryDirectory() as temp:
        spec_dir = Path(temp) / "specs"
        shutil.copytree(SOURCE_SPECS, spec_dir)

        mutate(spec_dir)

        original = generate.SPEC_DIR
        generate.SPEC_DIR = spec_dir

        try:
            try:
                generate.validate_all()
            except ValueError:
                print(f"[PASS] {name}")
                return

            print(f"[FAIL] {name}: validation unexpectedly passed")
            raise SystemExit(1)
        finally:
            generate.SPEC_DIR = original


def load(path):
    with path.open() as file:
        return yaml.safe_load(file)


def save(path, data):
    with path.open("w") as file:
        yaml.safe_dump(data, file, sort_keys=False)


def expect_address_relocation():
    with tempfile.TemporaryDirectory() as temp:
        temp_root = Path(temp)
        spec_dir = temp_root / "specs"
        out_dir = temp_root / "generated"
        shutil.copytree(SOURCE_SPECS, spec_dir)

        soc_path = spec_dir / "soc.yaml"
        data = load(soc_path)
        uart = next(entry for entry in data["memory_map"] if entry["name"] == "UART0")
        uart["base"] = "0x50000000"
        save(soc_path, data)

        original_spec_dir = generate.SPEC_DIR
        original_out_dir = generate.OUT_DIR
        generate.SPEC_DIR = spec_dir
        generate.OUT_DIR = out_dir

        try:
            soc_data, device_data = generate.validate_all()
            generate.generate_all(soc_data, device_data)
            header = (out_dir / "include" / "soc_memory_map.h").read_text()
            if "UART0_BASE = 0x50000000ULL" not in header:
                raise SystemExit("FAIL: relocated UART address was not generated")

            # Regeneration must preserve register metadata while relocating the
            # peripheral, and generated artifacts must be emitted as a set.
            metadata = (out_dir / "include" / "register_metadata.h").read_text()
            uart_header = (out_dir / "include" / "uart0.h").read_text()
            if "enum class RegisterAccess" not in metadata:
                raise SystemExit("FAIL: shared register metadata header was not generated")
            if '#include "register_metadata.h"' not in uart_header:
                raise SystemExit("FAIL: UART header does not include shared metadata")
            print("[PASS] UART address relocation regenerates memory map")
            print("[PASS] generated register metadata is emitted consistently")
        finally:
            generate.SPEC_DIR = original_spec_dir
            generate.OUT_DIR = original_out_dir


def expect_register_relocation():
    with tempfile.TemporaryDirectory() as temp:
        temp_root = Path(temp)
        spec_dir = temp_root / "specs"
        out_dir = temp_root / "generated"
        shutil.copytree(SOURCE_SPECS, spec_dir)

        uart_path = spec_dir / "uart.yaml"
        data = load(uart_path)
        wdata = next(reg for reg in data["registers"] if reg["name"] == "WDATA")
        wdata["offset"] = "0x20"
        wdata["reset"] = "0x1234"
        wdata["mask"] = "0xFF"
        save(uart_path, data)

        original_spec_dir = generate.SPEC_DIR
        original_out_dir = generate.OUT_DIR
        generate.SPEC_DIR = spec_dir
        generate.OUT_DIR = out_dir

        try:
            soc_data, device_data = generate.validate_all()
            generate.generate_all(soc_data, device_data)
            header = (out_dir / "include" / "uart0.h").read_text()
            if "WDATA_OFFSET = 0x00000020;" not in header:
                raise SystemExit("FAIL: relocated UART register offset was not generated")
            if "WDATA_RESET = 0x00001234;" not in header:
                raise SystemExit("FAIL: UART register reset metadata was not generated")
            if "WDATA_MASK = 0x000000FF;" not in header:
                raise SystemExit("FAIL: UART register mask metadata was not generated")
            if "RegisterAccess::WO" not in header:
                raise SystemExit("FAIL: UART register access metadata was not generated")
            firmware = (ROOT / "src" / "main.cpp").read_text()
            if "generated::uart0::WDATA_OFFSET" not in firmware:
                raise SystemExit(
                    "FAIL: firmware TX write does not consume generated UART WDATA offset"
                )
            print("[PASS] UART register relocation regenerates register offsets")
            print("[PASS] register reset, mask and access metadata follow YAML")
        finally:
            generate.SPEC_DIR = original_spec_dir
            generate.OUT_DIR = original_out_dir


def expect_generated_runtime_behavior():
    """
    Compile and execute metadata regenerated from mutated YAML.

    This closes the gap between checking generated text and proving that the
    generated C++ constants/lookup helpers expose the changed contract at
    runtime. It intentionally does not claim to execute the full SoC bus.
    """
    compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        raise SystemExit("FAIL: no C++ compiler available for generated runtime test")

    with tempfile.TemporaryDirectory() as temp:
        temp_root = Path(temp)
        spec_dir = temp_root / "specs"
        out_dir = temp_root / "generated"
        shutil.copytree(SOURCE_SPECS, spec_dir)

        soc_path = spec_dir / "soc.yaml"
        soc_data = load(soc_path)
        uart_range = next(
            entry for entry in soc_data["memory_map"]
            if entry["name"] == "UART0"
        )
        uart_range["base"] = "0x50000000"
        save(soc_path, soc_data)

        uart_path = spec_dir / "uart.yaml"
        uart_data = load(uart_path)
        wdata = next(reg for reg in uart_data["registers"] if reg["name"] == "WDATA")
        wdata["offset"] = "0x20"
        wdata["reset"] = "0x1234"
        wdata["mask"] = "0xFF"
        save(uart_path, uart_data)

        original_spec_dir = generate.SPEC_DIR
        original_out_dir = generate.OUT_DIR
        generate.SPEC_DIR = spec_dir
        generate.OUT_DIR = out_dir
        try:
            soc_data, device_data = generate.validate_all()
            generate.generate_all(soc_data, device_data)
        finally:
            generate.SPEC_DIR = original_spec_dir
            generate.OUT_DIR = original_out_dir

        probe = temp_root / "generated_contract_probe.cpp"
        probe.write_text(r'''#include <cstdint>
#include "soc_memory_map.h"
#include "uart0.h"

int main() {
    using namespace generated;

    if (UART0_BASE != 0x50000000ULL) return 1;

    bool found_uart = false;
    for (unsigned int i = 0; i < MEMORY_MAP_COUNT; ++i) {
        const auto& range = MEMORY_MAP[i];
        if (range.target == TargetId::UART0) {
            found_uart = true;
            if (range.base != 0x50000000ULL || range.size != 0x1000ULL)
                return 2;
            const uint64_t transaction_address = range.base + uart0::WDATA_OFFSET;
            if (transaction_address != 0x50000020ULL) return 3;
            if (transaction_address < range.base ||
                transaction_address >= range.base + range.size)
                return 4;
        }
    }
    if (!found_uart) return 5;

    const auto* reg = lookup_register(
        uart0::REGISTERS, uart0::REGISTER_COUNT, 0x20
    );
    if (reg == nullptr) return 6;
    if (reg->reset != 0x1234 || reg->mask != 0xFF) return 7;
    if (reg->access != RegisterAccess::WO) return 8;
    if (!register_write_allowed(*reg) || register_read_allowed(*reg)) return 9;
    if (apply_register_mask(*reg, 0xABCD) != 0xCD) return 10;

    if (lookup_register(
            uart0::REGISTERS, uart0::REGISTER_COUNT, 0x0C) != nullptr)
        return 11;

    return 0;
}
''')
        binary = temp_root / "generated_contract_probe"
        build = subprocess.run(
            [
                compiler, "-std=c++17",
                "-I", str(out_dir / "include"),
                str(probe), "-o", str(binary),
            ],
            capture_output=True,
            text=True,
        )
        if build.returncode != 0:
            raise SystemExit(
                "FAIL: generated runtime probe did not compile:\\n"
                + build.stdout + build.stderr
            )

        run = subprocess.run(
            [str(binary)],
            capture_output=True,
            text=True,
        )
        if run.returncode != 0:
            raise SystemExit(
                "FAIL: generated runtime probe failed with exit code "
                + str(run.returncode) + "\\n"
                + run.stdout + run.stderr
            )

    print("[PASS] mutated YAML -> generated C++ -> compiled runtime contract")

def check_firmware_uses_generated_bases():
    source = (ROOT / "src" / "main.cpp").read_text()
    required = [
        "generated::UART0_BASE",
        "generated::GPIO_BASE",
        "generated::RV_TIMER_BASE",
        "generated::SPI_DEVICE_BASE",
        "generated::IRQ_BASE",
    ]
    missing = [name for name in required if name not in source]
    if missing:
        raise SystemExit(
            "FAIL: firmware is missing generated address constants: "
            + ", ".join(missing)
        )

    register_symbols = [
        "generated::uart0::CTRL_OFFSET",
        "generated::uart0::WDATA_OFFSET",
        "generated::uart0::INTR_STATE_OFFSET",
        "generated::uart0::INTR_ENABLE_OFFSET",
        "generated::gpio::DIRECT_OUT_OFFSET",
        "generated::gpio::DIRECT_OE_OFFSET",
        "generated::gpio::MASKED_OUT_LOWER_OFFSET",
        "generated::gpio::MASKED_OUT_UPPER_OFFSET",
        "generated::gpio::INTR_STATE_OFFSET",
        "generated::gpio::INTR_ENABLE_OFFSET",
        "generated::gpio::INTR_RISE_OFFSET",
        "generated::rv_timer::CTRL_OFFSET",
        "generated::rv_timer::CFG0_OFFSET",
        "generated::rv_timer::TIMER_V_LOWER_OFFSET",
        "generated::rv_timer::TIMER_V_UPPER_OFFSET",
        "generated::rv_timer::COMPARE_LOWER_OFFSET",
        "generated::rv_timer::COMPARE_UPPER_OFFSET",
        "generated::rv_timer::INTR_STATE_OFFSET",
        "generated::rv_timer::INTR_ENABLE_OFFSET",
        "generated::spi_device::CONTROL_OFFSET",
        "generated::spi_device::CFG_OFFSET",
        "generated::spi_device::STATUS_OFFSET",
        "generated::spi_device::TX_OFFSET",
        "generated::spi_device::RX_OFFSET",
        "generated::irq::PENDING_OFFSET",
        "generated::irq::ENABLE_OFFSET",
        "generated::irq::CLAIM_OFFSET",
        "generated::irq::COMPLETE_OFFSET",
    ]
    missing_symbols = [name for name in register_symbols if name not in source]
    if missing_symbols:
        raise SystemExit(
            "FAIL: firmware is missing generated register offsets: "
            + ", ".join(missing_symbols)
        )

    # A register offset edited in YAML must flow into the actual firmware
    # transactions, not only into a generated metadata header.
    for base in ("UART0", "GPIO", "RV_TIMER", "SPI_DEVICE"):
        if re.search(rf"generated::{base}_BASE\s*\+\s*0x[0-9A-Fa-f]+ULL", source):
            raise SystemExit(
                f"FAIL: firmware still hardcodes a {base} register offset"
            )
    print("[PASS] firmware uses generated peripheral bases and register offsets")


def check_models_use_generated_register_offsets():
    models = {
        "uart.h": ("uart0", ["CTRL_OFFSET", "WDATA_OFFSET", "INTR_STATE_OFFSET"]),
        "gpio.h": ("gpio", ["DATA_IN_OFFSET", "DIRECT_OUT_OFFSET", "INTR_STATE_OFFSET"]),
        "rv_timer.h": ("rv_timer", ["CTRL_OFFSET", "TIMER_V_LOWER_OFFSET", "INTR_STATE_OFFSET"]),
        "spi_device.h": ("spi_device", ["CONTROL_OFFSET", "TX_OFFSET", "RX_OFFSET"]),
    }

    for filename, (namespace, symbols) in models.items():
        source = (ROOT / "model" / "peripherals" / filename).read_text()
        if f"../../generated/include/{filename.replace('uart.h', 'uart0.h')}" not in source and filename == "uart.h":
            raise SystemExit("FAIL: UART model does not include generated register definitions")
        if filename != "uart.h" and f"../../generated/include/{filename}" not in source:
            raise SystemExit(f"FAIL: {filename} does not include its generated register definitions")
        for symbol in symbols:
            reference = f"generated::{namespace}::{symbol}"
            if reference not in source:
                raise SystemExit(f"FAIL: {filename} is missing generated register reference {reference}")

    irq_source = (ROOT / "model" / "irq" / "irq_controller.h").read_text()
    for symbol in [
        "generated::irq::PENDING_OFFSET",
        "generated::irq::ENABLE_OFFSET",
        "generated::irq::CLAIM_OFFSET",
        "generated::irq::COMPLETE_OFFSET",
    ]:
        if symbol not in irq_source:
            raise SystemExit(
                f"FAIL: IRQ controller is missing generated register reference {symbol}"
            )

    print("[PASS] peripheral models use generated register offsets")
    print("[PASS] IRQ controller uses generated register offsets")


def check_models_enforce_generated_register_policy():
    model_files = [
        "model/peripherals/uart.h",
        "model/peripherals/gpio.h",
        "model/peripherals/rv_timer.h",
        "model/peripherals/spi_device.h",
        "model/irq/irq_controller.h",
    ]
    required_helpers = [
        "generated::lookup_register(",
        "generated::register_read_allowed(",
        "generated::register_write_allowed(",
        "generated::apply_register_mask(",
    ]
    for filename in model_files:
        source = (ROOT / filename).read_text()
        missing = [helper for helper in required_helpers if helper not in source]
        if missing:
            raise SystemExit(
                f"FAIL: {filename} does not enforce generated register metadata: "
                + ", ".join(missing)
            )

    uart_spec = load(SOURCE_SPECS / "uart.yaml")
    ctrl = next(reg for reg in uart_spec["registers"] if reg["name"] == "CTRL")
    if ctrl.get("mask") != "0x00000003" and ctrl.get("mask") != 3:
        raise SystemExit("FAIL: UART CTRL mask test fixture must limit writable bits to 0x3")
    uart_header = (ROOT / "generated" / "include" / "uart0.h").read_text()
    if "CTRL_MASK = 0x00000003" not in uart_header:
        raise SystemExit("FAIL: generated UART CTRL mask does not match YAML")
    print("[PASS] all target models enforce generated register access and masks")


def check_models_use_generated_reset_values():
    model_requirements = {
        "model/peripherals/uart.h": [
            "generated::uart0::CTRL_RESET",
            "generated::uart0::INTR_STATE_RESET",
            "generated::uart0::INTR_ENABLE_RESET",
        ],
        "model/peripherals/gpio.h": [
            "generated::gpio::DATA_IN_RESET",
            "generated::gpio::DIRECT_OUT_RESET",
            "generated::gpio::DIRECT_OE_RESET",
            "generated::gpio::INTR_STATE_RESET",
            "generated::gpio::INTR_ENABLE_RESET",
            "generated::gpio::INTR_RISE_RESET",
            "generated::gpio::INTR_FALL_RESET",
        ],
        "model/peripherals/rv_timer.h": [
            "generated::rv_timer::CTRL_RESET",
            "generated::rv_timer::CFG0_RESET",
            "generated::rv_timer::TIMER_V_LOWER_RESET",
            "generated::rv_timer::TIMER_V_UPPER_RESET",
            "generated::rv_timer::COMPARE_LOWER_RESET",
            "generated::rv_timer::COMPARE_UPPER_RESET",
            "generated::rv_timer::INTR_STATE_RESET",
            "generated::rv_timer::INTR_ENABLE_RESET",
        ],
        "model/peripherals/spi_device.h": [
            "generated::spi_device::CONTROL_RESET",
            "generated::spi_device::CFG_RESET",
        ],
        "model/irq/irq_controller.h": [
            "generated::irq::PENDING_RESET",
            "generated::irq::ENABLE_RESET",
        ],
    }

    for filename, symbols in model_requirements.items():
        source = (ROOT / filename).read_text()
        missing = [symbol for symbol in symbols if symbol not in source]
        if missing:
            raise SystemExit(
                f"FAIL: {filename} does not consume generated reset metadata: "
                + ", ".join(missing)
            )

    print("[PASS] peripheral and IRQ reset values consume generated YAML metadata")


def check_irq_w1c_metadata():
    spec = load(SOURCE_SPECS / "irq.yaml")
    pending = next(reg for reg in spec["registers"] if reg["name"] == "PENDING")
    if pending["access"] != "W1C":
        raise SystemExit("FAIL: IRQ PENDING must be declared W1C in YAML")

    header = (ROOT / "generated" / "include" / "irq.h").read_text()
    if 'PENDING_ACCESS = "W1C"' not in header:
        raise SystemExit("FAIL: generated IRQ metadata lost PENDING W1C access")
    if '{"PENDING", 0x00000000, 0x00000000, 0xFFFFFFFF, RegisterAccess::W1C}' not in header:
        raise SystemExit("FAIL: IRQ PENDING metadata is not generated as W1C")

    controller = (ROOT / "model" / "irq" / "irq_controller.h").read_text()
    if "pending &= ~value;" not in controller:
        raise SystemExit("FAIL: IRQ PENDING W1C behavior is missing")
    print("[PASS] IRQ PENDING W1C metadata matches the model behavior")


def main():
    expect_failure(
        "overlapping memory ranges",
        lambda spec_dir: overlap(spec_dir),
    )

    expect_failure(
        "invalid register access",
        lambda spec_dir: invalid_access(spec_dir),
    )

    expect_failure(
        "unaligned register offset",
        lambda spec_dir: unaligned_offset(spec_dir),
    )

    expect_failure(
        "register offset outside 32-bit range",
        lambda spec_dir: out_of_range_offset(spec_dir),
    )

    expect_failure(
        "invalid C++ register identifier",
        lambda spec_dir: invalid_register_identifier(spec_dir),
    )

    expect_failure(
        "invalid C++ memory-map identifier",
        lambda spec_dir: invalid_memory_map_identifier(spec_dir),
    )

    expect_failure(
        "duplicate register name",
        lambda spec_dir: duplicate_register_name(spec_dir),
    )

    expect_failure(
        "duplicate register offset",
        lambda spec_dir: duplicate_register_offset(spec_dir),
    )

    expect_failure(
        "out-of-range reset value",
        lambda spec_dir: invalid_reset(spec_dir),
    )

    expect_failure(
        "out-of-range register mask",
        lambda spec_dir: invalid_mask(spec_dir),
    )

    expect_address_relocation()
    expect_register_relocation()
    expect_generated_runtime_behavior()
    check_firmware_uses_generated_bases()
    check_models_use_generated_register_offsets()
    check_models_use_generated_reset_values()
    check_models_enforce_generated_register_policy()
    check_irq_w1c_metadata()

    print("GENERATOR VALIDATION TEST: PASS")


def overlap(spec_dir):
    path = spec_dir / "soc.yaml"
    data = load(path)
    data["memory_map"][1]["base"] = data["memory_map"][0]["base"]
    save(path, data)


def invalid_access(spec_dir):
    path = spec_dir / "uart.yaml"
    data = load(path)
    data["registers"][0]["access"] = "INVALID"
    save(path, data)


def unaligned_offset(spec_dir):
    path = spec_dir / "gpio.yaml"
    data = load(path)
    data["registers"][0]["offset"] = 2
    save(path, data)


def out_of_range_offset(spec_dir):
    path = spec_dir / "uart.yaml"
    data = load(path)
    data["registers"][0]["offset"] = "0x100000000"
    save(path, data)


def invalid_register_identifier(spec_dir):
    path = spec_dir / "uart.yaml"
    data = load(path)
    data["registers"][0]["name"] = "CTRL-REGISTER"
    save(path, data)


def invalid_memory_map_identifier(spec_dir):
    path = spec_dir / "soc.yaml"
    data = load(path)
    data["memory_map"][0]["name"] = "ROM-BLOCK"
    save(path, data)


def duplicate_register_name(spec_dir):
    path = spec_dir / "uart.yaml"
    data = load(path)
    data["registers"][1]["name"] = data["registers"][0]["name"]
    save(path, data)


def duplicate_register_offset(spec_dir):
    path = spec_dir / "uart.yaml"
    data = load(path)
    data["registers"][1]["offset"] = data["registers"][0]["offset"]
    save(path, data)


def invalid_reset(spec_dir):
    path = spec_dir / "uart.yaml"
    data = load(path)
    data["registers"][0]["reset"] = "0x100000000"
    save(path, data)


def invalid_mask(spec_dir):
    path = spec_dir / "gpio.yaml"
    data = load(path)
    data["registers"][0]["mask"] = "0x100000000"
    save(path, data)


if __name__ == "__main__":
    main()