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
    parts = {}
    for tok in line.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            if k in ("workload", "kind"):
                parts[k] = v
            elif k == "avg_od":
                parts[k] = float(v)
            else:
                try:
                    parts[k] = int(v)
                except ValueError:
                    parts[k] = v
    return {
        "workload_id": f"kind{parts.get('kind', kind)}",
        "workload_name": parts.get("workload", f"W?{kind}"),
        "version": 1,
        "seed": 0,
        "tile_size": parts.get("tile", tile),
        "tiles_total": parts.get("tiles", 0),
        "tiles_active": parts.get("active", 0),
        "workref_count": parts.get("refs", 0),
        "max_workrefs_per_tile": parts.get("maxrefs", 0),
        "tile_load_pixels": parts.get("load_px", 0),
        "tile_store_pixels": parts.get("store_px", 0),
        "tile_load_bytes": parts.get("load_b", 0),
        "tile_store_bytes": parts.get("store_b", 0),
        "blend_ops": parts.get("blend", 0),
        "pixels_written": parts.get("written", 0),
        "key_discards": parts.get("key", 0),
        "texture_samples": parts.get("samples", 0),
        "max_overdraw": parts.get("max_od", 0),
    }


def main() -> int:
    if not EXE.is_file():
        print(f"missing {EXE}; build stage004 first", file=sys.stderr)
        return 1
    OUT.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    for kind in range(6):
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
