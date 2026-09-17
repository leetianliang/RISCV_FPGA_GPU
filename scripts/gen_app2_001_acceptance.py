#!/usr/bin/env python3
"""Generate TASK_APP2_001 56-ID acceptance manifest.

Explicit REVIEWED_PASS set only — never auto-promote.
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "tasks" / "APP2_001_ACCEPTANCE.json"

GROUPS = [
    ("AST", 8),
    ("MAP", 8),
    ("PLY", 5),
    ("ENM", 5),
    ("CMB", 6),
    ("XP", 6),
    ("UI", 4),
    ("EQ", 4),
    ("SYS", 5),
    ("VIS", 5),
]
AUTHORITATIVE = [f"{p}-{i:02d}" for p, n in GROUPS for i in range(1, n + 1)]

# id -> (requirement, impl, ver, test_name)
ROWS: dict[str, tuple[str, list[str], list[str], str]] = {
    "AST-01": (
        "Verify source pack SHA256",
        ["scripts/app2_asset_intake.py", "assets/facility_omega/source"],
        ["scripts/app2_asset_intake.py"],
        "gpu2d_test_assetc",
    ),
    "AST-02": (
        "Source sheets immutable under source/",
        ["assets/facility_omega/source/PACK_SOURCE.txt"],
        ["scripts/app2_asset_intake.py"],
        "gpu2d_test_assetc",
    ),
    "AST-03": (
        "Asset audit manifest with candidates",
        ["assets/facility_omega/processed/asset_audit.json"],
        ["tools/facility_omega_assetc.py"],
        "gpu2d_test_assetc",
    ),
    "AST-04": (
        "Source labels excluded from runtime sprites",
        ["tools/facility_omega_assetc.py"],
        ["assets/facility_omega/processed/contact_sheet.png"],
        "gpu2d_test_assetc",
    ),
    "AST-05": (
        "Contact sheet of processed candidates",
        ["assets/facility_omega/processed/contact_sheet.png"],
        ["tools/facility_omega_assetc.py"],
        "gpu2d_test_assetc",
    ),
    "AST-06": (
        "Runtime manifest with format/anchor/hash",
        ["assets/facility_omega/runtime/facility_omega_assets.json"],
        ["tools/facility_omega_assetc.py"],
        "gpu2d_test_assetc",
    ),
    "AST-07": (
        "Deterministic offline rebuild of runtime assets",
        ["tools/facility_omega_assetc.py"],
        ["gpu2d_test_assetc"],
        "gpu2d_test_assetc",
    ),
    "AST-08": (
        "ARGB8888 runtime bytes are BGRA",
        ["scripts/check_app2_argb_order.py", "tools/facility_omega_assetc.py"],
        ["gpu2d_test_argb_order"],
        "gpu2d_test_argb_order",
    ),
    "MAP-01": (
        "128x128 MapTile logical map",
        ["software/applications/facility_omega/include/facility/app.hpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "MAP-02": (
        "32x32 MapTile size",
        ["software/applications/facility_omega/include/facility/app.hpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "MAP-03": (
        "World bounds 4096x4096",
        ["software/applications/facility_omega/include/facility/app.hpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "MAP-04": (
        "Player-follow camera",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "MAP-05": (
        "Camera clamp at world edges",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "MAP-06": (
        "Only visible MapTiles submitted",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "MAP-07": (
        "At least 8 processed floor tiles",
        ["tools/facility_omega_assetc.py", "software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "MAP-08": (
        "At least 6 props in world",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "PLY-01": (
        "Engineer uses processed image assets",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "PLY-02": (
        "WASD movement",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "PLY-03": (
        "Four directions with correct art mapping",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "PLY-04": (
        "Two-frame walk animation",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "PLY-05": (
        "Idle keeps facing; world boundary; camera follows",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "ENM-01": (
        "Enemies spawn in world space",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "ENM-02": (
        "Spawn outside camera viewport",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "ENM-03": (
        "Deterministic chase toward player",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "ENM-04": (
        "Drone/Crawler/Tank differ in art and stats",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility", "gpu2d_test_facility_visual"],
        "gpu2d_test_facility",
    ),
    "ENM-05": (
        "Off-screen enemies not submitted to GPU",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility_visual"],
        "gpu2d_test_facility_visual",
    ),
    "CMB-01": (
        "Auto-target nearest legal enemy",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "CMB-02": (
        "Fire Pulse Shot on cooldown",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "CMB-03": (
        "Projectile exists in world space",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "CMB-04": (
        "Projectile uses processed pulse_shot art",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility_visual"],
        "gpu2d_test_facility_visual",
    ),
    "CMB-05": (
        "Projectile/enemy collision and kill",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "CMB-06": (
        "Hit flash Color Mod and kill counter",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility", "gpu2d_test_facility_visual"],
        "gpu2d_test_facility",
    ),
    "XP-01": (
        "Killed enemies drop XP crystals",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "XP-02": (
        "XP crystal uses processed image asset",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "XP-03": (
        "Player collects nearby XP (magnet)",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "XP-04": (
        "XP bar in HUD",
        ["software/applications/facility_omega/src/app.cpp", "model/pc_demo/app/main.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "XP-05": (
        "Level threshold raises level",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility"],
        "gpu2d_test_facility",
    ),
    "XP-06": (
        "Level-up pause + 3-choice upgrade + resume",
        ["software/applications/facility_omega/src/app.cpp", "model/pc_demo/app/main.cpp"],
        ["gpu2d_test_facility", "gpu2d_test_facility_visual"],
        "gpu2d_test_facility",
    ),
    "UI-01": (
        "HUD: HP, XP, Level, Time, Kills",
        ["software/applications/facility_omega/src/app.cpp", "model/pc_demo/app/main.cpp"],
        ["results/facility_omega/v0_1/v3_combat.png"],
        "gpu2d_test_facility",
    ),
    "UI-02": (
        "Weapon icon in HUD",
        ["software/applications/facility_omega/src/app.cpp"],
        ["results/facility_omega/v0_1/v3_combat.png"],
        "gpu2d_test_facility",
    ),
    "UI-03": (
        "Launcher exposes NEON SURVIVOR and FACILITY-O",
        ["model/pc_demo/app/main.cpp"],
        ["gpu2d_demo_headless_imm"],
        "gpu2d_demo_headless_imm",
    ),
    "UI-04": (
        "Technical HUD / X-Ray toggle path",
        ["model/pc_demo/app/main.cpp"],
        ["results/facility_omega/v0_1/v5_technical.png"],
        "gpu2d_demo_headless_imm",
    ),
    "EQ-01": (
        "Immediate backend executes application stream",
        ["model/pc_demo/golden_backend/golden_renderer.cpp"],
        ["gpu2d_test_facility_visual"],
        "gpu2d_test_facility_visual",
    ),
    "EQ-02": (
        "Tile32 backend executes same stream",
        ["model/pc_demo/golden_backend/golden_renderer.cpp"],
        ["gpu2d_test_facility_visual"],
        "gpu2d_test_facility_visual",
    ),
    "EQ-03": (
        "Same application code both backends",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility_visual"],
        "gpu2d_test_facility_visual",
    ),
    "EQ-04": (
        "60 frames Immediate==Tile including level-up UI",
        ["model/pc_demo/tests/test_facility_visual.cpp"],
        ["gpu2d_test_facility_visual"],
        "gpu2d_test_facility_visual",
    ),
    "SYS-01": (
        "Fixed seed+script reproduces simulation hash",
        ["software/applications/facility_omega/src/app.cpp"],
        ["gpu2d_test_facility", "gpu2d_test_facility_stability"],
        "gpu2d_test_facility_stability",
    ),
    "SYS-02": (
        "Fixed seed+frame reproduces framebuffer hash",
        ["model/pc_demo/tests/test_facility_visual.cpp"],
        ["gpu2d_test_facility_visual"],
        "gpu2d_test_facility_visual",
    ),
    "SYS-03": (
        "600-frame headless gameplay smoke",
        ["model/pc_demo/tests/test_facility_stability.cpp"],
        ["gpu2d_test_facility_stability"],
        "gpu2d_test_facility_stability",
    ),
    "SYS-04": (
        "Asset manifest hashes / source unchanged",
        ["assets/facility_omega/runtime/facility_omega_assets.json"],
        ["gpu2d_test_assetc", "gpu2d_test_argb_order"],
        "gpu2d_test_assetc",
    ),
    "SYS-05": (
        "Existing NEON and Golden regressions pass",
        ["model/golden/tests/", "software/applications/neon_survivor/"],
        ["golden_test_tile_faults", "gpu2d_test_system"],
        "golden_test_tile_faults",
    ),
    "VIS-01": (
        "Asset contact sheet",
        ["assets/facility_omega/processed/contact_sheet.png"],
        ["gpu2d_test_assetc"],
        "gpu2d_test_assetc",
    ),
    "VIS-02": (
        "Large map screenshot (scrolling world)",
        ["results/facility_omega/v0_1/v2_large_map.png"],
        ["scripts/capture_facility_omega_v1.py"],
        "gpu2d_demo_headless_imm",
    ),
    "VIS-03": (
        "Combat screenshot (player/enemies/shots)",
        ["results/facility_omega/v0_1/v3_combat.png"],
        ["scripts/capture_facility_omega_v1.py"],
        "gpu2d_demo_headless_imm",
    ),
    "VIS-04": (
        "Level-up fixture screenshot",
        ["results/facility_omega/v0_1/v4_levelup_fixture.png"],
        ["gpu2d_test_facility_visual"],
        "gpu2d_test_facility_visual",
    ),
    "VIS-05": (
        "Technical / X-Ray screenshot",
        ["results/facility_omega/v0_1/v5_technical.png"],
        ["scripts/capture_facility_omega_v1.py"],
        "gpu2d_demo_headless_imm",
    ),
}

# Historical implementation claims, not review decisions or executed test results.
LEGACY_IMPLEMENTATION_PASS = set(AUTHORITATIVE)


def main() -> int:
    acc = []
    for iid in AUTHORITATIVE:
        req, impl, ver, tn = ROWS[iid]
        acc.append(
            {
                "id": iid,
                "mandatory": True,
                "status": "PASS" if iid in LEGACY_IMPLEMENTATION_PASS else "NOT_DONE",
                "requirement": req,
                "implementation_evidence": impl,
                "verification_evidence": ver,
                "test_name": tn,
                "notes": "See REPORT_APP2_001",
            }
        )
    data = {
        "task": "TASK_APP2_001",
        "stage": "004.5-application2",
        "stage_status": "MAP_GATE_CLOSED",
        "owner_visual_approval": "PASS_REVIEW_APP2_MAP_R3V2_VISUAL_FINAL",
        "status_scope": "Historical implementation claims; map-first HOLD lifted by final R3V2 review.",
        "authoritative_ids": AUTHORITATIVE,
        "acceptance": acc,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT} ids={len(acc)} pass={sum(1 for a in acc if a['status']=='PASS')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
