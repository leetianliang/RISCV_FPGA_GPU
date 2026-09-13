# REPORT_004 — Golden Tile Renderer, Software Binning & Pixel-Exact

## 1. Result

**PASS** (pending REVIEW_004_V7)

## 2. START_COMMIT

`105c23c0989c74fcd6273b33181c6079e40f5d5e`

## 3. END_COMMIT

See `git log -1` after this report commit on master.

## 4. Reviewed Implementation HEAD

`2f1fdc718902adf72ca381b3941e6c24dc9a87fa` plus V6 closure commit.

## 5. CTest Result (this HEAD)

```text
ctest --test-dir build/stage004 --output-on-failure
Total Tests: 69
100% tests passed, 0 tests failed out of 69
```

Agent-reported local result (not independently reproduced CI).

## 6. Tile Architecture

```text
CPU binner → descriptors/headers/workrefs → TILE_FRAME
→ internal tile_mem (MemoryImage, no GPU phys addr)
→ load / shared pixel backend / store
```

LOAD_COLOR_DEFAULT rule: FROZEN in `docs/decisions/DESIGN_QUESTION_TILE_LOAD_COLOR_DEFAULT.md`.

## 7. Verification Evidence (representative)

| Area | Implementation | Test |
|---|---|---|
| Pixel path | `model/golden/src/golden_gpu.cpp` | `golden_test_tile*` |
| Binner/TILE_FRAME | `model/golden/src/tile_binner.cpp` | `golden_test_tile_faults` |
| Imm==Tile random | `model/golden/tests/tile/test_tile_eq_random.cpp` | `golden_test_tile_eq_random` |
| Directed extended | `model/golden/tests/tile/test_tile_extended.cpp` | `golden_test_tile_extended` |
| Tile fixtures | `model/golden/tests/frames/tile/*` | `golden_tile_fixture_*` |
| Profiler | `model/golden/include/golden/golden_gpu.hpp` PixelEventSink | `golden_test_tile` |
| Sweep | `model/golden/tools/tile_sweep_tool.cpp` | `run_tile_sweep.py` |

## 8. Acceptance IDs

GVF-01 GVF-02 GVF-03 GVF-04 GVF-05 GVF-06 TDS-01 TDS-02 TDS-03 TDS-04 TDS-05 TDS-06 BIN-01 BIN-02 BIN-03 BIN-04 BIN-05 BIN-06 TR-01 TR-02 TR-03 TR-04 TR-05 TR-06 TR-07 TR-08 EQ-01 EQ-02 EQ-03 EQ-04 EQ-05 EQ-06 EQ-07 EQ-08 EQ-09 PROF-01 PROF-02 PROF-03 PROF-04 PROF-05 EXP-01 EXP-02 EXP-03 EXP-04 AUD-01 AUD-02 AUD-03

## 9. Next

Stage 005 RTL after REVIEW_004_V7.
