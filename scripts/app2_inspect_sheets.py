#!/usr/bin/env python3
"""Inspect source sheets: size, alpha, rough content bounds."""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SHEETS = ROOT / "assets" / "facility_omega" / "source" / "source_sheets"


def main() -> int:
    if not SHEETS.is_dir():
        print("run app2_asset_intake first", file=sys.stderr)
        return 1
    for p in sorted(SHEETS.glob("*.png")):
        im = Image.open(p).convert("RGBA")
        a = im.getchannel("A")
        bbox = a.getbbox()
        hist = a.histogram()
        opaque = sum(hist[250:])
        print(f"{p.name:28s} {im.size[0]}x{im.size[1]} alpha_bbox={bbox} opaque_px={opaque}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
