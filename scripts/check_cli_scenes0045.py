#!/usr/bin/env python3
"""B14: launch every required scene headless; optional capture determinism."""
from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "stage0045" / "model" / "pc_demo" / "gpu2d_demo.exe"

SCENES = ["game", "sprite", "alpha", "bullet", "scale", "overdraw"]


def main() -> int:
    if not EXE.is_file():
        print(f"missing {EXE}", file=sys.stderr)
        return 1
    bad = 0
    for sc in SCENES:
        cmd = [str(EXE), "--headless", "--frames", "30", "--seed", "1234",
               "--backend", "tile", "--scene", sc]
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
        if r.returncode != 0 or "gpu2d_demo done" not in r.stdout:
            print(f"[FAIL] scene {sc} rc={r.returncode} out={r.stdout[-120:]} err={r.stderr[-120:]}")
            bad += 1
        else:
            print(f"[PASS] scene {sc}")
    # capture determinism: two runs same hash
    with tempfile.TemporaryDirectory() as td:
        h = []
        for i in range(2):
            out = Path(td) / f"c{i}.raw"
            r = subprocess.run(
                [str(EXE), "--headless", "--frames", "20", "--seed", "42",
                 "--backend", "tile", "--scene", "game", "--capture", str(out)],
                capture_output=True, text=True, timeout=180)
            if r.returncode != 0 or not out.is_file():
                print(f"[FAIL] capture run {i}")
                bad += 1
                continue
            h.append(hashlib.sha256(out.read_bytes()).hexdigest())
        if len(h) == 2:
            if h[0] != h[1]:
                print(f"[FAIL] capture not deterministic\n  {h[0]}\n  {h[1]}")
                bad += 1
            else:
                print(f"[PASS] capture deterministic {h[0][:16]}...")
    if bad:
        print(f"cli scenes: FAIL ({bad})")
        return 1
    print("cli scenes: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
