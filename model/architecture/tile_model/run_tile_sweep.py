#!/usr/bin/env python3
"""Deterministic Tile size sweep for Stage-004 architecture experiment.

Standard library only. Produces results/stage004_tile/tile_sweep.csv
"""

from __future__ import annotations

import csv
from pathlib import Path

# Workload definitions (fixed): counts only, analytical metadata.
WORKLOADS = {
    "W1_sprite_grid": {"draws": 64, "small_spr": 8},
    "W2_high_overdraw": {"draws": 128, "area": 16},
    "W3_alpha_storm": {"draws": 80, "alpha": True},
    "W4_scaled_sprites": {"draws": 20, "scaled": True},
    "W5_edge_scatter": {"draws": 30, "scatter": True},
    "W6_mixed_scene": {"draws": 100, "mixed": True},
}


def metrics(name: str, tile: int, tw: int = 64, th: int = 64) -> dict:
    tx = (tw + tile - 1) // tile
    ty = (th + tile - 1) // tile
    tiles = tx * ty
    draws = WORKLOADS[name]["draws"]
    # analytical: small sprites touch ~1 tile; large/scatter more
    span = 1 if "small_spr" in WORKLOADS[name] else max(1, tile // 8)
    active = min(tiles, max(1, draws * span // max(1, tiles // 2 + 1)))
    workrefs = draws * span
    bpp = 2
    load_store = active * tile * tile * bpp
    return {
        "workload": name,
        "tile_size": tile,
        "tiles_total": tiles,
        "tiles_active": active,
        "draw_count": draws,
        "workref_count": workrefs,
        "max_workrefs_per_tile": max(1, workrefs // max(1, active)),
        "tile_load_bytes": load_store,
        "tile_store_bytes": load_store,
    }


def main() -> None:
    root = Path(__file__).resolve().parents[3]
    out = root / "results" / "stage004_tile" / "tile_sweep.csv"
    out.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    for name in WORKLOADS:
        for tile in (16, 32, 64):
            rows.append(metrics(name, tile))
    with out.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {out} ({len(rows)} rows)")


if __name__ == "__main__":
    main()
