#!/usr/bin/env python3
"""B12: regenerate V1-V4 captures to temp and compare SHA256. Do not overwrite refs."""
from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "stage0045" / "model" / "pc_demo" / "gpu2d_demo.exe"
REF = ROOT / "results" / "stage0045" / "captures"

# Frozen capture recipes — must match checked files.
CASES = [
    ("v1_game_showcase.raw", ["--headless", "--frames", "120", "--seed", "1234",
                              "--backend", "tile", "--profile", "showcase", "--scene", "game"]),
    ("v2_effects.raw", ["--headless", "--frames", "150", "--seed", "9",
                        "--backend", "tile", "--profile", "showcase", "--scene", "alpha"]),
    ("v3_xray.raw", ["--headless", "--frames", "120", "--seed", "1234",
                     "--backend", "tile", "--profile", "showcase", "--scene", "game", "--xray"]),
    ("v4_overdraw_stress.raw", ["--headless", "--frames", "120", "--seed", "9",
                                "--backend", "tile", "--profile", "showcase", "--scene", "overdraw"]),
]


def sha256(p: Path) -> str:
    h = hashlib.sha256()
    h.update(p.read_bytes())
    return h.hexdigest()


def main() -> int:
    if not EXE.is_file():
        print(f"missing {EXE}", file=sys.stderr)
        return 1
    bad = 0
    with tempfile.TemporaryDirectory(prefix="cap0045_") as td:
        tdir = Path(td)
        for name, args in CASES:
            ref = REF / name
            if not ref.is_file():
                print(f"[FAIL] missing reference {ref}")
                bad += 1
                continue
            out = tdir / name
            cmd = [str(EXE), *args, "--capture", str(out)]
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
            if r.returncode != 0:
                print(f"[FAIL] {name} exit {r.returncode}: {r.stderr[-200:]}")
                bad += 1
                continue
            if not out.is_file():
                print(f"[FAIL] {name} no output")
                bad += 1
                continue
            hs, hr = sha256(out), sha256(ref)
            if hs != hr:
                print(f"[FAIL] {name} hash mismatch\n  got {hs}\n  ref {hr}")
                bad += 1
            else:
                print(f"[PASS] {name} {hs[:16]}...")
    if bad:
        print(f"capture verify: FAIL ({bad})")
        return 1
    print("capture verify: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
