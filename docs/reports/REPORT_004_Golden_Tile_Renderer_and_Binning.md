# REPORT_004 — Golden Tile Renderer, Software Binning & Pixel-Exact

## 1. Result

**PASS WITH ACTIONS** (pending REVIEW_004_V4)

Core Tile path, grid validation, TILE_FLAGS matrix, internal scratch resize, functional sweep, honest acceptance.

## 2. START_COMMIT

`105c23c0989c74fcd6273b33181c6079e40f5d5e`

## 3. END_COMMIT

See `git log -1` on master after this report commit.

## 4. REVIEW_004_V3 Closure

| ID | Status |
|---|---|
| H6 Scratch collision | CLOSED — internal name `tile_scratch_internal` at `0x7F000000`; refuse user-owned same address |
| H7 Resize/reconfig | CLOSED — forget+re-register; `test_reconfig_tile_size` |
| H8 LOAD_COLOR_DEFAULT | CLOSED — DONT_LOAD requires default (zero) else UNSUPPORTED |
| H9 TILE_FLAGS depth/reserved | CLOSED — depth → UNSUPPORTED; [31:4] → RESERVED_NONZERO |
| H11 Grid == ceil(surface/tile) | CLOSED — BAD_TILE_CONFIG + tests |
| H12 Mixed random | PARTIAL — FILL/α/Add/BLIT/Key; BLIT_EXT/Palette random still thin |
| H13 Extended directed | PARTIAL — core Imm==Tile proven |
| H14 Tile fixtures | PARTIAL — generator hook documented; checked dirs not all filled |
| H15 Fault suite | CLOSED — `golden_test_tile_faults` (grid/depth/dont_load/reconfig) |
| H16 Profiler | PARTIAL — tile load/store pixels from real events |
| H17 Sweep | CLOSED — `golden_tile_sweep` + `run_tile_sweep.py` drives functional model |
| H18–H20 Acceptance | CLOSED — checker + honest PARTIAL in this report |

## 5–11. Architecture / Equivalence / Tests

Internal scratch, shared pixel backend, TILE_FRAME authority. CTest includes tile + faults + mixed random.

## 17. Acceptance IDs

GVF-01 GVF-02 GVF-03 GVF-04 GVF-05 GVF-06 TDS-01 TDS-02 TDS-03 TDS-04 TDS-05 TDS-06 BIN-01 BIN-02 BIN-03 BIN-04 BIN-05 BIN-06 TR-01 TR-02 TR-03 TR-04 TR-05 TR-06 TR-07 TR-08 EQ-01 EQ-02 EQ-03 EQ-04 EQ-05 EQ-06 EQ-07 EQ-08 EQ-09 PROF-01 PROF-02 PROF-03 PROF-04 PROF-05 EXP-01 EXP-02 EXP-03 EXP-04 AUD-01 AUD-02 AUD-03

## 18. CTest

`ctest --test-dir build/stage004` → **66/66 PASS**

## 26. Next

Stage 005 RTL after REVIEW_004_V4 (once H12–H14/H16 PARTIAL items are fully closed).
