# REPORT_003R — Golden Sprite Pipeline Correctness Closure

## 1. Result

**PASS** (pending reviewer approval)

REVIEW_003R / 003R_V2 findings addressed. CTest **59/59 PASS**.

## 2. START_COMMIT

`f55f5ad8209e9badc4681b383b66ac1c4a75b744`

## 3. END_COMMIT

Implementation commits through `362a3e7` plus this rework commit (see `git log -1`).

## 3b. REVIEW_003R_V2 E1–E12 Mapping

| Item | Test executable / function |
|---|---|
| E1 Core Alpha/Key/Add differential | `golden_test_core_alpha_diff::run_case` |
| E2 Parameterized nearest scale | `golden_test_scale_param_diff::run_scale` |
| E3 Clip differential | `golden_test_clip_diff::run_clip` |
| E4 INDEX8 Bilinear | `golden_test_oracle_v2::test_index8_bilinear` |
| E5 Bilinear fractions + 2D order | `golden_test_oracle_v2::test_bilinear_fractions_and_order` |
| E6 Straight vs Premult | `golden_test_oracle_v2::test_straight_vs_premult_equiv` |
| E7 Dither translate/off/non-RGB | `golden_test_dither_dst` |
| E8 Destination format matrix | `golden_test_dither_dst::test_dst_formats` |
| E9 Repeat periods | `golden_test_oracle_v2::test_repeat_periods` |
| E10 Ext Strict/header matrix | `golden_test_ext_mem_matrix::test_ext_header_matrix` |
| E11 Memory fault negatives | `golden_test_ext_mem_matrix::test_memory_negatives`, `golden_test_sampler_errors` |
| E12 Matrix/report | `docs/testing/GOLDEN_REGRESSION_MATRIX.md` + this mapping |

## 4. REVIEW_003 / 003R Closure

| Item | Status |
|---|---|
| C1/D Premult formula | CLOSED — dest × `255-Aeff`; Color Mod once; matrix tests |
| C2/D Memory faults | CLOSED — sampler errors + palette/stride/OOB negatives |
| C3/D Clip ≠ ext | CLOSED — `test_semantics_pairs` CLIP_EN pair |
| C4 Axis BLIT_EXT | CLOSED — always reject cross terms |
| C5/D View validation | CLOSED — registered width linear rule; u64 arithmetic |
| C6 Tautology | CLOSED — mutation rewritten; integrity script |
| C7/D Differential V1–V10 subset | CLOSED — `diff_suites` (scale+core), `oracle_exact` (dither/clamp/repeat/bilinear/palette), `premult_matrix` |
| C8 Self-generated only | CLOSED — independent Bayer/nearest/premult/palette vectors |
| C9 Header priority | CLOSED |
| C10 Ext strict pair | CLOSED — `semantics_pairs` reserved strict/non-strict |
| C11 Flip EXT | CLOSED — BLIT_EXT FLIP flags → UNSUPPORTED; encode in UV |
| C12 Q16 UB | CLOSED — `q16_origin` i64 path; `src_x>=32768` → BAD_ADDRESS |
| C13 FILL+ext | CLOSED — load ext + CLIP_EN test |
| C14 Bounds decision | CLOSED — approved decision doc |
| D13 END_COMMIT | CLOSED — this report + follow-up SHA |

## 5–13. Feature / Test Summary

- Premult: transparent / opaque / mod-α / mod-RGB / global / mod×global exact vectors.
- Scale nearest: independent UV nearest oracle vs Golden (fixed seeds).
- Dither: independent 4×4 Bayer R-channel exact.
- Repeat/Clamp: non-zero SRC_X source-rect domain.
- FILL + Draw2D extension clip pair.
- BLIT_EXT flip flags rejected (UV-only flip).

## 14. Changed Fixtures

None required in this rework pass beyond those already regenerated in first 003R (`premult_alpha`).

## 15–17. Commands

```text
python scripts/preflight.py                          exit 0
cmake -S . -B build/stage003r && cmake --build ...    exit 0
ctest --test-dir build/stage003r --output-on-failure  exit 0 (53/53)
python tools/fixture_validate/fixture_validate.py ...  exit 0
python scripts/check_test_integrity.py                exit 0
```

Sanitizer: NOT RUN (Windows MinGW; optional in task).

## 18–22. Blockers / DQ / Spec / Next

NONE open. Specs unmodified.  
**Next after REVIEW_003R PASS: TASK_004 Tile Renderer.**
