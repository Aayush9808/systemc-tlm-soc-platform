from pathlib import Path
import shutil
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
        save(uart_path, data)

        original_spec_dir = generate.SPEC_DIR
        original_out_dir = generate.OUT_DIR
        generate.SPEC_DIR = spec_dir
        generate.OUT_DIR = out_dir

        try:
            soc_data, device_data = generate.validate_all()
            generate.generate_all(soc_data, device_data)
            header = (out_dir / "include" / "uart0.h").read_text()
            if "WDATA_OFFSET = 0x00000020" not in header:
                raise SystemExit("FAIL: relocated UART register offset was not generated")
            print("[PASS] UART register relocation regenerates register offsets")
        finally:
            generate.SPEC_DIR = original_spec_dir
            generate.OUT_DIR = original_out_dir


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
    print("[PASS] firmware references generated peripheral bases")


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

    print("[PASS] peripheral models use generated register offsets")


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
    check_firmware_uses_generated_bases()
    check_models_use_generated_register_offsets()

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