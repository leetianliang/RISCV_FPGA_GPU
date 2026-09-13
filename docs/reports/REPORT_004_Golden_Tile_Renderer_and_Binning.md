# REPORT_004 — Golden Tile Renderer, Software Binning & Pixel-Exact

## 1. Result

**PASS** (pending REVIEW_004_V9)

## 2. START_COMMIT

`105c23c0989c74fcd6273b33181c6079e40f5d5e`

## 3. END_COMMIT

`PENDING — set after implementation commit via --end-commit`

## 4. Final Verification

```text
ctest --test-dir build/stage004 --output-on-failure
71/71 PASS (Agent-reported local; not CI-reproduced)
python scripts/check_stage004_acceptance.py → PASS
python tools/fixture_validate/validate_tile_fixtures.py → PASS
python tools/fixture_validate/fixture_validate.py → PASS
python scripts/check_test_integrity.py → PASS
python model/architecture/tile_model/run_tile_sweep.py → 18-row W1–W6 × 16/32/64
```

## 5. Architecture (accepted)

CPU binner → serialized descriptors/headers/workrefs → TILE_FRAME → internal tile_mem → shared pixel backend → store.

LOAD_COLOR_DEFAULT: FROZEN (see decisions file).

Alignment (Command ISA V0.1 + local interpretation before RTL):

- Draw Descriptor Array Base: **64B** (ISA frozen).
- Tile Header Array Base: **16B** (structure size; ISA does not freeze 64B).
- Work List Base: **4B** (WorkRef entry size; ISA does not freeze 64B).

Misaligned required pointer returns `BAD_ALIGNMENT` before array access.

## 6. 47-Row Evidence Matrix

| ID | Requirement | Result | Implementation Evidence | Verification Evidence |
|---|---|---|---|---|
| GVF-01 | Test integrity guard | PASS | scripts/check_test_integrity.py | golden_test_integrity |
| GVF-02 | Independent scale oracle | PASS | model/golden/tests/random/test_scale_param_diff.cpp | golden_test_scale_param_diff |
| GVF-03 | Proof assertions | PASS | model/golden/tests/directed/test_oracle_v2.cpp | golden_test_oracle_v2 |
| GVF-04 | Dest format/dither matrix | PASS | model/golden/tests/directed/test_dither_dst.cpp | golden_test_dither_dst |
| GVF-05 | Ext/mem negatives | PASS | model/golden/tests/directed/test_ext_mem_matrix.cpp | golden_test_ext_mem_matrix |
| GVF-06 | Report hygiene | PASS | docs/reports/REPORT_004_Golden_Tile_Renderer_and_Binning.md | golden_test_tile |
| TDS-01 | TILE_FRAME opcode | PASS | model/golden/src/command_decoder.cpp | golden_test_command_header |
| TDS-02 | Tile Header serialize | PASS | model/golden/include/golden/tile_types.hpp | golden_test_tile |
| TDS-03 | WorkRef | PASS | model/golden/include/golden/tile_types.hpp | golden_test_tile |
| TDS-04 | Descriptor 64B encoding | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TDS-05 | Tile grid | PASS | model/golden/include/golden/tile_types.hpp | golden_test_tile |
| TDS-06 | Tile fault matrix | PASS | model/golden/src/tile_binner.cpp, model/golden/tests/tile/test_tile_faults.cpp | golden_test_tile_faults |
| BIN-01 | One desc per draw | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| BIN-02 | Raster bounds | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| BIN-03 | WorkRef order | PASS | model/golden/tests/tile/test_tile_ext.cpp | golden_test_tile_ext |
| BIN-04 | Multi-tile coverage | PASS | model/golden/tests/tile/test_tile.cpp | golden_test_tile |
| BIN-05 | Deterministic output | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| BIN-06 | Binary path | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TR-01 | TILE_FRAME path | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TR-02 | Shared pixel backend | PASS | model/golden/src/golden_gpu.cpp | golden_test_tile_eq_random |
| TR-03 | Tile load | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TR-04 | Compat quantize | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TR-05 | Global coords | PASS | model/golden/src/golden_gpu.cpp | golden_test_tile_extended |
| TR-06 | Draw order | PASS | model/golden/src/tile_binner.cpp | golden_test_tile_ext |
| TR-07 | Edge tiles | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TR-08 | Descriptor/ext/palette memory | PASS | model/golden/src/tile_binner.cpp, model/golden/tests/tile/test_tile_faults.cpp | golden_test_tile_faults |
| EQ-01 | FILL Imm==Tile | PASS | model/golden/tests/tile/test_tile.cpp | golden_test_tile |
| EQ-02 | BLIT Imm==Tile | PASS | model/golden/tests/tile/test_tile_extended.cpp | golden_test_tile_extended |
| EQ-03 | Order-sensitive Imm==Tile | PASS | model/golden/tests/tile/test_tile.cpp | golden_test_tile |
| EQ-04 | Extended feature Imm==Tile | PASS | model/golden/tests/tile/test_tile_extended.cpp, model/golden/tests/frames/tile/tile_bilinear_cross_boundary, model/golden/tests/frames/tile/tile_dither_cross_boundary | golden_test_tile_extended, golden_tile_fixture_tile_bilinear_cross_boundary, golden_tile_fixture_tile_dither_cross_boundary |
| EQ-05 | Dest format Imm==Tile | PASS | model/golden/tests/tile/test_tile_extended.cpp | golden_test_tile_extended |
| EQ-06 | Tile sizes | PASS | model/golden/tests/tile/test_tile_eq_random.cpp | golden_test_tile_eq_random |
| EQ-07 | Mixed random Imm==Tile | PASS | model/golden/tests/tile/test_tile_eq_random.cpp | golden_test_tile_eq_random |
| EQ-08 | Pathological workloads | PASS | model/golden/tests/tile/test_tile_faults.cpp | golden_test_tile_faults |
| EQ-09 | Tile binary fixtures | PASS | tools/fixture_validate/validate_tile_fixtures.py | golden_tile_fixture_validate |
| PROF-01 | Tile counters | PASS | model/golden/include/golden/tile_binner.hpp | golden_test_tile |
| PROF-02 | Pixel workload counters | PASS | model/golden/include/golden/golden_gpu.hpp | golden_test_tile |
| PROF-03 | Overdraw | PASS | model/golden/include/golden/tile_binner.hpp | golden_test_tile |
| PROF-04 | Traffic separation | PASS | model/golden/src/tile_binner.cpp | golden_test_tile_sweep |
| PROF-05 | No fake timing | PASS | model/golden/tools/tile_sweep_tool.cpp | golden_test_tile_sweep |
| EXP-01 | Fixed workloads | PASS | model/golden/tools/tile_sweep_tool.cpp | golden_test_tile_sweep |
| EXP-02 | Sweep sizes | PASS | model/architecture/tile_model/run_tile_sweep.py | golden_test_tile_sweep |
| EXP-03 | CSV results | PASS | results/stage004_tile/tile_sweep.csv | golden_test_tile_sweep |
| EXP-04 | Summary | PASS | results/stage004_tile/tile_sweep_summary.md | golden_test_tile_sweep |
| AUD-01 | Acceptance manifest | PASS | docs/tasks/STAGE_004_ACCEPTANCE.json | golden_test_integrity |
| AUD-02 | Acceptance checker | PASS | scripts/check_stage004_acceptance.py | golden_test_integrity |
| AUD-03 | Report matrix | PASS | docs/reports/REPORT_004_Golden_Tile_Renderer_and_Binning.md | golden_test_tile |

## 7. Next

Stage 005 RTL after REVIEW_004_V9 PASS.
