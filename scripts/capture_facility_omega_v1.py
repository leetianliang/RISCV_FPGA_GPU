#!/usr/bin/env python3
"""Capture FACILITY-Omega review screenshots (interactive 640x360 for CI speed)."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "stage0045" / "model" / "pc_demo" / "gpu2d_demo.exe"
OUT = ROOT / "results" / "facility_omega" / "v0_1"
OUT.mkdir(parents=True, exist_ok=True)


def raw_to_png(raw: Path, png: Path, w: int = 640, h: int = 360) -> None:
    data = raw.read_bytes()
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        row = y * w * 2
        for x in range(w):
            p = data[row + x * 2] | (data[row + x * 2 + 1] << 8)
            r = ((p >> 11) & 31) << 3
            g = ((p >> 5) & 63) << 2
            b = (p & 31) << 3
            px[x, y] = (r | (r >> 5), g | (g >> 6), b | (b >> 5))
    im.save(png)


def run_capture(name: str, frames: int, extra: list[str]) -> None:
    raw = OUT / f"{name}.raw"
    cmd = [
        str(EXE), "--app", "facility", "--headless", "--frames", str(frames),
        "--seed", "1234", "--backend", "immediate",
        "--profile", "interactive", "--capture", str(raw), *extra,
    ]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    line = r.stdout.strip().splitlines()[-1] if r.stdout else r.stderr[-200:]
    print(name, r.returncode, line)
    if raw.is_file():
        raw_to_png(raw, OUT / f"{name}.png")


def main() -> int:
    if not EXE.is_file():
        print("build gpu2d_demo first", file=sys.stderr)
        return 1
    run_capture("v2_large_map", 120, ["--facility-route"])
    run_capture("v3_combat", 100, [])
    run_capture("v5_technical", 60, ["--xray"])
    vis = ROOT / "build" / "stage0045" / "model" / "pc_demo" / "gpu2d_test_facility_visual.exe"
    if vis.is_file():
        r = subprocess.run([str(vis), str(OUT / "v4_levelup_fixture")],
                           capture_output=True, text=True, timeout=120)
        print("v4", r.returncode, r.stdout.strip()[-160:])
        raw = OUT / "v4_levelup_fixture.raw"
        if raw.is_file():
            raw_to_png(raw, OUT / "v4_levelup_fixture.png", 640, 360)
    # V1 contact sheet pointer
    print("V1 contact:", ROOT / "assets/facility_omega/processed/contact_sheet.png")
    print("captures →", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
