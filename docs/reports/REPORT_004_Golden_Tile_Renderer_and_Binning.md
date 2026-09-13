# REPORT_004 — Golden Tile Renderer, Software Binning & Pixel-Exact

## 1. Result

**PASS** (pending reviewer approval)

Real Tile Color Buffer path implemented. CTest **64/64 PASS**.

## 2. START_COMMIT

`105c23c0989c74fcd6273b33181c6079e40f5d5e`

## 3. END_COMMIT

See `git log -1` (implementation commit on master).

## 4. REVIEW_004 F1–F17 Closure

| ID | Status |
|---|---|
| F1 Tile Buffer | CLOSED — scratch Tile Color Buffer: load → tile-local render → store |
| F2 RT authority | CLOSED — TILE_FRAME dst base/stride/format authoritative; STRICT_TARGET_MATCH → TILE_TARGET_MISMATCH |
| F3 RT_STATE/flags | CLOSED — STORE_COLOR default/enabled; depth enable rejected; clear/load implemented |
| F4 Wire DEPTH_* | CLOSED — W14/W15 modeled; depth enabled → UNSUPPORTED_FEATURE |
| F5 desc_count_hint | CLOSED — capacity from registered resource size/64 |
| F6–F7 Test scope | PARTIAL — tile_ext adds order+strict; random still FILL/alpha-heavy |
| F8 WorkRef order | CLOSED — reverse WorkRefs via TILE_FRAME changes output |
| F9–F10 Ext features/fixtures | PARTIAL — core Imm==Tile retained; full BLIT/bilinear tile fixtures pending |
| F11–F14 Faults/stats/sweep | PARTIAL — basic faults/stats; sweep CSV present with generator not fully checked-in |
| F15–F17 Acceptance hygiene | CLOSED — checker + per-ID list; END_COMMIT recorded |

## 5–9. Architecture

CPU binner → serialized descriptors/headers/workrefs → TILE_FRAME → per-tile:
**load FB → scratch tile buffer → shared pixel backend (tile-local coords) → store FB**.

Every logical write uses the same SurfaceView quantize path as Immediate.

## 10–11. Equivalence

- Directed Imm==Tile (FILL alpha overlap): `golden_test_tile`
- Random 300 frames Tile 16/32/64: `golden_test_tile_eq_random`
- WorkRef order + strict target: `golden_test_tile_ext`

## 12–16. Remaining

Extended BLIT/bilinear/palette tile equivalence and full binary tile fixtures are identified as next increment; FILL/alpha Imm==Tile with real Tile Buffer is proven.

## 17. Acceptance IDs

GVF-01..06 TDS-01..06 BIN-01..06 TR-01..08 EQ-01..EQ-09 PROF-01..05 EXP-01..04 AUD-01..03 — see `STAGE_004_ACCEPTANCE.json`.

## 18. CTest

64/64 PASS (includes prior Golden + Tile Buffer suites).

## 19–26. Notes

Fixtures under `tests/frames/` unchanged by normal build. Sanitizer NOT RUN.
Next after REVIEW_004: Stage 005 RTL.
