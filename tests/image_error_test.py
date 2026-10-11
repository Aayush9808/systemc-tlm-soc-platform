#!/usr/bin/env python3
"""Regression-test that ROM/SRAM reject missing and oversized images."""

from pathlib import Path
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parent.parent


def main():
    include_dir = os.environ.get("SYSTEMC_INCLUDE_DIR")
    library = os.environ.get("SYSTEMC_LIBRARY")
    if not include_dir or not library:
        raise SystemExit("FAIL: SYSTEMC_INCLUDE_DIR and SYSTEMC_LIBRARY are required")

    source = r"""
#include <systemc>
#include <stdexcept>
#include <string>
#include "model/memory/simple_memory.h"
#include "model/memory/simple_rom.h"

int sc_main(int argc, char* argv[]) {
    if (argc != 3) return 2;
    try {
        const std::string kind(argv[1]);
        const std::string path(argv[2]);
        if (kind == "rom") {
            SimpleRom target("rom", 4, path);
        } else if (kind == "sram") {
            SimpleMemory target(
                "sram", 4, sc_core::sc_time(1, sc_core::SC_NS), path);
        } else {
            return 2;
        }
    } catch (const std::exception&) {
        return 0;
    }
    return 1;
}
"""

    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        cpp = temp / "image_error_probe.cpp"
        binary = temp / "image_error_probe"
        oversized = temp / "oversized.bin"
        missing = temp / "does-not-exist.bin"
        oversized.write_bytes(bytes(range(8)))
        cpp.write_text(source)

        compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            raise SystemExit("FAIL: no C++ compiler found")

        build = subprocess.run(
            [
                compiler, "-std=c++17",
                "-I", include_dir,
                "-I", str(ROOT),
                str(cpp), library, "-o", str(binary),
            ],
            capture_output=True, text=True,
        )
        if build.returncode:
            raise SystemExit(
                "FAIL: image-error probe compilation failed:\\n"
                + build.stdout + build.stderr
            )

        cases = [
            ("rom", missing, "missing ROM image"),
            ("rom", oversized, "oversized ROM image"),
            ("sram", missing, "missing SRAM image"),
            ("sram", oversized, "oversized SRAM image"),
        ]
        for kind, image, description in cases:
            run = subprocess.run(
                [str(binary), kind, str(image)],
                capture_output=True, text=True,
            )
            if run.returncode != 0:
                raise SystemExit(
                    f"FAIL: {description} was not rejected as expected "
                    f"(exit {run.returncode})\\n{run.stdout}{run.stderr}"
                )
            print(f"[PASS] {description} rejected")

    print("IMAGE INITIALIZATION ERROR TEST: PASS")


if __name__ == "__main__":
    main()
