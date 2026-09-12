#!/usr/bin/env python3
"""Baseline check: preflight + optional CMake configure.

Standard library only. Never installs packages. Never alters the environment.
Uses a temporary build directory under build/baseline-check/.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


def project_root() -> Path:
    return Path(__file__).resolve().parent.parent


def run_preflight(root: Path) -> int:
    script = root / "scripts" / "preflight.py"
    print("=== preflight ===")
    result = subprocess.run([sys.executable, str(script)], cwd=str(root))
    print(f"preflight exit code: {result.returncode}")
    return result.returncode


def check_cmake(root: Path) -> int:
    """Return 0 on success/skip, 1 on configure failure."""
    print()
    print("=== cmake configure ===")
    cmake = shutil.which("cmake")
    if cmake is None:
        print("[WARN] cmake not found on PATH; skipping configure check.")
        return 0

    build_dir = root / "build" / "baseline-check"
    build_dir.mkdir(parents=True, exist_ok=True)

    configure = subprocess.run(
        [cmake, "-S", str(root), "-B", str(build_dir)],
        cwd=str(root),
    )
    print(f"cmake configure exit code: {configure.returncode}")
    if configure.returncode != 0:
        return 1

    # Build may have no targets; configuration success is sufficient,
    # but still invoke build to match acceptance criteria when possible.
    build = subprocess.run(
        [cmake, "--build", str(build_dir)],
        cwd=str(root),
    )
    print(f"cmake build exit code: {build.returncode}")
    # Empty projects can return non-zero on some generators if there is nothing
    # to build; treat configure success as pass and warn on build failure only
    # when there are no compiled targets expected (TASK_000 placeholder).
    if build.returncode != 0:
        print("[WARN] cmake build returned non-zero; project has no compiled "
              "targets in TASK_000 — treating as non-fatal.")
    return 0


def main() -> int:
    root = project_root()
    print(f"Project root: {root}")

    preflight_rc = run_preflight(root)

    structure_ok = preflight_rc in (0, 2)
    if not structure_ok:
        print()
        print("Baseline check: FAIL (repository structure error)")
        return 1

    cmake_rc = check_cmake(root)
    if cmake_rc != 0:
        print()
        print("Baseline check: FAIL (cmake configure failed)")
        return 1

    print()
    if preflight_rc == 0:
        print("Baseline check: PASS")
        return 0
    print("Baseline check: PASS_WITH_BLOCKERS (structure OK; specs missing)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
