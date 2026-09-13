#!/usr/bin/env python3
"""Tile sweep via functional model binary. Stdlib only."""

from __future__ import annotations

import csv
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
EXE = ROOT / "build" / "stage004" / "model" / "golden" / "golden_tile_sweep.exe"
OUT = ROOT / "results" / "stage004_tile" / "tile_sweep.csv"


def run(kind: int, tile: int) -> dict:
    r = subprocess.run([str(EXE), str(kind), str(tile)], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.strip() or r.stdout)
    line = r.stdout.strip().splitlines()[-1]
    parts = dict()
    for tok in line.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            parts[k] = int(v)
    names = {0: "W3_alpha_storm", 1: "W1_sprite_grid", 2: "W2_high_overdraw"}
    return {
        "workload": names.get(kind, f"kind{kind}"),
        "tile_size": parts["tile"],
        "tiles_total": parts["tiles"],
        "tiles_active": parts["active"],
        "workref_count": parts["refs"],
        "max_workrefs_per_tile": parts["maxrefs"],
        "tile_load_pixels": parts["load_px"],
        "tile_store_pixels": parts["store_px"],
        "tile_load_bytes": parts["load_b"],
        "tile_store_bytes": parts["store_b"],
    }


def main() -> int:
    if not EXE.is_file():
        print(f"missing {EXE}; build stage004 first", file=sys.stderr)
        return 1
    OUT.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    for kind in (0, 1, 2):
        for tile in (16, 32, 64):
            rows.append(run(kind, tile))
    with OUT.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {OUT} ({len(rows)} rows from functional model)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
