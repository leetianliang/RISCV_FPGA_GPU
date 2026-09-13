#!/usr/bin/env python3
"""Validate Tile binary fixtures deeply. Stdlib only."""

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
    for k in ("format_version", "isa_version", "pixel_arith_version", "width",
              "height", "stride", "tile", "framebuffer_format", "command_count",
              "base"):
        if k not in meta:
            fail(f"{dir_path.name}: manifest missing {k}")
            errors += 1

    cmd = dir_path / "command.bin"
    if not cmd.is_file() or cmd.stat().st_size != 64:
        fail(f"{dir_path.name}: command.bin must be 64B")
        errors += 1
        return False
    data = cmd.read_bytes()
    w0 = int.from_bytes(data[0:4], "little")
    if ((w0 >> 28) & 0xF) != 1 or ((w0 >> 20) & 0xFF) != 0x10:
        fail(f"{dir_path.name}: not TILE_FRAME")
        errors += 1
    desc_b = int.from_bytes(data[16:20], "little")
    hdr_b = int.from_bytes(data[20:24], "little")
    work_b = int.from_bytes(data[24:28], "little")
    if desc_b & 0x3F:
        fail(f"{dir_path.name}: desc base not 64B aligned")
        errors += 1
    if hdr_b & 0x0F:
        fail(f"{dir_path.name}: header base not 16B aligned")
        errors += 1
    if work_b & 0x03:
        fail(f"{dir_path.name}: work base not 4B aligned")
        errors += 1
    surface_w = int.from_bytes(data[36:38], "little")
    surface_h = int.from_bytes(data[38:40], "little")
    grid_w = int.from_bytes(data[40:42], "little")
    grid_h = int.from_bytes(data[42:44], "little")
    tile_w = int.from_bytes(data[44:46], "little")
    tile_h = int.from_bytes(data[46:48], "little")
    if tile_w and tile_w != int(meta.get("tile", 0)):
        fail(f"{dir_path.name}: tile cmd vs manifest")
        errors += 1
    if surface_w != int(meta.get("width", 0)) or surface_h != int(meta.get("height", 0)):
        fail(f"{dir_path.name}: surface cmd vs manifest")
        errors += 1
    if tile_w and tile_h:
        ew = (surface_w + tile_w - 1) // tile_w
        eh = (surface_h + tile_h - 1) // tile_h
        if grid_w != ew or grid_h != eh:
            fail(f"{dir_path.name}: grid {grid_w}x{grid_h} != {ew}x{eh}")
            errors += 1
    dst_stride = int.from_bytes(data[32:36], "little")
    dst_base = int.from_bytes(data[28:32], "little")
    if dst_base != int(meta.get("base", dst_base)):
        fail(f"{dir_path.name}: dst_base cmd vs manifest")
        errors += 1
    if dst_stride and dst_stride != int(meta.get("stride", 0)):
        fail(f"{dir_path.name}: dst_stride cmd vs manifest")
        errors += 1

    descs = dir_path / "descriptors.bin"
    hdrs = dir_path / "tile_headers.bin"
    works = dir_path / "workrefs.bin"
    if not descs.is_file() or descs.stat().st_size % 64 != 0:
        fail(f"{dir_path.name}: descriptors.bin N*64")
        errors += 1
    if not hdrs.is_file() or hdrs.stat().st_size % 16 != 0:
        fail(f"{dir_path.name}: tile_headers.bin N*16")
        errors += 1
    if works.is_file() and works.stat().st_size % 4 != 0:
        fail(f"{dir_path.name}: workrefs.bin N*4")
        errors += 1

    n_desc = descs.stat().st_size // 64 if descs.is_file() else 0
    n_headers = hdrs.stat().st_size // 16 if hdrs.is_file() else 0
    n_tiles = grid_w * grid_h
    if n_headers != n_tiles:
        fail(f"{dir_path.name}: header count {n_headers} != tiles {n_tiles}")
        errors += 1
    if works.is_file() and hdrs.is_file():
        hdata = hdrs.read_bytes()
        wdata = works.read_bytes()
        n_work = len(wdata) // 4
        for i in range(n_headers):
            off = int.from_bytes(hdata[i * 16 : i * 16 + 4], "little")
            cnt = int.from_bytes(hdata[i * 16 + 4 : i * 16 + 8], "little")
            if off + cnt > n_work:
                fail(f"{dir_path.name}: tile {i} worklist OOB")
                errors += 1
                continue
            for j in range(cnt):
                idx = int.from_bytes(wdata[(off + j) * 4 : (off + j) * 4 + 4], "little")
                if idx >= max(1, n_desc):
                    fail(f"{dir_path.name}: workref desc OOB")
                    errors += 1

    stride = int(meta.get("stride", 0))
    height = int(meta.get("height", 0))
    for fb in ("initial_fb.raw", "golden_fb.raw"):
        p = dir_path / fb
        if not p.is_file() or p.stat().st_size != stride * height:
            fail(f"{dir_path.name}: {fb} size")
            errors += 1

    # resource files: when present, enforce exact structural sizes
    pal = dir_path / "palette.bin"
    if pal.is_file():
        psz = pal.stat().st_size
        if psz != 1024:
            fail(f"{dir_path.name}: palette.bin must be 1024B (got {psz})")
            errors += 1
    tex = dir_path / "textures.bin"
    if tex.is_file() and tex.stat().st_size == 0:
        fail(f"{dir_path.name}: textures.bin empty")
        errors += 1
    ext = dir_path / "extensions.bin"
    if ext.is_file():
        esz = ext.stat().st_size
        if esz == 0 or esz % 64 != 0:
            fail(f"{dir_path.name}: extensions.bin must be N*64 non-zero (got {esz})")
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
