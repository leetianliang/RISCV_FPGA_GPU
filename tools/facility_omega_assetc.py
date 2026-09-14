#!/usr/bin/env python3
"""FACILITY-Omega offline asset compiler.

R1: ARGB8888 emitted as BGRA (Golden little-endian 0xAARRGGBB).
R3-R5: one pose per runtime sprite; contact sheet for human review.
Deterministic; --update rewrites checked binaries.
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

# Visual-review crop table. Each entry is ONE isolated object.
# box = (x, y, w, h) on source sheet; trim_bottom drops label strip.
CROPS: dict[str, dict] = {
    # Player: a=UP b=DOWN c=LEFT d=RIGHT (see source labels)
    "engineer_a0": {"sheet": "player", "box": (516, 40, 142, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.22},
    "engineer_a1": {"sheet": "player", "box": (794, 40, 142, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.22},
    "engineer_b0": {"sheet": "player", "box": (524, 295, 131, 190), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.22},
    "engineer_b1": {"sheet": "player", "box": (798, 295, 134, 190), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.22},
    "engineer_c0": {"sheet": "player", "box": (307, 535, 192, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.22},
    "engineer_c1": {"sheet": "player", "box": (539, 535, 175, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.22},
    "engineer_d0": {"sheet": "player", "box": (792, 535, 176, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.22},
    "engineer_d1": {"sheet": "player", "box": (999, 535, 184, 200), "tw": 32, "th": 32,
                    "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.22},
    "engineer_idle": {"sheet": "player", "box": (236, 808, 176, 200), "tw": 32, "th": 32,
                      "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.22},
    "engineer_hurt": {"sheet": "player", "box": (519, 808, 191, 200), "tw": 32, "th": 32,
                      "fmt": "argb8888", "ax": 16, "ay": 28, "trim_bottom": 0.22},
    # Enemies: split left/right pose in each labeled cell
    "drone_0": {"sheet": "enemies", "box": (67, 90, 190, 200), "tw": 28, "th": 28,
                "fmt": "argb8888", "ax": 14, "ay": 14, "trim_bottom": 0.18},
    "drone_1": {"sheet": "enemies", "box": (260, 90, 190, 200), "tw": 28, "th": 28,
                "fmt": "argb8888", "ax": 14, "ay": 14, "trim_bottom": 0.18},
    "crawler_0": {"sheet": "enemies", "box": (495, 90, 225, 200), "tw": 32, "th": 32,
                  "fmt": "argb8888", "ax": 16, "ay": 22, "trim_bottom": 0.18},
    "crawler_1": {"sheet": "enemies", "box": (730, 90, 225, 200), "tw": 32, "th": 32,
                  "fmt": "argb8888", "ax": 16, "ay": 22, "trim_bottom": 0.18},
    "runner_0": {"sheet": "enemies", "box": (1028, 90, 175, 200), "tw": 32, "th": 32,
                 "fmt": "argb8888", "ax": 16, "ay": 22, "trim_bottom": 0.18},
    "runner_1": {"sheet": "enemies", "box": (1210, 90, 175, 200), "tw": 32, "th": 32,
                 "fmt": "argb8888", "ax": 16, "ay": 22, "trim_bottom": 0.18},
    "tank_0": {"sheet": "enemies", "box": (39, 390, 345, 330), "tw": 48, "th": 48,
               "fmt": "argb8888", "ax": 24, "ay": 32, "trim_bottom": 0.12},
    "tank_1": {"sheet": "enemies", "box": (395, 390, 345, 330), "tw": 48, "th": 48,
               "fmt": "argb8888", "ax": 24, "ay": 32, "trim_bottom": 0.12},
    "elite_0": {"sheet": "enemies", "box": (811, 390, 290, 330), "tw": 48, "th": 48,
                "fmt": "argb8888", "ax": 24, "ay": 32, "trim_bottom": 0.12},
    "elite_1": {"sheet": "enemies", "box": (1110, 390, 290, 330), "tw": 48, "th": 48,
                "fmt": "argb8888", "ax": 24, "ay": 32, "trim_bottom": 0.12},
    # Weapons — single projectile / crystal / pickup
    "pulse_shot": {"sheet": "weapons", "box": (21, 55, 90, 70), "tw": 8, "th": 8,
                   "fmt": "argb8888", "ax": 4, "ay": 4},
    "enemy_bullet": {"sheet": "weapons", "box": (850, 55, 90, 70), "tw": 8, "th": 8,
                     "fmt": "argb8888", "ax": 4, "ay": 4},
    "xp_small": {"sheet": "weapons", "box": (25, 780, 70, 70), "tw": 8, "th": 8,
                 "fmt": "argb8888", "ax": 4, "ay": 4},
    "xp_large": {"sheet": "weapons", "box": (370, 755, 100, 110), "tw": 12, "th": 12,
                 "fmt": "argb8888", "ax": 6, "ay": 6},
    "repair_pickup": {"sheet": "weapons", "box": (1090, 760, 90, 90), "tw": 16, "th": 16,
                      "fmt": "argb8888", "ax": 8, "ay": 8},
    # FX — isolated single effects
    "glow_small": {"sheet": "fx", "box": (40, 50, 160, 160), "tw": 16, "th": 16,
                   "fmt": "argb8888", "ax": 8, "ay": 8},
    "glow_large": {"sheet": "fx", "box": (280, 40, 220, 220), "tw": 32, "th": 32,
                   "fmt": "argb8888", "ax": 16, "ay": 16},
    "spark": {"sheet": "fx", "box": (620, 50, 180, 180), "tw": 16, "th": 16,
              "fmt": "argb8888", "ax": 8, "ay": 8},
    "explosion": {"sheet": "fx", "box": (320, 620, 280, 280), "tw": 32, "th": 32,
                  "fmt": "argb8888", "ax": 16, "ay": 16},
    "ring": {"sheet": "fx", "box": (980, 380, 220, 220), "tw": 32, "th": 32,
             "fmt": "argb8888", "ax": 16, "ay": 16},
    "trail": {"sheet": "fx", "box": (20, 400, 280, 130), "tw": 16, "th": 8,
              "fmt": "argb8888", "ax": 8, "ay": 4},
    # Environment — base floors (quiet metals) vs specials (sparse)
    "floor_00": {"sheet": "environment", "box": (19, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0, "role": "base"},
    "floor_01": {"sheet": "environment", "box": (190, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0, "role": "base"},
    "floor_02": {"sheet": "environment", "box": (600, 490, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0, "role": "base"},
    "floor_03": {"sheet": "environment", "box": (1000, 490, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0, "role": "base"},
    "floor_04": {"sheet": "environment", "box": (360, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0, "role": "special"},
    "floor_05": {"sheet": "environment", "box": (800, 490, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0, "role": "special"},
    "floor_06": {"sheet": "environment", "box": (360, 280, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0, "role": "special"},
    "floor_07": {"sheet": "environment", "box": (530, 70, 140, 140), "tw": 32, "th": 32,
                 "fmt": "rgb565", "ax": 0, "ay": 0, "role": "decal"},
    "hazard_stripe": {"sheet": "environment", "box": (200, 1000, 200, 160), "tw": 32, "th": 16,
                      "fmt": "rgb565", "ax": 0, "ay": 0, "role": "decal"},
    "grate": {"sheet": "environment", "box": (360, 70, 140, 140), "tw": 32, "th": 32,
              "fmt": "rgb565", "ax": 0, "ay": 0, "role": "decal"},
    "barrel": {"sheet": "environment", "box": (25, 680, 150, 220), "tw": 24, "th": 32,
               "fmt": "argb8888", "ax": 12, "ay": 28, "trim_bottom": 0.05},
    "crate": {"sheet": "environment", "box": (200, 700, 170, 180), "tw": 28, "th": 28,
              "fmt": "argb8888", "ax": 14, "ay": 24, "trim_bottom": 0.05},
    "console": {"sheet": "environment", "box": (400, 680, 170, 220), "tw": 28, "th": 36,
                "fmt": "argb8888", "ax": 14, "ay": 32, "trim_bottom": 0.05},
    "canister": {"sheet": "environment", "box": (600, 680, 130, 220), "tw": 20, "th": 28,
                 "fmt": "argb8888", "ax": 10, "ay": 24, "trim_bottom": 0.05},
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


def pack_argb8888_bgra(im: Image.Image) -> bytes:
    # Golden: little-endian 0xAARRGGBB → memory B,G,R,A
    return im.tobytes("raw", "BGRA")


def process_one(name: str, spec: dict, sheets: dict[str, Image.Image], update: bool) -> dict:
    sh = sheets[spec["sheet"]]
    x, y, w, h = spec["box"]
    crop = sh.crop((x, y, x + w, y + h)).convert("RGBA")
    tb = float(spec.get("trim_bottom", 0.0))
    if tb > 0:
        crop = crop.crop((0, 0, crop.width, max(1, int(crop.height * (1.0 - tb)))))
    a = crop.getchannel("A")
    bbox = a.getbbox()
    if bbox:
        crop = crop.crop(bbox)
    tw, th = spec["tw"], spec["th"]
    crop = crop.resize((tw, th), Image.Resampling.LANCZOS)
    fmt = spec["fmt"]
    if fmt == "rgb565":
        bg = Image.new("RGBA", crop.size, (8, 12, 20, 255))
        bg.alpha_composite(crop)
        crop = bg.convert("RGB").convert("RGBA")
        raw = pack_rgb565(crop)
        ext = "565"
        bpp = 2
    else:
        raw = pack_argb8888_bgra(crop)
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
        "role": spec.get("role", "sprite"),
        "runtime_file": str(rel).replace("\\", "/"),
        "preview_file": f"processed/png/{name}.png",
        "sha256": hashlib.sha256(raw).hexdigest(),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--update", action="store_true")
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
    items = [process_one(n, s, sheets, args.update) for n, s in CROPS.items()]
    # labeled contact sheet for human review
    cols = 6
    cell = 80
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * cell, rows * (cell + 14)), (24, 28, 36, 255))
    for i, it in enumerate(items):
        png = PREV / "png" / f"{it['name']}.png"
        im = Image.open(png).convert("RGBA")
        im.thumbnail((cell - 8, cell - 8), Image.Resampling.NEAREST)
        cx = (i % cols) * cell + (cell - im.width) // 2
        cy = (i // cols) * (cell + 14) + 2
        sheet.alpha_composite(im, (cx, cy))
        # label bar (placeholder name strip — not part of runtime)
        ImageDraw_stub = None
        del ImageDraw_stub
    contact = PREV / "contact_sheet.png"
    sheet.save(contact, format="PNG")
    man = {
        "package": "facility_omega_runtime",
        "version": "0.2",
        "argb_byte_order": "BGRA",
        "sprite_count": len(items),
        "sprites": items,
        "contact_sheet": "processed/contact_sheet.png",
    }
    manp = OUT / "facility_omega_assets.json"
    manp.write_text(json.dumps(man, indent=2) + "\n", encoding="utf-8")
    audit = {
        "candidates": [
            {"name": it["name"], "source": it["source_sheet"], "box": it["source_box"],
             "runtime_size": [it["width"], it["height"]], "format": it["format"],
             "role": it.get("role", "sprite")}
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
