#!/usr/bin/env python3
"""J-05 / SYS-05: game code must not include Golden internals."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAME_DIRS = [
    ROOT / "software" / "applications" / "neon_survivor",
    ROOT / "software" / "applications" / "facility_omega",
]
FORBIDDEN = (
    "golden/",
    "GoldenGPU",
    "MemoryImage",
    "TileHeader",
    "WorkRef",
    "TILE_FRAME",
    "tile_binner",
    "golden_gpu.hpp",
    "tile_types.hpp",
    "memory_image.hpp",
)

def main() -> int:
    bad = 0
    for d in GAME_DIRS:
        for p in d.rglob("*"):
            if p.suffix not in {".cpp", ".hpp", ".h", ".c", ".cc"}:
                continue
            text = p.read_text(encoding="utf-8", errors="replace")
            for tok in FORBIDDEN:
                if tok in text:
                    print(f"[FAIL] {p.relative_to(ROOT)} contains forbidden {tok!r}")
                    bad += 1
    if bad:
        print(f"app boundary: FAIL ({bad})")
        return 1
    print("app boundary: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
