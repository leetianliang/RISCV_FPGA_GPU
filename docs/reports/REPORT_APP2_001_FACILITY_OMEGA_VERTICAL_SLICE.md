# REPORT_APP2_001 — FACILITY-Omega Vertical Slice

## 1. Result

**MAP GATE CLOSED — M13 VISUAL PASS (2026-09-16)**

关闭更新：依据 REVIEW_APP2_MAP_R3V2_VISUAL_FINAL，MAP R3 technical PASS、R3V2 visual polish complete、M13 VISUAL PASS，map-first HOLD lifted。地图基线为 57c5eb3；受控展示属于人工布置证据，残余美术问题均为非阻塞 polish。原审查结论保持历史记录，后续进入 TASK_APP2_002。

The matrix below records historical local
implementation checks; it does not constitute owner visual approval.

## 2. START_COMMIT

`758fda91d8f05be4b508da5b4da9db0e20494382`

## 3. END_COMMIT

`95c93501b557494cce3be565e99b7375d924e8a7`

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
gpu2d_test_facility* 3/3 PASS; golden+gpu2d suite (Agent-local)
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
| AST-01 | Verify source pack SHA256 | PASS | scripts/app2_asset_intake.py, assets/facility_omega/source | scripts/app2_asset_intake.py | gpu2d_test_assetc |
| AST-02 | Source sheets immutable under source/ | PASS | assets/facility_omega/source/PACK_SOURCE.txt | scripts/app2_asset_intake.py | gpu2d_test_assetc |
| AST-03 | Asset audit manifest with candidates | PASS | assets/facility_omega/processed/asset_audit.json | tools/facility_omega_assetc.py | gpu2d_test_assetc |
| AST-04 | Source labels excluded from runtime sprites | PASS | tools/facility_omega_assetc.py | assets/facility_omega/processed/contact_sheet.png | gpu2d_test_assetc |
| AST-05 | Contact sheet of processed candidates | PASS | assets/facility_omega/processed/contact_sheet.png | tools/facility_omega_assetc.py | gpu2d_test_assetc |
| AST-06 | Runtime manifest with format/anchor/hash | PASS | assets/facility_omega/runtime/facility_omega_assets.json | tools/facility_omega_assetc.py | gpu2d_test_assetc |
| AST-07 | Deterministic offline rebuild of runtime assets | PASS | tools/facility_omega_assetc.py | gpu2d_test_assetc | gpu2d_test_assetc |
| AST-08 | ARGB8888 runtime bytes are BGRA | PASS | scripts/check_app2_argb_order.py, tools/facility_omega_assetc.py | gpu2d_test_argb_order | gpu2d_test_argb_order |
| MAP-01 | 128x128 MapTile logical map | PASS | software/applications/facility_omega/include/facility/app.hpp | gpu2d_test_facility | gpu2d_test_facility |
| MAP-02 | 32x32 MapTile size | PASS | software/applications/facility_omega/include/facility/app.hpp | gpu2d_test_facility | gpu2d_test_facility |
| MAP-03 | World bounds 4096x4096 | PASS | software/applications/facility_omega/include/facility/app.hpp | gpu2d_test_facility | gpu2d_test_facility |
| MAP-04 | Player-follow camera | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| MAP-05 | Camera clamp at world edges | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| MAP-06 | Only visible MapTiles submitted | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| MAP-07 | At least 8 processed floor tiles | PASS | tools/facility_omega_assetc.py, software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| MAP-08 | At least 6 props in world | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| PLY-01 | Engineer uses processed image assets | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| PLY-02 | WASD movement | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| PLY-03 | Four directions with correct art mapping | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| PLY-04 | Two-frame walk animation | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| PLY-05 | Idle keeps facing; world boundary; camera follows | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| ENM-01 | Enemies spawn in world space | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| ENM-02 | Spawn outside camera viewport | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| ENM-03 | Deterministic chase toward player | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| ENM-04 | Drone/Crawler/Tank differ in art and stats | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility, gpu2d_test_facility_visual | gpu2d_test_facility |
| ENM-05 | Off-screen enemies not submitted to GPU | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility_visual | gpu2d_test_facility_visual |
| CMB-01 | Auto-target nearest legal enemy | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| CMB-02 | Fire Pulse Shot on cooldown | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| CMB-03 | Projectile exists in world space | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| CMB-04 | Projectile uses processed pulse_shot art | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility_visual | gpu2d_test_facility_visual |
| CMB-05 | Projectile/enemy collision and kill | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| CMB-06 | Hit flash Color Mod and kill counter | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility, gpu2d_test_facility_visual | gpu2d_test_facility |
| XP-01 | Killed enemies drop XP crystals | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| XP-02 | XP crystal uses processed image asset | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| XP-03 | Player collects nearby XP (magnet) | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| XP-04 | XP bar in HUD | PASS | software/applications/facility_omega/src/app.cpp, model/pc_demo/app/main.cpp | gpu2d_test_facility | gpu2d_test_facility |
| XP-05 | Level threshold raises level | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility | gpu2d_test_facility |
| XP-06 | Level-up pause + 3-choice upgrade + resume | PASS | software/applications/facility_omega/src/app.cpp, model/pc_demo/app/main.cpp | gpu2d_test_facility, gpu2d_test_facility_visual | gpu2d_test_facility |
| UI-01 | HUD: HP, XP, Level, Time, Kills | PASS | software/applications/facility_omega/src/app.cpp, model/pc_demo/app/main.cpp | results/facility_omega/v0_1/v3_combat.png | gpu2d_test_facility |
| UI-02 | Weapon icon in HUD | PASS | software/applications/facility_omega/src/app.cpp | results/facility_omega/v0_1/v3_combat.png | gpu2d_test_facility |
| UI-03 | Launcher exposes NEON SURVIVOR and FACILITY-O | PASS | model/pc_demo/app/main.cpp | gpu2d_demo_headless_imm | gpu2d_demo_headless_imm |
| UI-04 | Technical HUD / X-Ray toggle path | PASS | model/pc_demo/app/main.cpp | results/facility_omega/v0_1/v5_technical.png | gpu2d_demo_headless_imm |
| EQ-01 | Immediate backend executes application stream | PASS | model/pc_demo/golden_backend/golden_renderer.cpp | gpu2d_test_facility_visual | gpu2d_test_facility_visual |
| EQ-02 | Tile32 backend executes same stream | PASS | model/pc_demo/golden_backend/golden_renderer.cpp | gpu2d_test_facility_visual | gpu2d_test_facility_visual |
| EQ-03 | Same application code both backends | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility_visual | gpu2d_test_facility_visual |
| EQ-04 | 60 frames Immediate==Tile including level-up UI | PASS | model/pc_demo/tests/test_facility_visual.cpp | gpu2d_test_facility_visual | gpu2d_test_facility_visual |
| SYS-01 | Fixed seed+script reproduces simulation hash | PASS | software/applications/facility_omega/src/app.cpp | gpu2d_test_facility, gpu2d_test_facility_stability | gpu2d_test_facility_stability |
| SYS-02 | Fixed seed+frame reproduces framebuffer hash | PASS | model/pc_demo/tests/test_facility_visual.cpp | gpu2d_test_facility_visual | gpu2d_test_facility_visual |
| SYS-03 | 600-frame headless gameplay smoke | PASS | model/pc_demo/tests/test_facility_stability.cpp | gpu2d_test_facility_stability | gpu2d_test_facility_stability |
| SYS-04 | Asset manifest hashes / source unchanged | PASS | assets/facility_omega/runtime/facility_omega_assets.json | gpu2d_test_assetc, gpu2d_test_argb_order | gpu2d_test_assetc |
| SYS-05 | Existing NEON and Golden regressions pass | PASS | model/golden/tests/, software/applications/neon_survivor/ | golden_test_tile_faults, gpu2d_test_system | golden_test_tile_faults |
| VIS-01 | Asset contact sheet | PASS | assets/facility_omega/processed/contact_sheet.png | gpu2d_test_assetc | gpu2d_test_assetc |
| VIS-02 | Large map screenshot (scrolling world) | PASS | results/facility_omega/v0_1/v2_large_map.png | scripts/capture_facility_omega_v1.py | gpu2d_demo_headless_imm |
| VIS-03 | Combat screenshot (player/enemies/shots) | PASS | results/facility_omega/v0_1/v3_combat.png | scripts/capture_facility_omega_v1.py | gpu2d_demo_headless_imm |
| VIS-04 | Level-up fixture screenshot | PASS | results/facility_omega/v0_1/v4_levelup_fixture.png | gpu2d_test_facility_visual | gpu2d_test_facility_visual |
| VIS-05 | Technical / X-Ray screenshot | PASS | results/facility_omega/v0_1/v5_technical.png | scripts/capture_facility_omega_v1.py | gpu2d_demo_headless_imm |

## 13. Git Status

Implementation + this report are committed on `master`. Untracked review documents may accompany the push as review artifacts.
