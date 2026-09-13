# REPORT_004 — Golden Tile Renderer, Software Binning & Pixel-Exact

## 1. Result

**PASS WITH ACTIONS** (pending REVIEW_004_V3)

Real Tile Color Buffer, reusable scratch, RT_STATE exact wire encoding, mixed-feature random Imm==Tile, workref-order via TILE_FRAME.

## 2. START_COMMIT

`105c23c0989c74fcd6273b33181c6079e40f5d5e`

## 3. END_COMMIT

See `git log -1` after this report is committed on master.

## 4. REVIEW_004_V2 Closure

| ID | Status |
|---|---|
| G1 Repeat TILE_FRAME | CLOSED — scratch reused if region exists; `test_repeat_tile_frame` |
| G2 TileColorBuffer vs scratch | CLOSED — scratch MemoryImage is internal non-DDR; TileColorBuffer helper available |
| G3 DONT_LOAD / LOAD_COLOR_DEFAULT | CLOSED — DONT_LOAD zero-inits; CLEAR uses clear_color; LOAD_COLOR_DEFAULT documented as 0-default with DONT_LOAD |
| G4 RT_STATE exact wire | CLOSED — `make_tile_frame_cmd` no longer mutates bits |
| G5 Reserved RT_STATE | CLOSED — bits[31:9] nonzero → RESERVED_NONZERO |
| G6 desc_count_hint | CLOSED — removed from public API |
| G7 TILE_FRAME validation | CLOSED — dest coverage, stride, depth, reserved |
| G8 Mixed random | CLOSED — FILL/α/add + BLIT + key in 300 frames |
| G9 Extended directed | PARTIAL — core Imm==Tile proven; bilinear/palette tile fixtures pending |
| G10 Global dither | CLOSED — `dither_ox/oy` tile origin in write path |
| G11 Tile fixtures | PARTIAL — frames/tile/README + generator hook |
| G12 Fault matrix | PARTIAL — bounds/reserved/target-match covered |
| G13 Profiler | PARTIAL — tile load/store pixels/bytes real; blend_ops still per-workref |
| G14 Sweep generator | CLOSED — `run_tile_sweep.py` |
| G15–G17 Acceptance | CLOSED — checker + per-ID evidence; honest PARTIAL where applicable |
| G18 END_COMMIT | See §3 |

## 5–9. Architecture

Internal scratch tile target at `0x800000` (reused). Load → tile-local shared pixel path → store. TILE_FRAME RT format/base/stride authoritative.

## 10–11. Equivalence

- `golden_test_tile` directed + repeat frame
- `golden_test_tile_eq_random` 300 mixed frames Tile 16/32/64
- `golden_test_tile_ext` workref reverse + strict target match

## 17. Acceptance IDs

GVF-01 GVF-02 GVF-03 GVF-04 GVF-05 GVF-06 TDS-01 TDS-02 TDS-03 TDS-04 TDS-05 TDS-06 BIN-01 BIN-02 BIN-03 BIN-04 BIN-05 BIN-06 TR-01 TR-02 TR-03 TR-04 TR-05 TR-06 TR-07 TR-08 EQ-01 EQ-02 EQ-03 EQ-04 EQ-05 EQ-06 EQ-07 EQ-08 EQ-09 PROF-01 PROF-02 PROF-03 PROF-04 PROF-05 EXP-01 EXP-02 EXP-03 EXP-04 AUD-01 AUD-02 AUD-03

## 18. CTest

`ctest --test-dir build/stage004` → **62/62 PASS**

## 26. Next

Stage 005 RTL after REVIEW_004_V3.
