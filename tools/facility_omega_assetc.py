#!/usr/bin/env python3
"""FACILITY-Omega offline asset compiler.

Processes immutable source sheets into runtime binaries + JSON manifest.
Deterministic: same source → same outputs. Opt-in --update to rewrite refs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets" / "facility_omega" / "source" / "source_sheets"
OUT = ROOT / "assets" / "facility_omega" / "runtime"
PREV = ROOT / "assets" / "facility_omega" / "processed"
AUDIT = ROOT / "assets" / "facility_omega" / "processed" / "asset_audit.json"

# Frozen crop boxes (x,y,w,h) from source-sheet band detection.
# Format: name -> dict(sheet, box, target_w, target_h, format, anchor)
# format: rgb565 | argb8888 | index8
CROPS: dict[str, dict] = {
    # Player — 4 row bands × columns from detect_bands
    "engineer_a0": {"sheet": "player", "box": (516, 40, 142, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.18},
    "engineer_a1": {"sheet": "player", "box": (794, 40, 142, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.18},
    "engineer_b0": {"sheet": "player", "box": (524, 295, 131, 190), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.18},
    "engineer_b1": {"sheet": "player", "box": (798, 295, 134, 190), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.18},
    "engineer_c0": {"sheet": "player", "box": (307, 535, 192, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.18},
    "engineer_c1": {"sheet": "player", "box": (539, 535, 175, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.18},
    "engineer_d0": {"sheet": "player", "box": (792, 535, 176, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.18},
    "engineer_d1": {"sheet": "player", "box": (999, 535, 184, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.18},
    "engineer_idle": {"sheet": "player", "box": (236, 808, 176, 200), "tw": 32, "th": 32,
                      "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.18},
    "engineer_hurt": {"sheet": "player", "box": (519, 808, 191, 200), "tw": 32, "th": 32,
                      "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.18},
    # Enemies
    "drone_0": {"sheet": "enemies", "box": (67, 90, 386, 200), "tw": 24, "th": 24,
                "fmt": "argb8888", "ax": 12, "ay": 12, "trim_bottom": 0.2},
    "crawler_0": {"sheet": "enemies", "box": (495, 90, 463, 200), "tw": 28, "th": 28,
                  "fmt": "argb8888", "ax": 14, "ay": 20, "trim_bottom": 0.2},
    "runner_0": {"sheet": "enemies", "box": (1028, 90, 365, 200), "tw": 28, "th": 28,
                 "fmt": "argb8888", "ax": 14, "ay": 20, "trim_bottom": 0.2},
    "tank_0": {"sheet": "enemies", "box": (39, 390, 712, 330), "tw": 48, "th": 48,
               "fmt": "argb8888", "ax": 24, "ay": 28, "trim_bottom": 0.15},
    "elite_0": {"sheet": "enemies", "box": (811, 390, 603, 330), "tw": 48, "th": 48,
                "fmt": "argb8888", "ax": 24, "ay": 28, "trim_bottom": 0.15},
    # Weapons / pickups
    "pulse_shot": {"sheet": "weapons", "box": (21, 20, 301, 100), "tw": 8, "th": 8,
                   "fmt": "argb8888", "ax": 4, "ay": 4, "trim_bottom": 0.1},
    "enemy_bullet": {"sheet": "weapons", "box": (342, 20, 140, 100), "tw": 8, "th": 8,
                     "fmt": "argb8888", "ax": 4, "ay": 4, "trim_bottom": 0.1},
    "orbit_drone": {"sheet": "weapons", "box": (795, 20, 235, 100), "tw": 12, "th": 12,
                    "fmt": "argb8888", "ax": 6, "ay": 6, "trim_bottom": 0.1},
    "xp_small": {"sheet": "weapons", "box": (21, 895, 304, 150), "tw": 8, "th": 8,
                 "fmt": "argb8888", "ax": 4, "ay": 4, "trim_bottom": 0.12},
    "xp_large": {"sheet": "weapons", "box": (364, 895, 310, 150), "tw": 12, "th": 12,
                 "fmt": "argb8888", "ax": 6, "ay": 6, "trim_bottom": 0.12},
    "repair_pickup": {"sheet": "weapons", "box": (733, 895, 323, 150), "tw": 16, "th": 16,
                      "fmt": "argb8888", "ax": 8, "ay": 8, "trim_bottom": 0.12},
    # FX
    "spark": {"sheet": "fx", "box": (20, 50, 180, 180), "tw": 8, "th": 8,
              "fmt": "argb8888", "ax": 4, "ay": 4},
    "glow_small": {"sheet": "fx", "box": (220, 50, 220, 220), "tw": 16, "th": 16,
                   "fmt": "argb8888", "ax": 8, "ay": 8},
    "glow_large": {"sheet": "fx", "box": (460, 50, 280, 280), "tw": 32, "th": 32,
                   "fmt": "argb8888", "ax": 16, "ay": 16},
    "ring": {"sheet": "fx", "box": (760, 50, 300, 300), "tw": 32, "th": 32,
             "fmt": "argb8888", "ax": 16, "ay": 16},
    "explosion": {"sheet": "fx", "box": (20, 380, 280, 280), "tw": 32, "th": 32,
                  "fmt": "argb8888", "ax": 16, "ay": 16},
    "trail": {"sheet": "fx", "box": (320, 380, 160, 160), "tw": 8, "th": 8,
              "fmt": "argb8888", "ax": 4, "ay": 4},
    # Environment
    "floor_00": {"sheet": "environment", "box": (19, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0},
    "floor_01": {"sheet": "environment", "box": (190, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0},
    "floor_02": {"sheet": "environment", "box": (360, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0},
    "floor_03": {"sheet": "environment", "box": (530, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0},
    "floor_04": {"sheet": "environment", "box": (700, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0},
    "floor_05": {"sheet": "environment", "box": (870, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0},
    "floor_06": {"sheet": "environment", "box": (1040, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0},
    "floor_07": {"sheet": "environment", "box": (19, 280, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0},
    "hazard_stripe": {"sheet": "environment", "box": (30, 700, 110, 200), "tw": 32, "th": 16,
                      "fmt": "rgb565", "ax": 0, "ay": 0},
    "grate": {"sheet": "environment", "box": (180, 700, 180, 220), "tw": 32, "th": 32,
              "fmt": "rgb565", "ax": 0, "ay": 0},
    "barrel": {"sheet": "environment", "box": (25, 985, 240, 200), "tw": 24, "th": 32,
               "fmt": "argb8888", "ax": 12, "ay": 28, "trim_bottom": 0.1},
    "crate": {"sheet": "environment", "box": (300, 985, 240, 200), "tw": 28, "th": 28,
              "fmt": "argb8888", "ax": 14, "ay": 24, "trim_bottom": 0.1},
    "console": {"sheet": "environment", "box": (570, 985, 190, 200), "tw": 28, "th": 36,
                "fmt": "argb8888", "ax": 14, "ay": 32, "trim_bottom": 0.1},
    "canister": {"sheet": "environment", "box": (790, 985, 170, 200), "tw": 20, "th": 28,
                 "fmt": "argb8888", "ax": 10, "ay": 24, "trim_bottom": 0.1},
}

SHEET_FILES = {
    "player": "player_source.png",
    "enemies": "enemies_source.png",
    "weapons": "weapons_pickups_source.png",
    "fx": "fx_source.png",
    "environment": "environment_source.png",
    "ui": "ui_source.png",
}


def pack_rgb565(im: Image.Image) -> bytes:
    px = im.load()
    out = bytearray(im.width * im.height * 2)
    i = 0
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            out[i] = v & 0xFF
            out[i + 1] = (v >> 8) & 0xFF
            i += 2
    return bytes(out)


def pack_argb8888(im: Image.Image) -> bytes:
    return im.tobytes("raw", "RGBA")


def sha256_file(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def process_one(name: str, spec: dict, sheets: dict[str, Image.Image], update: bool) -> dict:
    sh = sheets[spec["sheet"]]
    x, y, w, h = spec["box"]
    crop = sh.crop((x, y, x + w, y + h)).convert("RGBA")
    tb = float(spec.get("trim_bottom", 0.0))
    if tb > 0:
        crop = crop.crop((0, 0, crop.width, max(1, int(crop.height * (1.0 - tb)))))
    # trim near-transparent margins then resize
    a = crop.getchannel("A")
    bbox = a.getbbox()
    if bbox:
        crop = crop.crop(bbox)
    tw, th = spec["tw"], spec["th"]
    # alpha-aware downscale
    crop = crop.resize((tw, th), Image.Resampling.LANCZOS)
    fmt = spec["fmt"]
    if fmt == "rgb565":
        # flatten onto dark facility floor color for opaque tiles
        bg = Image.new("RGBA", crop.size, (8, 12, 20, 255))
        bg.alpha_composite(crop)
        crop = bg.convert("RGB").convert("RGBA")
        raw = pack_rgb565(crop)
        ext = "565"
        bpp = 2
    else:
        raw = pack_argb8888(crop)
        ext = "argb"
        bpp = 4
    rel = Path(fmt) / f"{name}.{ext}"
    outp = OUT / rel
    outp.parent.mkdir(parents=True, exist_ok=True)
    prev = PREV / "png" / f"{name}.png"
    prev.parent.mkdir(parents=True, exist_ok=True)
    crop.save(prev, format="PNG", optimize=False)
    if not outp.exists() or update:
        outp.write_bytes(raw)
    return {
        "name": name,
        "source_sheet": SHEET_FILES[spec["sheet"]],
        "source_box": [x, y, w, h],
        "format": fmt,
        "width": tw,
        "height": th,
        "stride": tw * bpp,
        "anchor_x": spec["ax"],
        "anchor_y": spec["ay"],
        "runtime_file": str(rel).replace("\\", "/"),
        "preview_file": f"processed/png/{name}.png",
        "sha256": hashlib.sha256(raw).hexdigest(),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--update", action="store_true", help="rewrite runtime binaries")
    ap.add_argument("--audit-only", action="store_true")
    args = ap.parse_args()
    if not SRC.is_dir():
        print("missing source/", file=sys.stderr)
        return 1
    sheets = {}
    for key, fn in SHEET_FILES.items():
        p = SRC / fn
        if not p.is_file():
            print(f"missing {p}", file=sys.stderr)
            return 1
        sheets[key] = Image.open(p).convert("RGBA")
    OUT.mkdir(parents=True, exist_ok=True)
    PREV.mkdir(parents=True, exist_ok=True)
    items = []
    for name, spec in CROPS.items():
        items.append(process_one(name, spec, sheets, args.update))
    # contact sheet
    cols = 8
    cell = 64
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * cell, rows * cell), (20, 24, 32, 255))
    for i, it in enumerate(items):
        png = PREV / "png" / f"{it['name']}.png"
        im = Image.open(png).convert("RGBA")
        im.thumbnail((cell - 4, cell - 4), Image.Resampling.NEAREST)
        cx = (i % cols) * cell + (cell - im.width) // 2
        cy = (i // cols) * cell + (cell - im.height) // 2
        sheet.alpha_composite(im, (cx, cy))
    contact = PREV / "contact_sheet.png"
    sheet.save(contact, format="PNG")
    man = {
        "package": "facility_omega_runtime",
        "version": "0.1",
        "source_intake": "assets/facility_omega/source (immutable)",
        "sprite_count": len(items),
        "sprites": items,
        "contact_sheet": "processed/contact_sheet.png",
        "notes": "Labels on source sheets are NOT part of runtime sprites.",
    }
    manp = OUT / "facility_omega_assets.json"
    manp.write_text(json.dumps(man, indent=2) + "\n", encoding="utf-8")
    AUDIT.parent.mkdir(parents=True, exist_ok=True)
    audit = {
        "candidates": [
            {"name": it["name"], "source": it["source_sheet"], "box": it["source_box"],
             "runtime_size": [it["width"], it["height"]], "format": it["format"]}
            for it in items
        ],
        "source_labels_are_runtime": False,
        "contact_sheet": "processed/contact_sheet.png",
    }
    AUDIT.write_text(json.dumps(audit, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(items)} sprites → {OUT}")
    print(f"manifest {manp}")
    print(f"contact {contact}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
