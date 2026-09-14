#!/usr/bin/env python3
"""R1: verify ARGB8888 runtime bytes are BGRA (Golden 0xAARRGGBB LE)."""
from __future__ import annotations

import struct
import sys
import tempfile
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from facility_omega_assetc import pack_argb8888_bgra  # noqa: E402


def main() -> int:
    im = Image.new("RGBA", (2, 1))
    im.putpixel((0, 0), (255, 0, 0, 255))  # red
    im.putpixel((1, 0), (0, 0, 255, 255))  # blue
    raw = pack_argb8888_bgra(im)
    assert len(raw) == 8, len(raw)
    # pixel0: B,G,R,A = 0,0,255,255
    assert raw[0] == 0 and raw[1] == 0 and raw[2] == 255 and raw[3] == 255, list(raw[:4])
    # pixel1: B,G,R,A = 255,0,0,255
    assert raw[4] == 255 and raw[5] == 0 and raw[6] == 0 and raw[7] == 255, list(raw[4:8])
    # Golden pack: Rgba8888::pack(p[3], p[2], p[1], p[0]) → A,R,G,B
    a0, r0, g0, b0 = raw[3], raw[2], raw[1], raw[0]
    assert (a0, r0, g0, b0) == (255, 255, 0, 0)
    print("argb BGRA order: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
