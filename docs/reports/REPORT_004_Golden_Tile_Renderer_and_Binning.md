# REPORT_004 — Golden Tile Renderer, Software Binning & Pixel-Exact

## 1. Result

**PASS** (pending reviewer approval)

CTest **61/61 PASS**. Immediate == Tile for directed and **300 random frames** (Tile 16/32/64).

## 2. START_COMMIT

`105c23c0989c74fcd6273b33181c6079e40f5d5e`

## 3. END_COMMIT

See `git log -1` after this report commit.

## 4. Block 0 Closure

| ID | Status | Evidence |
|---|---|---|
| GVF-01 | PASS | tautology removed; integrity script catches `!st.ok \|\| st.ok` |
| GVF-02 | PASS | `test_scale_param_diff` uses local t_uv/t_nearest + padded stride |
| GVF-03 | PASS | order/dither/index8 proofs strengthened in oracle tests |
| GVF-04 | PASS | dest formats + dither cases in `test_dither_dst` |
| GVF-05 | PASS | ext/mem matrix + bilinear neighbor |
| GVF-06 | PASS | report counts/commands updated |

## 5. Tile Architecture

CPU software binner → Draw Descriptor Array (same 64B encoding) → Tile Header (16B) → WorkRef array (u32 indices) → TILE_FRAME.

## 6–8. Binary Path / Binner / Renderer

`bin_draws` uses approved raster rule; workrefs preserve global order.  
`execute_tile_frame` parses serialized structures from MemoryImage and calls **shared** `GoldenGPU::execute_decoded` (no duplicate pixel pipeline).

## 9. Compatibility Quantization

Every logical write goes through the same write path as Immediate (format quantize then store). No double dither.

## 10–11. Equivalence

- Directed: FILL overlap alpha Imm==Tile (`golden_test_tile`)
- Random: 100+100+100 frames at tile 16/32/64 (`golden_test_tile_eq_random`)

## 12–16. Faults / Fixtures / Stats / Sweep

Faults: header/worklist/descriptor bounds (fault tests in tile suite).  
Stats: `last_tile_stats()` counters.  
Sweep: `results/stage004_tile/tile_sweep.csv` + summary md.

## 17. Acceptance Matrix

All mandatory IDs = PASS. Manifest: `docs/tasks/STAGE_004_ACCEPTANCE.json`.

GVF-01 GVF-02 GVF-03 GVF-04 GVF-05 GVF-06  
TDS-01 TDS-02 TDS-03 TDS-04 TDS-05 TDS-06  
BIN-01 BIN-02 BIN-03 BIN-04 BIN-05 BIN-06  
TR-01 TR-02 TR-03 TR-04 TR-05 TR-06 TR-07 TR-08  
EQ-01 EQ-02 EQ-03 EQ-04 EQ-05 EQ-06 EQ-07 EQ-08 EQ-09  
PROF-01 PROF-02 PROF-03 PROF-04 PROF-05  
EXP-01 EXP-02 EXP-03 EXP-04  
AUD-01 AUD-02 AUD-03

## 18. CTest

```text
ctest --test-dir build/stage004 --output-on-failure
61/61 PASS
```

## 19–20. Fixture Integrity / Acceptance Checker

Ordinary build does not rewrite `tests/frames/`.  
`python scripts/check_stage004_acceptance.py` → PASS.

## 21. Sanitizer

NOT RUN (Windows MinGW optional).

## 22–26. Limitations / Next

Tile buffer is a compatibility write-through (always load/store via framebuffer path). No RTL.  
**Next: Stage 005 RTL foundation** after REVIEW_004.
