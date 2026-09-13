# REPORT_0045 — PC Golden Interactive Application

## 1. Result

**PASS** (pending REVIEW_0045)

## 2. START_COMMIT

`f034f01ba913c44ce749e7756bd682008f75d08a`

## 3. END_COMMIT

`ff282315fd0f2f6fcd8b1c401155d0386c33f28a`

## 4. Environment

- OS: Windows 10/11 (Agent-local)
- Compiler: MinGW-W64 g++ 14.2.0
- CMake: 3.31.5
- Window library: Win32 API (no SDL; Golden Core stays GUI-independent)
- Build: Ninja Release, `build/stage0045`

## 5. Implemented Architecture

```text
NEON SURVIVOR (software/applications/neon_survivor)
        │
        ▼
gpu2d Graphics API + CommandRecorder (software/graphics)
        │
        ├─ Immediate Golden Backend
        └─ Tile32 Golden Backend (bin → TILE_FRAME)
                │
                ▼
        Golden Framebuffer (RGB565)
                │
                ▼
        Win32 Presenter (host only; no game drawing)
```

## 6. Implemented Game Content

- Player WASD/HP/auto-attack
- Normal/Fast/Heavy enemies
- Straight / radial / spiral projectiles
- Particles: alpha fade + additive glow + scaled
- Procedural neon assets + GPU bitmap font HUD
- Scenes: game, sprite/alpha/bullet/scale/overdraw stress

## 7. Graphics API / Backend Separation Evidence

- Game code has zero Golden includes (`gpu2d_test_boundary`)
- Same `RecCommand` stream feeds Immediate and Tile
- Handles only; no physical addresses in game

## 8. Immediate-vs-Tile Application Equality

- `gpu2d_test_backend`: integrated scene byte-equal
- `gpu2d_test_system`: 100 deterministic frames byte-equal

## 9. Automated Tests

```text
cmake -S . -B build/stage0045 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/stage0045 --parallel
ctest --test-dir build/stage0045 --output-on-failure
80/80 PASS
python scripts/check_stage0045_acceptance.py
python scripts/check_app_boundary.py
```

## 10. Interactive Demo Controls

```text
WASD move | F1-F5 stress | F6 Immediate | F7 Tile32
F8 tech HUD | F9 normal | F10 X-Ray | P pause | R reset | ESC quit
```

CLI: `--headless --frames N --seed N --backend immediate|tile --profile interactive|showcase --scene game|sprite|alpha|bullet|scale|overdraw --capture path --xray`

## 11. Stress Modes

See `results/stage0045/stress/stress_stats.md` (labeled **Functional PC Golden workload statistics**).

## 12. Visual Captures

```text
results/stage0045/captures/v1_game_showcase.raw (+ .ppm) 1280x720
results/stage0045/captures/v2_effects.raw
results/stage0045/captures/v3_xray.raw
results/stage0045/captures/v4_overdraw_stress.raw
```

## 13. Known Limitations

- PC host FPS is not FPGA performance.
- Interactive window is Win32-only for this stage; headless is the authority.
- Stress unit-test thresholds are lower than demo targets for CI speed; CLI demo reaches higher density.
- X-Ray grid overlay draws many fills (demo/debug layer).

## 14. Technical Debt / Follow-up

- Optional SDL2 presenter for non-Windows hosts
- Richer launcher menu (App 2/3 placeholders)
- Stronger stress thresholds in CTest when runtime budget allows

## 15. 56-row Acceptance Evidence Matrix

| ID | Requirement | Result | Implementation Evidence | Verification Evidence | Test Name |
|---|---|---|---|---|---|
| HOST-01 | PC window displays project RGB565 framebuffer | PASS | model/pc_demo/host/presenter.cpp | gpu2d_test_presenter, gpu2d_demo_headless_imm | gpu2d_test_presenter |
| HOST-02 | Input and clean shutdown | PASS | model/pc_demo/host/presenter.cpp, model/pc_demo/app/main.cpp | gpu2d_test_presenter | gpu2d_test_presenter |
| HOST-03 | 640x360 and 1280x720 paths | PASS | model/pc_demo/host/presenter.cpp | gpu2d_test_presenter | gpu2d_test_presenter |
| HOST-04 | Headless mode requires no window | PASS | model/pc_demo/host/presenter.cpp | gpu2d_test_presenter, gpu2d_demo_headless_imm | gpu2d_test_presenter |
| HOST-05 | Golden Core free of window/SDL deps | PASS | scripts/check_app_boundary.py, model/golden/CMakeLists.txt | gpu2d_test_boundary, golden_test_integrity | gpu2d_test_boundary |
| API-01 | Common Graphics API application-facing | PASS | software/graphics/include/gpu2d/graphics_api.hpp | gpu2d_test_api | gpu2d_test_api |
| API-02 | Game uses handles not Golden addresses | PASS | software/graphics/include/gpu2d/types.hpp, scripts/check_app_boundary.py | gpu2d_test_boundary, gpu2d_test_api | gpu2d_test_boundary |
| API-03 | Fill/Sprite/Alpha/Additive/Scale/Clip/Palette expressible | PASS | software/graphics/include/gpu2d/graphics_api.hpp | gpu2d_test_api, gpu2d_test_backend | gpu2d_test_api |
| API-04 | Command order deterministic | PASS | software/graphics/include/gpu2d/renderer.hpp | gpu2d_test_api | gpu2d_test_api |
| API-05 | API stream backend-neutral | PASS | model/pc_demo/golden_backend/golden_renderer.cpp | gpu2d_test_backend, gpu2d_test_system | gpu2d_test_backend |
| API-06 | No game-specific GPU opcode or GpuCmd construction in app | PASS | scripts/check_app_boundary.py | gpu2d_test_boundary | gpu2d_test_boundary |
| BACK-01 | Immediate backend executes app streams | PASS | model/pc_demo/golden_backend/golden_renderer.cpp | gpu2d_test_backend | gpu2d_test_backend |
| BACK-02 | Tile32 backend executes same streams | PASS | model/pc_demo/golden_backend/golden_renderer.cpp | gpu2d_test_backend | gpu2d_test_backend |
| BACK-03 | Integrated scene Immediate==Tile byte-for-byte | PASS | model/pc_demo/tests/test_backend.cpp | gpu2d_test_backend, gpu2d_test_system | gpu2d_test_backend |
| BACK-04 | Runtime backend switching | PASS | model/pc_demo/golden_backend/golden_renderer.cpp, model/pc_demo/tests/test_system.cpp | gpu2d_test_system | gpu2d_test_system |
| BACK-05 | Resource upload backend-owned | PASS | model/pc_demo/golden_backend/golden_renderer.cpp | gpu2d_test_backend | gpu2d_test_backend |
| BACK-06 | RendererTelemetry abstraction | PASS | software/graphics/include/gpu2d/telemetry.hpp | gpu2d_test_api, gpu2d_test_system | gpu2d_test_api |
| SIM-01 | Fixed-timestep simulation | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_sim | gpu2d_test_sim |
| SIM-02 | Fixed-seed deterministic mode | PASS | software/applications/neon_survivor/include/neon/sim.hpp | gpu2d_test_sim | gpu2d_test_sim |
| SIM-03 | Scripted/headless input path | PASS | model/pc_demo/app/main.cpp | gpu2d_test_sim, gpu2d_demo_headless_imm | gpu2d_test_sim |
| SIM-04 | Reset restores initial state | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_sim | gpu2d_test_sim |
| SIM-05 | Rendering backend does not alter gameplay state | PASS | software/applications/neon_survivor/src/sim.cpp, model/pc_demo/tests/test_system.cpp | gpu2d_test_system | gpu2d_test_system |
| GAME-01 | Player movement and HP | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_sim | gpu2d_test_sim |
| GAME-02 | Normal/Fast/Heavy enemy classes | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_sim | gpu2d_test_sim |
| GAME-03 | Straight projectile | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_sim | gpu2d_test_sim |
| GAME-04 | Radial/spread projectile pattern | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_sim | gpu2d_test_sim |
| GAME-05 | Spiral/bullet-hell pattern | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_sim | gpu2d_test_sim |
| GAME-06 | Spawn/damage/kill/score playable loop | PASS | software/applications/neon_survivor/src/sim.cpp, model/pc_demo/app/main.cpp | gpu2d_test_sim, gpu2d_demo_headless_imm | gpu2d_test_sim |
| FX-01 | Alpha-fade particle/trail | PASS | software/applications/neon_survivor/src/assets.cpp | gpu2d_test_system, gpu2d_test_backend | gpu2d_test_backend |
| FX-02 | Additive glow/explosion | PASS | software/applications/neon_survivor/src/assets.cpp | gpu2d_test_system, gpu2d_test_backend | gpu2d_test_backend |
| FX-03 | Scaling and bilinear effect | PASS | software/applications/neon_survivor/src/assets.cpp | gpu2d_test_backend | gpu2d_test_backend |
| FX-04 | Color Mod / Palette damage or variant | PASS | software/applications/neon_survivor/src/assets.cpp | gpu2d_test_backend | gpu2d_test_backend |
| FX-05 | Color Key + Indexed8/Palette paths exercised | PASS | software/applications/neon_survivor/src/assets.cpp, model/pc_demo/tests/test_system.cpp | gpu2d_test_system | gpu2d_test_system |
| FX-06 | GPU-rendered HUD with telemetry | PASS | software/applications/neon_survivor/src/assets.cpp | gpu2d_test_system, gpu2d_demo_headless_imm | gpu2d_test_system |
| XR-01 | Tile grid visualization | PASS | software/applications/neon_survivor/src/assets.cpp | gpu2d_test_system | gpu2d_test_system |
| XR-02 | Per-Tile WorkRef visualization | PASS | software/applications/neon_survivor/src/assets.cpp | gpu2d_test_system | gpu2d_test_system |
| XR-03 | Active Tile visualization | PASS | software/applications/neon_survivor/src/assets.cpp | gpu2d_test_system | gpu2d_test_system |
| XR-04 | Overdraw visualization when available | PASS | software/applications/neon_survivor/src/assets.cpp | gpu2d_test_system | gpu2d_test_system |
| XR-05 | Normal/X-Ray switch does not affect simulation | PASS | model/pc_demo/app/main.cpp | gpu2d_test_system | gpu2d_test_system |
| STR-01 | Sprite Storm | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_system, gpu2d_test_sim | gpu2d_test_system |
| STR-02 | Alpha Storm | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_system, gpu2d_test_sim | gpu2d_test_system |
| STR-03 | Bullet Hell | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_system, gpu2d_test_sim | gpu2d_test_system |
| STR-04 | Scale Storm | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_system, gpu2d_test_sim | gpu2d_test_system |
| STR-05 | Overdraw Storm | PASS | software/applications/neon_survivor/src/sim.cpp | gpu2d_test_system | gpu2d_test_system |
| STR-06 | Stress telemetry labeled PC Golden workload stats | PASS | results/stage0045/stress/stress_stats.md | gpu2d_test_system | gpu2d_test_system |
| SYS-01 | 100 frames Immediate==Tile exact | PASS | model/pc_demo/tests/test_system.cpp | gpu2d_test_system | gpu2d_test_system |
| SYS-02 | 1000-frame headless stability | PASS | model/pc_demo/tests/test_sim.cpp | gpu2d_test_sim | gpu2d_test_sim |
| SYS-03 | Immediate→Tile→Immediate switch stability | PASS | model/pc_demo/tests/test_system.cpp | gpu2d_test_system | gpu2d_test_system |
| SYS-04 | Deterministic frame capture/hash | PASS | model/pc_demo/tests/test_system.cpp, results/stage0045/captures/ | gpu2d_test_system | gpu2d_test_system |
| SYS-05 | Game code independent of Golden internals | PASS | scripts/check_app_boundary.py | gpu2d_test_boundary | gpu2d_test_boundary |
| SYS-06 | Presenter does not host-render game content | PASS | model/pc_demo/host/presenter.cpp | gpu2d_test_presenter | gpu2d_test_presenter |
| SYS-07 | Stage-004 regressions still pass | PASS | model/golden/ | ctest stage0045 includes golden_* | golden_test_tile_faults |
| AUD-01 | 56-ID acceptance manifest | PASS | docs/tasks/STAGE_0045_ACCEPTANCE.json | gpu2d_test_boundary | gpu2d_test_boundary |
| AUD-02 | Strict acceptance checker | PASS | scripts/check_stage0045_acceptance.py | gpu2d_test_boundary | gpu2d_test_boundary |
| AUD-03 | REPORT_0045 exact commits and 56-row matrix | PASS | docs/reports/REPORT_0045_PC_Golden_Interactive_Application.md | gpu2d_test_boundary | gpu2d_test_boundary |
| AUD-04 | Captures and stress outputs checked/generated | PASS | results/stage0045/captures/, results/stage0045/stress/stress_stats.md | gpu2d_test_system, gpu2d_demo_headless_tile | gpu2d_test_system |

## 16. Git Status

All Stage 004.5 sources, captures, acceptance, and this report are committed on `master`. No uncommitted required artifacts.
