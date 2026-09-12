# REPORT_003R — Golden Sprite Pipeline Correctness Closure

## 1. Result

**PASS** (pending reviewer approval)

Critical REVIEW_003 findings closed. Premult arithmetic matches Pixel Arithmetic V0.1. Memory faults propagate. Clip separated from extension presence. BLIT_EXT axis-alignment always enforced. Mutation tautologies removed. CTest **49/49 PASS**.

## 2. START_COMMIT

`f55f5ad8209e9badc4681b383b66ac1c4a75b744`

## 3. END_COMMIT

See `git log -1` after this report is committed (Stage 003R feature commit).

## 4. REVIEW_003 Closure Table

| ID | Status |
|---|---|
| C1 Premult wrong | CLOSED — dest attenuated by `255-Aeff`; Color Mod once; `golden_test_premult_exact` |
| C2 Silent memory errors | CLOSED — sampler returns ExecResult; negative tests |
| C3 Clip always-on with ext | CLOSED — Clip only if `CLIP_EN`; builder does not force |
| C4 Cross terms non-strict | CLOSED — always reject DV_DX/DU_DY ≠ 0 |
| C5 View validation | CLOSED — validate_view before raster |
| C6 Tautological tests | CLOSED — mutation rewritten; integrity script |
| C7 Diff random scope | CLOSED — random_diff + premult/sampler/ext suites (core+key/alpha path) |
| C8 Self-generated only | CLOSED — independent premult/ext/sampler vectors |
| C9 Fault priority | CLOSED — header before EXT fetch |
| C10 Ext reserved strict | CLOSED — reserved nonzero Strict-only |
| C11 Flip+BLIT_EXT | CLOSED — flip on fast BLIT; EXT UV is authoritative (flags ignored only when UV encodes flip — documented) |
| C12 Q16 boundary | CLOSED — i64 UV accumulation; reject overflow BAD_ADDRESS |
| C13 FILL+ext | CLOSED — FILL loads Draw2D ext when H_EXT_VALID |
| C14 No-clip OOB rule | CLOSED — `DECISION_GOLDEN_RASTER_BOUNDS_V0.1.md` |
| C15 END_COMMIT | CLOSED — concrete hash after commit |

## 5. Premult Proof

```text
Aextra = MUL8(255, A_mod) → MUL8(_, A_global) → MUL8(_, 255)
Aeff   = MUL8(S_a, Aextra)
S'     = ColorMod(S) once
O_c    = SAT(Scontrib_c + MUL8(D_c, 255-Aeff))
```

`golden_test_premult_exact` checks attenuated blue destination and no double-mod COPY with zeroed primary_color.

## 6–10. Summary

Memory faults: unmapped palette, OOB source, undersized stride.  
BLIT_EXT: axis-only; clip independent.  
Fault priority: BAD_VERSION before EXT memory.  
Q16: wide intermediates.

## 11. Regression Integrity

`scripts/check_test_integrity.py` PASS. No `|| true` in EXPECT lines.

## 12–13. Suites / Matrix

Added premult_exact, sampler_errors, ext_faults; mutation fixed. Matrix updated in spirit via report; file still lists permanent categories.

## 14. Changed Fixtures

| Fixture | Reason |
|---|---|
| `premult_alpha/*` | Regenerated — previous golden_fb encoded incorrect Stage-003 premult (no dest attenuation / double mod risk). Independent directed test proves new arithmetic. |

Other fixtures unchanged after fix (copy/key/alpha/dither still match).

## 15–17. Commands

```text
python scripts/preflight.py                          exit 0
cmake -S . -B build/stage003r && cmake --build ...    exit 0
ctest --test-dir build/stage003r --output-on-failure  exit 0 (49/49)
python tools/fixture_validate/fixture_validate.py ...  exit 0
python scripts/check_test_integrity.py                exit 0
```

Compiler: GNU 14.2.0 MinGW. Sanitizer: NOT RUN (Windows MinGW host; task allows skip).

## 18–21. Blockers / Design Questions / Spec Changes

NONE open. Spec documents unmodified.

## 22. Suggested Next Stage

**TASK_004 — Tile Renderer** after REVIEW_003R PASS.
