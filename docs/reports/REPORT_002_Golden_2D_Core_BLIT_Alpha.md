# REPORT_002 — Golden 2D Core: BLIT, Texture Formats, Color Key & Alpha

## 1. Result

**PASS** (pending reviewer approval)

REVIEW_001 Correction Block 0 closed. Stage-002 features implemented. CTest **22/22 PASS**. No third-party Golden/test dependency. Frozen specs unmodified.

## 2. START_COMMIT / END_COMMIT

- START_COMMIT: `ed4a593d378f8d16046f8463ef04de68fed1ddbf` (REVIEW_001 reviewed HEAD)
- END_COMMIT: see Stage-002 feature commit on master

## 3. REVIEW_001 A1–A11 Closure

| ID | Action | Status |
|---|---|---|
| A1 | Immutable fixtures on normal build | CLOSED — `golden_regenerate_fixtures` opt-in only |
| A2 | Explicit LE command I/O | CLOSED — `serialize_cmd_le` / `deserialize_cmd_le` + tests |
| A3 | FaultCode sync | CLOSED — full V0.1 table incl. BAD_RING_CONFIG/MEMORY_ERROR/BAD_ADDRESS |
| A4 | H_STRICT semantics | CLOSED — reserved/EXT_PTR strict-only checks + paired tests |
| A5 | Opcode classification | CLOSED — FILL/BLIT execute; BLIT_EXT/TILE_FRAME unsupported; 0x11 BAD_OPCODE |
| A6 | Region end > 2^32 | CLOSED — reject + boundary tests |
| A7 | UNMAPPED vs OUT_OF_RANGE | CLOSED |
| A8 | round_div_signed INT64_MIN | CLOSED — overflow-safe |
| A9 | fill_basic manifest base | CLOSED — base 65536 |
| A10 | Pillow hidden dep | CLOSED — `scripts/preview_fill_basic.py` removed |
| A11 | Golden README stale | CLOSED — rewritten for Stage 002 |

## 4. Architecture Implemented

```text
64B LE bytes → deserialize → header classifier → common 2D payload
→ normalized Draw2DState → FILL/BLIT front-end → shared pixel pipeline
→ RGB565 RT write
```

Pipeline order: source decode → Color Key → staged Effective Alpha → dest read (if blend) → blend → dest quantize → write.

## 5. Supported Feature Matrix (exit)

| Source | COPY | Key | Global α | Pixel α | Straight | Additive |
|---|---:|---:|---:|---:|---:|---:|
| RGB565 | yes | yes | yes | A=255 | yes | yes |
| ARGB8888 | yes | yes | yes | yes | yes | yes |
| XRGB8888 | yes | yes | yes | A=255 | yes | yes |

Destination RGB565 only. FILL uses shared path (COPY + Straight/Additive).

## 6. Files Added / Modified

Added/updated under `model/golden/` (types, ISA LE helpers, math, memory, surface, decoder, GPU, CLI, tests, fixtures), `docs/reviews/REVIEW_001_*.md`, `docs/tasks/TASK_002_*.md`, this report, README/PROJECT_STATUS. Specs under `docs/RISC-V_FPGA_2D_GPU_*.md`: **unmodified**.

## 7. Verification

| Item | Result |
|---|---|
| Unit (math/memory/surface/cmd_io/pixel) | PASS |
| Command header / strict / classification | PASS |
| FILL directed | PASS |
| BLIT directed (copy/key/alpha/add/stream) | PASS |
| Fixed-seed random (7×20 cases) | PASS |
| fill_basic immutable fixture | PASS |
| 5 BLIT fixtures + exact compare | PASS |
| intentional mismatch | PASS (non-zero exit) |
| Ordinary build/test leaves fixtures unchanged | PASS (`git status` frames: fill_basic clean; new dirs only before first check-in) |

Commands:

```text
python scripts/preflight.py                          exit 0
cmake -S . -B build/stage002 && cmake --build ...    exit 0
ctest --test-dir build/stage002 --output-on-failure  exit 0 (22/22)
```

Compiler: GNU 14.2.0 MinGW / CMake 3.31.5 / Python 3.10.0

## 8. Random Regression

- Algorithm: LCG `s = s*1664525 + 1013904223`
- Seeds: `1, 42, 12345, 0xC0FFEE, 7, 99, 20260912` (+20 offsets each)
- ~140 sequences of FILL/BLIT; all must succeed with in-bounds rects

## 9. Limitations

- No BLIT_EXT / scale / flip / clip / palette / premult / color-mod / dither / bilinear / tile
- Source/destination overlap for BLIT not defined in specs → not exercised as memmove
- Destination format RGB565 only this stage

## 10. Blockers / Design Questions

NONE open.

## 11. Task Deviations

- Header LENGTH_DW still always requires 16 for this fixed-64B Stage-002 runner (needed to parse binary streams); reserved/EXT_PTR checks are strict-gated per A4.
- `golden_add_blit_fixture` unused stub removed in favor of `golden_add_blit_fixture2`.

## 12. Suggested Next Stage

**Stage 003 — Extended Sprite Path: BLIT_EXT, Scaling, Flip, Clip, Indexed8/Palette, Color Mod, Bilinear** (pending REVIEW_002).
