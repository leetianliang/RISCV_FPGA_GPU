#!/usr/bin/env python3
"""Repository preflight: structure + required specification files.

Standard library only. Read-only: never modifies any file.

Exit codes:
  0 - structure OK and all required specs found
  1 - repository structure error
  2 - structure OK but required specs missing
"""

from __future__ import annotations

import sys
from pathlib import Path


REQUIRED_SPECS = [
    ("Requirements", "RISC-V_FPGA_2D_GPU_Requirements_V1.0.md"),
    ("System Architecture", "RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md"),
    ("Command ISA", "RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md"),
    ("Internal Interface", "RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md"),
    ("Pixel Arithmetic", "RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md"),
    ("Register/Memory Map", "RISC-V_FPGA_2D_GPU_Register_Map_Memory_Map_V0.1.md"),
    ("Verification Plan", "RISC-V_FPGA_2D_GPU_Verification_Plan_V0.1.md"),
    ("Project Development Flow", "RISC-V_FPGA_2D_GPU_Project_Development_Flow_V1.0.md"),
    ("Golden Software Architecture", "RISC-V_FPGA_2D_GPU_Golden_GPU_Software_Architecture_V0.1.md"),
]

# Blocking specs for Gate A (task document section 3.2)
BLOCKING_SPECS = {
    "RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md",
    "RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md",
}

REQUIRED_DIRS = [
    "docs",
    "docs/tasks",
    "docs/reports",
    "docs/reviews",
    "docs/decisions",
    "spec",
    "spec/generated",
    "model/golden",
    "model/golden/include",
    "model/golden/src",
    "model/golden/tests",
    "model/architecture",
    "rtl",
    "rtl/common",
    "rtl/ctrl",
    "rtl/command",
    "rtl/frontend",
    "rtl/texture",
    "rtl/pixel",
    "rtl/tile",
    "rtl/memory",
    "rtl/display",
    "rtl/perf",
    "sim",
    "sim/unit",
    "sim/subsystem",
    "sim/random",
    "sim/fullsystem",
    "software",
    "software/driver",
    "software/graphics",
    "software/engine",
    "software/benchmark",
    "software/applications",
    "tools",
    "tools/asset_converter",
    "tools/command_generator",
    "tools/frame_compare",
    "tools/result_parser",
    "scripts",
    "board",
    "assets",
    "results",
    "release",
]

REQUIRED_FILES = [
    "README.md",
    "AGENTS.md",
    ".gitignore",
    ".gitattributes",
    "CMakeLists.txt",
    "model/golden/CMakeLists.txt",
    "scripts/preflight.py",
    "scripts/check_baseline.py",
]


def project_root() -> Path:
    """Locate project root relative to this script (scripts/ -> root)."""
    return Path(__file__).resolve().parent.parent


def check_structure(root: Path) -> list[str]:
    errors: list[str] = []
    for rel in REQUIRED_DIRS:
        path = root / rel
        if not path.is_dir():
            print(f"[FAIL] directory: {rel}")
            errors.append(f"missing directory: {rel}")
        else:
            print(f"[PASS] directory: {rel}")
    for rel in REQUIRED_FILES:
        path = root / rel
        if not path.is_file():
            print(f"[FAIL] file: {rel}")
            errors.append(f"missing file: {rel}")
        else:
            print(f"[PASS] file: {rel}")
    return errors


def check_specs(root: Path) -> list[str]:
    missing: list[str] = []
    docs = root / "docs"
    for label, filename in REQUIRED_SPECS:
        path = docs / filename
        if path.is_file():
            print(f"[PASS] spec: {label} ({filename})")
        else:
            print(f"[MISSING] spec: {label} ({filename})")
            missing.append(filename)
    return missing


def main() -> int:
    root = project_root()
    print(f"Project root: {root}")
    print()

    structure_errors = check_structure(root)
    print()
    missing_specs = check_specs(root)
    print()

    structure_ok = not structure_errors
    specs_ok = not missing_specs
    blocking_missing = sorted(BLOCKING_SPECS & set(missing_specs))

    print(f"Repository structure: {'PASS' if structure_ok else 'FAIL'}")
    if specs_ok:
        print("Specification preflight: PASS")
        exit_code = 0
    else:
        if blocking_missing:
            print("Specification preflight: BLOCKED")
            print(f"Blocking missing specs: {', '.join(blocking_missing)}")
        else:
            print("Specification preflight: WARN (non-blocking specs missing)")
        # Task requires exit 2 when required specs are missing
        exit_code = 2

    if not structure_ok:
        exit_code = 1

    print(f"Exit code: {exit_code}")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
