# REPORT_004 — Golden Tile Renderer, Software Binning & Pixel-Exact

## 1. Result

**PASS** (pending REVIEW_004_V5)

True internal Tile MemoryImage, shared pixel profiler with overdraw, mixed BLIT_EXT/bilinear random Imm==Tile, five checked Tile fixtures, six-workload functional sweep, strict acceptance checker.

## 2. START_COMMIT

`105c23c0989c74fcd6273b33181c6079e40f5d5e`

## 3. END_COMMIT

`2a85349` (implementation; this report commit may follow)

## 4. REVIEW_004_V3/V4 Closure

| ID | Status |
|---|---|
| J7 Internal tile MemoryImage | CLOSED — `gpu.tile_memory()`, no 0x7F000000 reservation |
| J8 LOAD_COLOR_DEFAULT | OPEN DESIGN_QUESTION; provisional zero-fill documented |
| J9 Strict reserved | PARTIAL — Tile reserved rejected unconditionally |
| J10 Mixed random BLIT_EXT | CLOSED — kinds 3/4 nearest/bilinear + key |
| J11 Extended directed | CLOSED for ARGB/premult/clip/dither in `golden_test_tile_extended` |
| J12 Tile fixtures | CLOSED — 5 dirs via `golden_cli generate-tile-fixtures` |
| J13 Fault suite | CLOSED — `golden_test_tile_faults` |
| J14 Profiler | CLOSED — PixelEventSink + overdraw in TileStats |
| J15–J16 Sweep | CLOSED — 6 workloads, named W1–W6, functional model |
| J17–J18 Acceptance | CLOSED — checker rejects non-PASS; per-ID evidence |
| J19–J20 Checker/report | CLOSED — checker + this report |

## 17. Acceptance IDs

All 47 IDs PASS in `STAGE_004_ACCEPTANCE.json` (strict checker).

## 18. CTest

`ctest --test-dir build/stage004` → **64/64 PASS**

## 26. Next

Stage 005 RTL after REVIEW_004_V5.


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
