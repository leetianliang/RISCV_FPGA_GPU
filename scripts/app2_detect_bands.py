#!/usr/bin/env python3
"""Detect sprite candidate bounding boxes on a source sheet via alpha gaps."""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SHEETS = ROOT / "assets" / "facility_omega" / "source" / "source_sheets"


def row_occupancy(im: Image.Image) -> list[int]:
    a = im.getchannel("A")
    w, h = im.size
    px = a.load()
    return [sum(1 for x in range(w) if px[x, y] > 8) for y in range(h)]


def col_occupancy(im: Image.Image, y0: int, y1: int) -> list[int]:
    a = im.getchannel("A")
    w, _ = im.size
    px = a.load()
    return [sum(1 for y in range(y0, y1) if px[x, y] > 8) for x in range(w)]


def bands(occ: list[int], min_run: int = 8, gap: int = 6) -> list[tuple[int, int]]:
    """Return (start, end) inclusive bands where occupancy is mostly nonzero."""
    out: list[tuple[int, int]] = []
    i = 0
    n = len(occ)
    while i < n:
        if occ[i] > 2:
            j = i
            gap_len = 0
            last = i
            while j < n:
                if occ[j] > 2:
                    last = j
                    gap_len = 0
                else:
                    gap_len += 1
                    if gap_len >= gap:
                        break
                j += 1
            if last - i + 1 >= min_run:
                out.append((i, last))
            i = last + gap_len + 1
        else:
            i += 1
    return out


def main() -> int:
    name = sys.argv[1] if len(sys.argv) > 1 else "player_source.png"
    p = SHEETS / name
    im = Image.open(p).convert("RGBA")
    rows = row_occupancy(im)
    rb = bands(rows, min_run=20, gap=10)
    print(f"{name} {im.size} row_bands={rb}")
    for y0, y1 in rb:
        cols = col_occupancy(im, y0, y1 + 1)
        cb = bands(cols, min_run=12, gap=12)
        print(f"  band y={y0}-{y1} h={y1-y0+1} cols={[(a,b,b-a+1) for a,b in cb][:20]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
