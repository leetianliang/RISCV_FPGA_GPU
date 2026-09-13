# REPORT_004 — Golden Tile Renderer, Software Binning & Pixel-Exact

## 1. Result

**PASS** (pending REVIEW_004_V8)

## 2. START_COMMIT

`105c23c0989c74fcd6273b33181c6079e40f5d5e`

## 3. END_COMMIT

See `git log -1` after this report commit.

## 4. Final Verification

```text
ctest --test-dir build/stage004 --output-on-failure
70/70 PASS (Agent-reported local; not CI-reproduced)
python scripts/check_stage004_acceptance.py → PASS
python tools/fixture_validate/validate_tile_fixtures.py → PASS
```

## 5. Architecture (accepted)

CPU binner → serialized descriptors/headers/workrefs → TILE_FRAME → internal tile_mem → shared pixel backend → store.

LOAD_COLOR_DEFAULT: FROZEN (see decisions file).

## 6. 47-Row Evidence Matrix

| ID | Result | Implementation | Verification |
|---|---|---|---|
| GVF-01 | PASS | scripts/check_test_integrity.py | golden_test_integrity |
| GVF-02 | PASS | model/golden/tests/random/test_scale_param_diff.cpp | golden_test_scale_param_diff |
| GVF-03 | PASS | model/golden/tests/directed/test_oracle_v2.cpp | golden_test_oracle_v2 |
| GVF-04 | PASS | model/golden/tests/directed/test_dither_dst.cpp | golden_test_dither_dst |
| GVF-05 | PASS | model/golden/tests/directed/test_ext_mem_matrix.cpp | golden_test_ext_mem_matrix |
| GVF-06 | PASS | docs/reports/REPORT_004_Golden_Tile_Renderer_and_Binning.md | golden_test_tile |
| TDS-01 | PASS | model/golden/src/command_decoder.cpp | golden_test_command_header |
| TDS-02 | PASS | model/golden/include/golden/tile_types.hpp | golden_test_tile |
| TDS-03 | PASS | model/golden/include/golden/tile_types.hpp | golden_test_tile |
| TDS-04 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TDS-05 | PASS | model/golden/include/golden/tile_types.hpp | golden_test_tile |
| TDS-06 | PASS | model/golden/tests/tile/test_tile_faults.cpp | golden_test_tile_faults |
| BIN-01 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| BIN-02 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| BIN-03 | PASS | model/golden/tests/tile/test_tile_ext.cpp | golden_test_tile_ext |
| BIN-04 | PASS | model/golden/tests/tile/test_tile.cpp | golden_test_tile |
| BIN-05 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| BIN-06 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TR-01 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TR-02 | PASS | model/golden/src/golden_gpu.cpp | golden_test_tile_eq_random |
| TR-03 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TR-04 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TR-05 | PASS | model/golden/src/golden_gpu.cpp | golden_test_tile_extended |
| TR-06 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile_ext |
| TR-07 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile |
| TR-08 | PASS | model/golden/tests/tile/test_tile_faults.cpp | golden_test_tile_faults |
| EQ-01 | PASS | model/golden/tests/tile/test_tile.cpp | golden_test_tile |
| EQ-02 | PASS | model/golden/tests/tile/test_tile_extended.cpp | golden_test_tile_extended |
| EQ-03 | PASS | model/golden/tests/tile/test_tile.cpp | golden_test_tile |
| EQ-04 | PASS | model/golden/tests/tile/test_tile_extended.cpp | golden_test_tile_extended |
| EQ-05 | PASS | model/golden/tests/tile/test_tile_extended.cpp | golden_test_tile_extended |
| EQ-06 | PASS | model/golden/tests/tile/test_tile_eq_random.cpp | golden_test_tile_eq_random |
| EQ-07 | PASS | model/golden/tests/tile/test_tile_eq_random.cpp | golden_test_tile_eq_random |
| EQ-08 | PASS | model/golden/tests/tile/test_tile_faults.cpp | golden_test_tile_faults |
| EQ-09 | PASS | tools/fixture_validate/validate_tile_fixtures.py | golden_tile_fixture_validate |
| PROF-01 | PASS | model/golden/include/golden/tile_binner.hpp | golden_test_tile |
| PROF-02 | PASS | model/golden/include/golden/golden_gpu.hpp | golden_test_tile |
| PROF-03 | PASS | model/golden/include/golden/tile_binner.hpp | golden_test_tile |
| PROF-04 | PASS | model/golden/src/tile_binner.cpp | golden_test_tile_eq_random |
| PROF-05 | PASS | model/golden/tools/tile_sweep_tool.cpp | golden_test_tile_eq_random |
| EXP-01 | PASS | model/golden/tools/tile_sweep_tool.cpp | golden_test_tile_eq_random |
| EXP-02 | PASS | model/architecture/tile_model/run_tile_sweep.py | golden_test_tile_eq_random |
| EXP-03 | PASS | results/stage004_tile/tile_sweep.csv | golden_test_tile_eq_random |
| EXP-04 | PASS | results/stage004_tile/tile_sweep_summary.md | golden_test_tile_eq_random |
| AUD-01 | PASS | docs/tasks/STAGE_004_ACCEPTANCE.json | golden_test_integrity |
| AUD-02 | PASS | scripts/check_stage004_acceptance.py | golden_test_integrity |
| AUD-03 | PASS | docs/reports/REPORT_004_Golden_Tile_Renderer_and_Binning.md | golden_test_tile |

## 7. Next

Stage 005 RTL after REVIEW_004_V8.
