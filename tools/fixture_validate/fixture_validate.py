#!/usr/bin/env python3
"""Validate checked-in golden fixtures. Standard library only. Read-only."""

from __future__ import annotations

import json
import sys
from pathlib import Path

REQUIRED = {
    "format_version",
    "isa_version",
    "pixel_arith_version",
    "width",
    "height",
    "stride",
    "framebuffer_format",
    "command_count",
    "base",
}


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}")


def ok(msg: str) -> None:
    print(f"[PASS] {msg}")


def validate_fixture(dir_path: Path) -> bool:
    errors = 0
    man = dir_path / "manifest.json"
    cmd = dir_path / "command.bin"
    fb = dir_path / "initial_fb.raw"
    gold = dir_path / "golden_fb.raw"

    if not man.is_file():
        fail(f"{dir_path.name}: missing manifest.json")
        return False
    try:
        meta = json.loads(man.read_text(encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        fail(f"{dir_path.name}: manifest json error: {e}")
        return False

    missing = REQUIRED - set(meta)
    if missing:
        fail(f"{dir_path.name}: missing manifest fields {sorted(missing)}")
        errors += 1

    if not cmd.is_file() or cmd.stat().st_size == 0 or cmd.stat().st_size % 64 != 0:
        fail(f"{dir_path.name}: command.bin must be N*64 bytes")
        errors += 1
    else:
        ncmds = cmd.stat().st_size // 64
        if meta.get("command_count") != ncmds:
            fail(f"{dir_path.name}: command_count {meta.get('command_count')} != {ncmds}")
            errors += 1

    bpp = {"RGB565": 2, "ARGB8888": 4, "XRGB8888": 4}.get(
        str(meta.get("framebuffer_format")), 0
    )
    if bpp and fb.is_file():
        expect = int(meta.get("stride", 0)) * int(meta.get("height", 0))
        if fb.stat().st_size != expect:
            fail(f"{dir_path.name}: initial_fb size {fb.stat().st_size} != {expect}")
            errors += 1
        if gold.is_file() and gold.stat().st_size != expect:
            fail(f"{dir_path.name}: golden_fb size mismatch")
            errors += 1
        if int(meta.get("stride", 0)) < int(meta.get("width", 0)) * bpp:
            fail(f"{dir_path.name}: stride < width*bpp")
            errors += 1

    # Command W0 class/opcode + DST_BASE for FILL/BLIT when present
    if cmd.is_file() and cmd.stat().st_size >= 64:
        data = cmd.read_bytes()
        w0 = int.from_bytes(data[0:4], "little")
        w5 = int.from_bytes(data[20:24], "little")  # DST_BASE
        w7 = int.from_bytes(data[28:32], "little")  # DST_STRIDE
        cls = (w0 >> 28) & 0xF
        opc = (w0 >> 20) & 0xFF
        if cls != 1:
            fail(f"{dir_path.name}: cmd class {cls} != DRAW_2D")
            errors += 1
        if opc not in (0x00, 0x01, 0x02):
            fail(f"{dir_path.name}: unexpected opcode {opc}")
            errors += 1
        if w5 != int(meta.get("base", 0)):
            fail(f"{dir_path.name}: DST_BASE {w5:#x} != manifest base {meta.get('base')}")
            errors += 1
        if opc in (0x00, 0x01) and w7 != int(meta.get("stride", 0)):
            fail(f"{dir_path.name}: DST_STRIDE mismatch with manifest")
            errors += 1

    tex = dir_path / "textures.bin"
    if "texture_base" in meta and not tex.is_file():
        # textures optional for some fixtures that embed differently
        pass
    if tex.is_file() and "texture_format" in meta:
        tf = meta["texture_format"]
        tbpp = {"RGB565": 2, "ARGB8888": 4, "XRGB8888": 4, "INDEX8": 1}.get(tf)
        tw = int(meta.get("texture_width", 8))
        th = int(meta.get("texture_height", 8))
        ts = int(meta.get("texture_stride", tw * (tbpp or 2)))
        if tbpp and tex.stat().st_size != ts * th:
            # allow optional texture_stride default 16 for rgb565 8x8
            if not (tf == "RGB565" and tex.stat().st_size == 16 * th):
                fail(
                    f"{dir_path.name}: textures.bin size {tex.stat().st_size} != {ts * th}"
                )
                errors += 1

    if errors == 0:
        ok(dir_path.name)
        return True
    return False


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("Usage: fixture_validate.py <frames_root>", file=sys.stderr)
        return 2
    root = Path(argv[1])
    if not root.is_dir():
        print(f"missing dir {root}", file=sys.stderr)
        return 2
    fixtures = [p for p in sorted(root.iterdir()) if p.is_dir() and (p / "manifest.json").is_file()]
    if not fixtures:
        fail("no fixtures found")
        return 1
    bad = 0
    for f in fixtures:
        if not validate_fixture(f):
            bad += 1
    print(f"validated {len(fixtures)} fixtures, failures={bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
