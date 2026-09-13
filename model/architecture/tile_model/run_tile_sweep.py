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
    kind_i = int(parts.get("kind", kind))
    return {
        "workload_id": f"W{kind_i + 1}",
        "workload_name": parts.get("workload", f"W{kind_i + 1}"),
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


def expected_tiles(tile: int, tw: int = 64, th: int = 64) -> int:
    gw = (tw + tile - 1) // tile
    gh = (th + tile - 1) // tile
    return gw * gh


def verify_row(row: dict) -> list[str]:
    errs: list[str] = []
    tile = int(row["tile_size"])
    want = expected_tiles(tile)
    if int(row["tiles_total"]) != want:
        errs.append(f"tiles_total {row['tiles_total']} != {want} for tile={tile}")
    if int(row["tiles_active"]) <= 0:
        errs.append("tiles_active must be > 0")
    if int(row["tile_load_bytes"]) < 0 or int(row["tile_store_bytes"]) < 0:
        errs.append("negative traffic bytes")
    return errs


def main() -> int:
    if not EXE.is_file():
        print(f"missing {EXE}; build stage004 first", file=sys.stderr)
        return 1
    OUT.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    for kind in range(6):
        for tile in (16, 32, 64):
            rows.append(run(kind, tile))
    bad = 0
    for r in rows:
        for e in verify_row(r):
            print(f"[FAIL] {r['workload_id']} tile={r['tile_size']}: {e}", file=sys.stderr)
            bad += 1
    if len(rows) != 18:
        print(f"[FAIL] expected 18 sweep rows, got {len(rows)}", file=sys.stderr)
        bad += 1
    if bad:
        return 1
    with OUT.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {OUT} ({len(rows)} rows from functional model, W1-W6 x 16/32/64)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
