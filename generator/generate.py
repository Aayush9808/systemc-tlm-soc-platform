from pathlib import Path
import re
import sys
import tempfile
import yaml


ROOT = Path(__file__).resolve().parent.parent
SPEC_DIR = ROOT / "specs"
OUT_DIR = ROOT / "generated"

VALID_ACCESS = {"RO", "RW", "WO", "W1C"}
IDENTIFIER_PATTERN = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def is_cpp_identifier(value):
    return isinstance(value, str) and IDENTIFIER_PATTERN.fullmatch(value) is not None


def load_yaml(path):
    with open(path, "r") as file:
        data = yaml.safe_load(file)

    if not isinstance(data, dict):
        raise ValueError(f"{path}: root must be a mapping")

    return data


def numeric_value(value, field_name):
    if isinstance(value, int):
        return value

    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError:
            raise ValueError(
                f"{field_name}: expected integer or hexadecimal value, "
                f"got '{value}'"
            )

    raise ValueError(f"{field_name}: expected integer")


def hex_value(value):
    return f"0x{value:08X}"


def validate_soc(data, path):
    if "soc" not in data:
        raise ValueError(f"{path}: missing 'soc' section")

    if "memory_map" not in data:
        raise ValueError(f"{path}: missing 'memory_map' section")

    if not isinstance(data["memory_map"], list):
        raise ValueError(f"{path}: 'memory_map' must be a list")

    ranges = []
    names = set()

    for index, entry in enumerate(data["memory_map"]):
        prefix = f"{path}: memory_map[{index}]"

        if not isinstance(entry, dict):
            raise ValueError(f"{prefix}: must be a mapping")

        for field in ("name", "base", "size", "type"):
            if field not in entry:
                raise ValueError(f"{prefix}: missing '{field}'")

        name = entry["name"]
        base = numeric_value(entry["base"], f"{prefix}.base")
        size = numeric_value(entry["size"], f"{prefix}.size")

        if not is_cpp_identifier(name):
            raise ValueError(
                f"{prefix}: name must be a valid C++ identifier"
            )

        if not isinstance(entry["type"], str) or not entry["type"].strip():
            raise ValueError(f"{prefix}: type must be a non-empty string")

        if name in names:
            raise ValueError(
                f"{prefix}: duplicate memory-map name '{name}'"
            )

        if size <= 0:
            raise ValueError(
                f"{prefix}: size must be greater than zero"
            )

        if base < 0:
            raise ValueError(
                f"{prefix}: base cannot be negative"
            )

        end = base + size

        if end > 0xFFFFFFFFFFFFFFFF:
            raise ValueError(
                f"{prefix}: address range overflows uint64"
            )

        ranges.append((base, end, name))
        names.add(name)

    for i in range(len(ranges)):
        base_a, end_a, name_a = ranges[i]

        for j in range(i + 1, len(ranges)):
            base_b, end_b, name_b = ranges[j]

            if base_a < end_b and base_b < end_a:
                raise ValueError(
                    f"{path}: overlapping memory ranges: "
                    f"{name_a} and {name_b}"
                )


def validate_device(data, path):
    if "device" not in data:
        raise ValueError(f"{path}: missing 'device' section")

    if "registers" not in data:
        raise ValueError(f"{path}: missing 'registers' section")

    device = data["device"]

    if not isinstance(device, dict):
        raise ValueError(
            f"{path}: 'device' must be a mapping"
        )

    if "name" not in device:
        raise ValueError(
            f"{path}: device missing 'name'"
        )

    if not is_cpp_identifier(device["name"]):
        raise ValueError(
            f"{path}: device name must be a valid C++ identifier"
        )

    registers = data["registers"]

    if not isinstance(registers, list):
        raise ValueError(
            f"{path}: 'registers' must be a list"
        )

    names = set()
    offsets = set()

    for index, reg in enumerate(registers):
        prefix = f"{path}: registers[{index}]"

        if not isinstance(reg, dict):
            raise ValueError(
                f"{prefix}: must be a mapping"
            )

        for field in ("name", "offset", "access"):
            if field not in reg:
                raise ValueError(
                    f"{prefix}: missing '{field}'"
                )

        name = reg["name"]
        offset = numeric_value(
            reg["offset"],
            f"{prefix}.offset"
        )
        access = reg["access"]

        if not is_cpp_identifier(name):
            raise ValueError(
                f"{prefix}: name must be a valid C++ identifier"
            )

        if name in names:
            raise ValueError(
                f"{path}: duplicate register name '{name}'"
            )

        if offset in offsets:
            raise ValueError(
                f"{path}: duplicate register offset "
                f"{hex_value(offset)}"
            )

        if offset < 0:
            raise ValueError(
                f"{prefix}: offset cannot be negative"
            )

        if offset > 0xFFFFFFFF:
            raise ValueError(
                f"{prefix}: offset must fit in 32 bits"
            )

        if offset % 4 != 0:
            raise ValueError(
                f"{prefix}: offset {hex_value(offset)} "
                f"is not 4-byte aligned"
            )

        if access not in VALID_ACCESS:
            raise ValueError(
                f"{prefix}: invalid access '{access}'. "
                f"Expected one of {sorted(VALID_ACCESS)}"
            )

        if "reset" in reg:
            reset = numeric_value(
                reg["reset"],
                f"{prefix}.reset"
            )

            if reset < 0 or reset > 0xFFFFFFFF:
                raise ValueError(
                    f"{prefix}.reset: must fit in 32 bits"
                )

        if "mask" in reg:
            mask = numeric_value(
                reg["mask"],
                f"{prefix}.mask"
            )

            if mask < 0 or mask > 0xFFFFFFFF:
                raise ValueError(
                    f"{prefix}.mask: must fit in 32 bits"
                )

        names.add(name)
        offsets.add(offset)


def validate_all():
    soc_path = SPEC_DIR / "soc.yaml"
    soc_data = load_yaml(soc_path)
    validate_soc(soc_data, soc_path)

    device_files = [
        "uart.yaml",
        "gpio.yaml",
        "rv_timer.yaml",
        "spi_device.yaml",
        "irq.yaml",
    ]

    device_data = {}

    for filename in device_files:
        path = SPEC_DIR / filename
        data = load_yaml(path)
        validate_device(data, path)
        device_data[filename] = data

    return soc_data, device_data


def generate_soc_map(data):
    output = OUT_DIR / "include" / "soc_memory_map.h"
    output.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace generated {",
        "",
        "enum class TargetId {",
    ]

    target_names = []

    for entry in data["memory_map"]:
        target_names.append(entry["name"])

    for name in target_names:
        lines.append(f"    {name},")

    lines.extend([
        "};",
        "",
        "struct MemoryRange {",
        "    const char* name;",
        "    uint64_t base;",
        "    uint64_t size;",
        "    TargetId target;",
        "};",
        "",
    ])

    for entry in data["memory_map"]:
        name = entry["name"]

        base = numeric_value(
            entry["base"],
            f"{name}.base"
        )

        size = numeric_value(
            entry["size"],
            f"{name}.size"
        )

        lines.append(
            f"constexpr uint64_t {name}_BASE = "
            f"{hex_value(base)}ULL;"
        )

        lines.append(
            f"constexpr uint64_t {name}_SIZE = "
            f"{hex_value(size)}ULL;"
        )

        lines.append("")

    lines.append(
        "constexpr MemoryRange MEMORY_MAP[] = {"
    )

    for name in target_names:
        lines.append(
            f'    {{"{name}", {name}_BASE, {name}_SIZE, '
            f"TargetId::{name}}},"
        )

    lines.extend([
        "};",
        "",
        "constexpr unsigned int MEMORY_MAP_COUNT =",
        "    sizeof(MEMORY_MAP) / sizeof(MEMORY_MAP[0]);",
        "",
        "}",
        "",
    ])

    output.write_text("\n".join(lines))
    print(f"generated: {output}")



def generate_register_metadata_header():
    output = OUT_DIR / "include" / "register_metadata.h"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join([
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace generated {",
        "",
        "enum class RegisterAccess {",
        "    RO,",
        "    RW,",
        "    WO,",
        "    W1C",
        "};",
        "",
        "struct RegisterMetadata {",
        "    const char* name;",
        "    uint32_t offset;",
        "    uint32_t reset;",
        "    uint32_t mask;",
        "    RegisterAccess access;",
        "};",
        "",
        "inline const RegisterMetadata* lookup_register(",
        "    const RegisterMetadata* registers,",
        "    unsigned int count,",
        "    uint32_t offset) {",
        "    for (unsigned int i = 0; i < count; ++i) {",
        "        if (registers[i].offset == offset)",
        "            return &registers[i];",
        "    }",
        "    return nullptr;",
        "}",
        "",
        "inline bool register_read_allowed(const RegisterMetadata& reg) {",
        "    return reg.access == RegisterAccess::RO ||",
        "           reg.access == RegisterAccess::RW ||",
        "           reg.access == RegisterAccess::W1C;",
        "}",
        "",
        "inline bool register_write_allowed(const RegisterMetadata& reg) {",
        "    return reg.access == RegisterAccess::RW ||",
        "           reg.access == RegisterAccess::WO ||",
        "           reg.access == RegisterAccess::W1C;",
        "}",
        "",
        "inline uint32_t apply_register_mask(",
        "    const RegisterMetadata& reg, uint32_t value) {",
        "    return value & reg.mask;",
        "}",
        "",
        "inline uint32_t load_register_value(const unsigned char* data) {",
        "    return static_cast<uint32_t>(data[0]) |",
        "           (static_cast<uint32_t>(data[1]) << 8) |",
        "           (static_cast<uint32_t>(data[2]) << 16) |",
        "           (static_cast<uint32_t>(data[3]) << 24);",
        "}",
        "",
        "inline void store_register_value(unsigned char* data, uint32_t value) {",
        "    data[0] = value & 0xff;",
        "    data[1] = (value >> 8) & 0xff;",
        "    data[2] = (value >> 16) & 0xff;",
        "    data[3] = (value >> 24) & 0xff;",
        "}",
        "",
        "}",
        "",
    ]))
    print(f"generated: {output}")


def generate_device_header(data):
    device_name = data["device"]["name"]
    namespace_name = device_name.lower()

    output = (
        OUT_DIR /
        "include" /
        f"{namespace_name}.h"
    )

    output.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        '#include "register_metadata.h"',
        "",
        "namespace generated {",
        "",
        f"namespace {namespace_name} {{",
        "",
    ]

    for reg in data["registers"]:
        reg_name = reg["name"]

        offset = numeric_value(
            reg["offset"],
            f"{device_name}.{reg_name}.offset"
        )

        reset = numeric_value(
            reg.get("reset", 0),
            f"{device_name}.{reg_name}.reset"
        )

        mask = numeric_value(
            reg.get("mask", 0xFFFFFFFF),
            f"{device_name}.{reg_name}.mask"
        )

        access = reg["access"]

        lines.append(
            f"constexpr uint32_t {reg_name}_OFFSET = "
            f"{hex_value(offset)};"
        )

        lines.append(
            f"constexpr uint32_t {reg_name}_RESET = "
            f"{hex_value(reset)};"
        )

        lines.append(
            f"constexpr uint32_t {reg_name}_MASK = "
            f"{hex_value(mask)};"
        )

        lines.append(
            f'constexpr const char* {reg_name}_ACCESS = '
            f'"{access}";'
        )

        lines.append("")

    lines.append(
        "constexpr RegisterMetadata REGISTERS[] = {"
    )

    for reg in data["registers"]:
        reg_name = reg["name"]

        offset = numeric_value(
            reg["offset"],
            f"{device_name}.{reg_name}.offset"
        )

        reset = numeric_value(
            reg.get("reset", 0),
            f"{device_name}.{reg_name}.reset"
        )

        mask = numeric_value(
            reg.get("mask", 0xFFFFFFFF),
            f"{device_name}.{reg_name}.mask"
        )

        access = reg["access"]

        lines.append(
            f'    {{"{reg_name}", '
            f"{hex_value(offset)}, "
            f"{hex_value(reset)}, "
            f"{hex_value(mask)}, "
            f"RegisterAccess::{access}}},"
        )

    lines.extend([
        "};",
        "",
        "constexpr unsigned int REGISTER_COUNT =",
        "    sizeof(REGISTERS) / sizeof(REGISTERS[0]);",
        "",
        "}",
        "}",
        "",
    ])

    output.write_text("\n".join(lines))
    print(f"generated: {output}")


def generate_dispatch_source(data):
    device_name = data["device"]["name"]
    namespace_name = device_name.lower()

    output = (
        OUT_DIR /
        "src" /
        f"{namespace_name}_registers.cpp"
    )

    output.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        f'#include "../include/{namespace_name}.h"',
        "",
        "namespace generated {",
        "",
        f"namespace {namespace_name} {{",
        "",
        "const RegisterMetadata* find_register(",
        "    uint32_t offset) {",
        "",
        "    for (unsigned int i = 0;",
        "         i < REGISTER_COUNT;",
        "         ++i) {",
        "",
        "        if (REGISTERS[i].offset == offset)",
        "            return &REGISTERS[i];",
        "    }",
        "",
        "    return nullptr;",
        "}",
        "",
        "bool allows_read(const RegisterMetadata& reg) {",
        "    return reg.access == RegisterAccess::RO ||",
        "           reg.access == RegisterAccess::RW ||",
        "           reg.access == RegisterAccess::W1C;",
        "}",
        "",
        "bool allows_write(const RegisterMetadata& reg) {",
        "    return reg.access == RegisterAccess::RW ||",
        "           reg.access == RegisterAccess::WO ||",
        "           reg.access == RegisterAccess::W1C;",
        "}",
        "",
        "}",
        "}",
        "",
    ]

    output.write_text("\n".join(lines))
    print(f"generated: {output}")


def generate_software_header(data):
    device_name = data["device"]["name"]
    name = device_name.lower()

    output = OUT_DIR / "sw" / f"{name}.h"
    output.parent.mkdir(parents=True, exist_ok=True)

    guard_name = f"GENERATED_{name.upper()}_SW_H"

    lines = [
        f"#ifndef {guard_name}",
        f"#define {guard_name}",
        "",
        "#include <stdint.h>",
        "",
    ]

    for reg in data["registers"]:
        reg_name = reg["name"]

        offset = numeric_value(
            reg["offset"],
            f"{device_name}.{reg_name}.offset"
        )

        mask = numeric_value(
            reg.get("mask", 0xFFFFFFFF),
            f"{device_name}.{reg_name}.mask"
        )

        prefix = name.upper()

        lines.append(
            f"#define {prefix}_{reg_name}_OFFSET "
            f"{hex_value(offset)}"
        )

        lines.append(
            f"#define {prefix}_{reg_name}_MASK "
            f"{hex_value(mask)}"
        )

    lines.extend([
        "",
        "#endif",
        "",
    ])

    output.write_text("\n".join(lines))
    print(f"generated: {output}")


def generate_device_docs(data):
    device_name = data["device"]["name"]
    name = device_name.lower()

    output = OUT_DIR / "docs" / f"{name}.md"
    output.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        f"# {device_name}",
        "",
        f"- Version: "
        f"{data['device'].get('version', 'unknown')}",
        f"- Address width: "
        f"{data['device'].get('address_width', 'unknown')}",
        f"- Register width: "
        f"{data['device'].get('register_width', 'unknown')}",
        "",
        "| Register | Offset | Access | Reset | Mask |",
        "|---|---:|---|---:|---:|",
    ]

    for reg in data["registers"]:
        offset = numeric_value(
            reg["offset"],
            f"{device_name}.{reg['name']}.offset"
        )

        reset = numeric_value(
            reg.get("reset", 0),
            f"{device_name}.{reg['name']}.reset"
        )

        mask = numeric_value(
            reg.get("mask", 0xFFFFFFFF),
            f"{device_name}.{reg['name']}.mask"
        )

        lines.append(
            f"| {reg['name']} | "
            f"{hex_value(offset)} | "
            f"{reg['access']} | "
            f"{hex_value(reset)} | "
            f"{hex_value(mask)} |"
        )

    lines.append("")

    output.write_text("\n".join(lines))
    print(f"generated: {output}")


def generate_verification_artifact(data):
    device_name = data["device"]["name"]
    name = device_name.lower()

    output = (
        OUT_DIR /
        "docs" /
        f"{name}_verification.md"
    )

    output.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        f"# {device_name} Verification Artifact",
        "",
        "Generated from the device schema.",
        "",
        "## Register checks",
        "",
    ]

    for reg in data["registers"]:
        offset = numeric_value(
            reg["offset"],
            f"{device_name}.{reg['name']}.offset"
        )

        reset = numeric_value(
            reg.get("reset", 0),
            f"{device_name}.{reg['name']}.reset"
        )

        mask = numeric_value(
            reg.get("mask", 0xFFFFFFFF),
            f"{device_name}.{reg['name']}.mask"
        )

        lines.append(
            f"- `{reg['name']}` "
            f"offset=`{hex_value(offset)}` "
            f"access=`{reg['access']}` "
            f"reset=`{hex_value(reset)}` "
            f"mask=`{hex_value(mask)}`"
        )

    lines.extend([
        "",
        "## Generator validation",
        "",
        "- Register names are unique.",
        "- Register offsets are unique.",
        "- Register offsets are 4-byte aligned.",
        "- Access policies are validated.",
        "- Reset values are validated.",
        "- Register masks are validated.",
        "- Memory ranges are validated for overlap.",
        "- Memory ranges are validated for uint64 overflow.",
        "- The memory map is generated as a data-driven table.",
        "- Register metadata is generated from the schema.",
        "- Software offsets and masks are generated.",
        "",
    ])

    output.write_text("\n".join(lines))
    print(f"generated: {output}")


def generate_all(soc_data, device_data):
    generate_soc_map(soc_data)
    generate_register_metadata_header()

    for data in device_data.values():
        generate_device_header(data)
        generate_dispatch_source(data)
        generate_software_header(data)
        generate_device_docs(data)
        generate_verification_artifact(data)


def collect_files(directory):
    if not directory.exists():
        return set()

    return {
        path.relative_to(directory)
        for path in directory.rglob("*")
        if path.is_file()
    }


def check_generated(soc_data, device_data):
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_out = Path(temp_dir)

        global OUT_DIR
        original_out_dir = OUT_DIR
        OUT_DIR = temp_out

        try:
            generate_all(soc_data, device_data)
        finally:
            OUT_DIR = original_out_dir

        expected_files = collect_files(temp_out)
        actual_files = collect_files(original_out_dir)

        if expected_files != actual_files:
            missing = sorted(expected_files - actual_files)
            extra = sorted(actual_files - expected_files)

            if missing:
                print("Missing generated files:")
                for path in missing:
                    print(f"  {path}")

            if extra:
                print("Unexpected generated files:")
                for path in extra:
                    print(f"  {path}")

            raise ValueError(
                "generated output is out of date"
            )

        for relative_path in sorted(expected_files):
            expected = temp_out / relative_path
            actual = original_out_dir / relative_path

            if expected.read_bytes() != actual.read_bytes():
                raise ValueError(
                    f"generated output is out of date: {relative_path}"
                )

    print("Generator check: PASS")
    print("Generated files match the current schemas.")


def main():
    valid_arguments = {"--check"}

    arguments = set(sys.argv[1:])

    unknown_arguments = arguments - valid_arguments

    if unknown_arguments:
        print(
            "Unknown argument(s): "
            + ", ".join(sorted(unknown_arguments))
        )
        print("Usage: python generator/generate.py [--check]")
        return 1

    check_mode = "--check" in arguments

    print("Validating schemas...")

    try:
        soc_data, device_data = validate_all()
    except ValueError as error:
        print(f"Schema validation failed: {error}")
        return 1

    print("Schema validation: OK")
    print()

    if check_mode:
        try:
            check_generated(
                soc_data,
                device_data
            )
        except ValueError as error:
            print(f"Generator check failed: {error}")
            return 1

        return 0

    generate_all(
        soc_data,
        device_data
    )

    print()
    print("Generation complete.")

    return 0


if __name__ == "__main__":
    sys.exit(main())