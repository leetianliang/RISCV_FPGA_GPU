# REPORT_003 — Golden Feature-Complete Sprite Pipeline

## 1. Result

**PASS** (pending reviewer approval)

REVIEW_002 B1–B11 closed. Immediate-mode sprite path extended with BLIT_EXT, scaling, samplers, clip/flip, palette, color-mod, premult, dither, multi-format RTs. CTest **45/45 PASS**. Fixture validation PASS. Ordinary build/test leaves fill_basic hash unchanged.

## 2. START_COMMIT / END_COMMIT

- START_COMMIT: `d11c80110e39b7409f8aad65c00dc30572cb123a`
- END_COMMIT: Stage-003 feature commit on master (see git log)

## 3. REVIEW_002 Correction Closure

| ID | Status |
|---|---|
| B1 Command stride/format authoritative | CLOSED — RegisteredResource + SurfaceView from command fields |
| B2 SRC_X/Y uint16 | CLOSED — decode/build unsigned; parser test 0x8000→32768 |
| B3 fill_basic manifest base | CLOSED — base 65536; fixture_validate.py |
| B4 Stage-001 coverage | CLOSED — FILL-001..010 restored in test_fill_directed |
| B5 Random checks pixels | CLOSED — test_random_diff independent oracle |
| B6 Reserved vs unsupported | CLOSED — classify_* + BAD_FORMAT/BLEND/FILTER |
| B7 XRGB write 0xFF | CLOSED — Surface + SurfaceView + unit test |
| B8 round_div overflow-safe | CLOSED — q/r method; INT64_MIN tests |
| B9 Stride/alpha acceptance tests | CLOSED — non-tight stride, mutation, alpha 0/1/127/128/254/255 |
| B10/B11 Tooling | CLOSED — CLI rejects unknown format; unused helpers cleaned |

## 4. Architecture

```text
Command bytes → LE deserialize → classify → Draw2DState
→ RegisteredResource bounds + command stride/format = SurfaceView
→ sample (nearest/bilinear, clamp/repeat, palette)
→ key → color mod → effective alpha → blend → dither/RT encode → write
```

## 5. Feature Matrix

FILL/BLIT/BLIT_EXT; RGB565/ARGB/XRGB sources; INDEX8+palette; COPY/straight/premult/add; key; color mod; global/pixel alpha; nearest/bilinear; scale; flip; clip; clamp/repeat; RGB565 dither; RGB565/ARGB/XRGB targets.

## 6–13. Feature Notes

- BLIT_EXT uses DRAW2D_EXT_V1 64B LE descriptor (clip + Q16.16 UV).
- Scale UV: frozen pixel-center coefficients via `round_div_signed`.
- Clip does not reset UV origin (UV from full dest rect).
- Indexed8 bilinear: palette decode then RGBA lerp.
- Premult: no double source-alpha; extra opacity staged.
- Dither: 4×4 Bayer on global RT coords.

## 14. Regression Mapping

| Stage-001/002 | Stage-003 |
|---|---|
| FILL-001..010 | restored in `golden_test_fill_directed` |
| arithmetic exhaustive | `golden_test_gpu_math` |
| command header | `golden_test_command_header` |
| BLIT copy/key/alpha/add | `golden_test_blit_directed` + stage002 fixtures |
| random success-only | replaced by pixel-compare `golden_test_random_diff` |

## 15–16. Results

Unit/directed/mutation/sprite_ext/random_diff: PASS.  
Fixtures (16): run+exact compare PASS.

## 17–18. Fixture Validation & Integrity

`fixture_validate.py` 16/16 PASS.  
`fill_basic/golden_fb.raw` SHA256 unchanged across rebuild+ctest.

## 19. BLIT vs BLIT_EXT

`golden_test_sprite_ext::test_blit_ext_eq_blit` PASS.

## 20. Commands

```text
python scripts/preflight.py                          exit 0
cmake -S . -B build/stage003 && cmake --build ...    exit 0
ctest --test-dir build/stage003 --output-on-failure  exit 0 (45/45)
python tools/fixture_validate/fixture_validate.py model/golden/tests/frames  exit 0
```

Compiler: GNU 14.2.0 / CMake 3.31.5 / Python 3.10.0

## 21. Cross-Compiler / Sanitizer

Optional sanitizer config not enabled on this Windows MinGW host (task allows skip).

## 22. Limitations

- Tile/Affine/RTL not started.
- BLIT_EXT axis-aligned preferred; non-axis UV accepted when non-strict.
- Overlapping BLIT source/dest not defined by specs — not tested as memmove.

## 23–25. Blockers / Design Questions / Deviations

NONE open.  
Deviations: CMake fixture args driven by `texture_meta.txt` rather than full JSON runner; `run-fixture` manifest orchestrator deferred (Python validate present).

## 26. Suggested Next Stage

**Stage 004 — Tile Renderer, WorkList/Binning & Immediate-vs-Tile Pixel-Exact** (pending REVIEW_003).
