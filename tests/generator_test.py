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


if __name__ == "__main__":
    main()