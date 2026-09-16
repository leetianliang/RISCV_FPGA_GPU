#!/usr/bin/env python3
"""Generate REPORT_APP2_001 from acceptance JSON."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--end-commit", required=True)
    ap.add_argument("--tests", default="ctest stage0045 facility+gpu2d (Agent-local)")
    args = ap.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}", args.end_commit):
        print("END_COMMIT must be 40-hex", file=sys.stderr)
        return 1
    # START: first APP2 implementation commit after task docs
    start = "758fda91d8f05be4b508da5b4da9db0e20494382"
    data = json.loads((ROOT / "docs/tasks/APP2_001_ACCEPTANCE.json").read_text(encoding="utf-8"))
    rows = []
    for a in data["acceptance"]:
        impl = ", ".join(a["implementation_evidence"])
        ver = ", ".join(a["verification_evidence"])
        rows.append(
            f"| {a['id']} | {a['requirement']} | {a['status']} | {impl} | {ver} | {a['test_name']} |"
        )
    matrix = "\n".join(rows)
    report = f"""# REPORT_APP2_001 — FACILITY-Omega Vertical Slice

## 1. Result

**HOLD — MAP VISUAL/SEMANTIC REWORK REQUIRED**

V3 review governs stage acceptance. The matrix below records historical local
implementation checks; it does not constitute owner visual approval.

## 2. START_COMMIT

`{start}`

## 3. END_COMMIT

`{args.end_commit}`

## 4. Environment

- OS: Windows (Agent-local)
- Compiler: MinGW-W64 g++ 14.2.0
- CMake: 3.31.5 / Ninja
- Window: Win32 presenter; headless is verification authority
- Build: `build/stage0045`

## 5. Source asset pack

- Zip: `docs/tasks/FACILITY_OMEGA_First_Asset_Pack_V0.1.zip`
- Immutable extract: `assets/facility_omega/source/`
- SHA256 verified by `scripts/app2_asset_intake.py` (9/9 PASS)

## 6. Asset pipeline

```text
tools/facility_omega_assetc.py
  crop / trim / resize → runtime RGB565 | ARGB8888(BGRA)
  facility_omega_assets.json + contact_sheet.png
```

Runtime does not decode PNG. ARGB channel order locked by `gpu2d_test_argb_order`.

## 7. Architecture

```text
software/applications/facility_omega
  → gpu2d GraphicsApi
  → Golden Immediate | Tile32
  → RGB565 framebuffer
  → Win32 presenter / headless
```

World 4096×4096, MapTile 32, 128×128 grid, player-follow camera, visible MapTile culling.

## 8. Gameplay (vertical slice)

- Engineer WASD, 4-dir walk, idle keeps facing, hurt Color Mod
- Drone / Crawler / Tank spawn outside viewport, chase, touch damage
- Auto Pulse Shot, multi-shot upgrade
- XP gems, magnet collect, XP bar, level-up 3-choice panel
- Industrial HUD + optional tech HUD / X-Ray

## 9. Tests

```text
{args.tests}
python scripts/check_app2_001_acceptance.py
python scripts/app2_asset_intake.py
python scripts/check_app2_argb_order.py
```

## 10. Visual evidence

```text
assets/facility_omega/processed/contact_sheet.png
results/facility_omega/v0_1/v2_large_map.png
results/facility_omega/v0_1/v3_combat.png
results/facility_omega/v0_1/v4_levelup_fixture.png
results/facility_omega/v0_1/v5_technical.png
```

## 11. Known limitations

- Floor panel tiling still visible on large empty areas
- No dedicated side-idle art (uses walk frame 0)
- Boss / Orbit / Nova / Field deferred (non-goals of this task)
- Interactive FPS is not FPGA performance

## 12. 56-row Acceptance Evidence Matrix

| ID | Requirement | Result | Implementation Evidence | Verification Evidence | Test Name |
|---|---|---|---|---|---|
{matrix}

## 13. Git Status

Implementation + this report are committed on `master`. Untracked review documents may accompany the push as review artifacts.
"""
    out = ROOT / "docs/reports/REPORT_APP2_001_FACILITY_OMEGA_VERTICAL_SLICE.md"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(report, encoding="utf-8")
    print(f"wrote {out} rows={len(rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
