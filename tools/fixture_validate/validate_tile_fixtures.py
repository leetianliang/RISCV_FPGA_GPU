#!/usr/bin/env python3
"""Validate Tile binary fixtures. Stdlib only."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TILE_ROOT = ROOT / "model" / "golden" / "tests" / "frames" / "tile"


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def ok(msg: str) -> None:
    print(f"[PASS] {msg}")


def validate_tile_fixture(dir_path: Path) -> bool:
    errors = 0
    man = dir_path / "manifest.json"
    if not man.is_file():
        fail(f"{dir_path.name}: missing manifest.json")
        return False
    meta = json.loads(man.read_text(encoding="utf-8"))
    for k in ("format_version", "isa_version", "width", "height", "stride",
              "tile", "framebuffer_format", "command_count", "base"):
        if k not in meta:
            fail(f"{dir_path.name}: manifest missing {k}")
            errors += 1
    cmd = dir_path / "command.bin"
    if not cmd.is_file() or cmd.stat().st_size != 64:
        fail(f"{dir_path.name}: command.bin must be 64B TILE_FRAME")
        errors += 1
    else:
        data = cmd.read_bytes()
        w0 = int.from_bytes(data[0:4], "little")
        cls = (w0 >> 28) & 0xF
        opc = (w0 >> 20) & 0xFF
        if cls != 1 or opc != 0x10:
            fail(f"{dir_path.name}: not TILE_FRAME (class={cls} op={opc})")
            errors += 1
        desc_b = int.from_bytes(data[16:20], "little")
        hdr_b = int.from_bytes(data[20:24], "little")
        work_b = int.from_bytes(data[24:28], "little")
        if desc_b == 0 or hdr_b == 0:
            fail(f"{dir_path.name}: zero TILE_FRAME bases")
            errors += 1
    descs = dir_path / "descriptors.bin"
    hdrs = dir_path / "tile_headers.bin"
    works = dir_path / "workrefs.bin"
    if not descs.is_file() or descs.stat().st_size % 64 != 0:
        fail(f"{dir_path.name}: descriptors.bin must be N*64")
        errors += 1
    if not hdrs.is_file() or hdrs.stat().st_size % 16 != 0:
        fail(f"{dir_path.name}: tile_headers.bin must be N*16")
        errors += 1
    if works.is_file() and works.stat().st_size % 4 != 0:
        fail(f"{dir_path.name}: workrefs.bin must be N*4")
        errors += 1
    stride = int(meta.get("stride", 0))
    height = int(meta.get("height", 0))
    for fb in ("initial_fb.raw", "golden_fb.raw"):
        p = dir_path / fb
        if not p.is_file():
            fail(f"{dir_path.name}: missing {fb}")
            errors += 1
        elif p.stat().st_size != stride * height:
            fail(f"{dir_path.name}: {fb} size mismatch")
            errors += 1
    if errors == 0:
        ok(dir_path.name)
        return True
    return False


def main() -> int:
    if not TILE_ROOT.is_dir():
        fail(f"missing {TILE_ROOT}")
        return 1
    fixtures = [p for p in sorted(TILE_ROOT.iterdir()) if p.is_dir()]
    if not fixtures:
        fail("no tile fixtures")
        return 1
    bad = 0
    for f in fixtures:
        if not validate_tile_fixture(f):
            bad += 1
    print(f"tile fixtures validated: {len(fixtures)}, failures={bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
