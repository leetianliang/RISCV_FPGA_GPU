# REPORT_001 — Golden GPU Foundation & FILL

## 1. Result

**PASS** (pending reviewer approval)

SPEC PREFLIGHT remains PASS. Frozen specifications unmodified. No third-party dependencies. CTest 8/8 passed.

## 2. START_COMMIT / END_COMMIT

- START_COMMIT: `610b0fb87a97d77243f480c00a9eaf498de2f717` (TASK_000 baseline / REVIEW_000)
- END_COMMIT: `0a22fe8b1dae67bb6a5f04a9bc53e58c0070f5cd`

## 3. Subtasks Implemented

| Subtask | Status |
|---|---|
| 001.1 Build/Test Infrastructure | DONE — `golden_core` + CTest targets |
| 001.2 Core Types | DONE — `gpu_types.hpp` |
| 001.3 Bit-Exact Arithmetic | DONE — `gpu_math` |
| 001.4 Arithmetic Verification | DONE — exhaustive RGB565/DIV255/MUL8 + directed Q16/LERP |
| 001.5 MemoryImage | DONE — region model, LE RW, overlap reject |
| 001.6 Surface | DONE — RGB565 read/write via MemoryImage |
| 001.7 Exact 64B Command | DONE — `GpuCmd64` = `std::array<u32,16>` |
| 001.8 Header Decode/Validate | DONE |
| 001.9 FILL Payload Decode | DONE — Stage-001 profile COPY/RGB565 |
| 001.10 FILL Golden Execution | DONE — half-open rect, zero-size no-op |
| 001.11 GoldenGPU Facade | DONE — `execute_command(GpuCmd64)` |
| 001.12 Directed FILL Tests | DONE — FILL-001..010 |
| 001.13 Binary Golden Fixture | DONE — `tests/frames/fill_basic/` via `golden_cli generate-fill-basic` |
| 001.14 Frame Compare Tool | DONE — `tools/frame_compare/frame_compare.py` |
| 001.15 Binary Fixture Runner | DONE — `golden_cli run-fill` + CTest |
| 001.16 CTest Integration | DONE — all registered tests PASS |
| 001.17 Project Status | DONE — `docs/PROJECT_STATUS.md` |

## 4. Files Added

- `model/golden/include/golden/{gpu_types,gpu_isa,gpu_math,memory_image,surface,command_decoder,golden_gpu}.hpp`
- `model/golden/src/{gpu_math,memory_image,surface,command_decoder,golden_gpu}.cpp`
- `model/golden/tests/unit/test_{gpu_math,memory_image,surface}.cpp`
- `model/golden/tests/directed/test_{command_header,fill_directed}.cpp`
- `model/golden/tests/frames/fill_basic/{command.bin,initial_fb.raw,golden_fb.raw,manifest.json}`
- `model/golden/tools/golden_cli.cpp`
- `model/golden/README.md` (updated layout note optional)
- `tools/frame_compare/frame_compare.py`
- `docs/reviews/REVIEW_000_Project_Initialization.md`
- `docs/reports/REPORT_001_Golden_Foundation_and_FILL.md`

## 5. Files Modified

- `CMakeLists.txt` — `enable_testing()` at root
- `model/golden/CMakeLists.txt` — real targets and tests
- `docs/PROJECT_STATUS.md`
- `docs/SPEC_STATUS.md` — Gate A PASSED after REVIEW_000
- Existing specification documents under `docs/RISC-V_FPGA_2D_GPU_*.md`: **NONE**

## 6. Specification Compliance

- Command layout, VERSION=1, LENGTH_DW=16, FILL class/opcode per Command ISA V0.1.
- RGB565 decode/encode, DIV255_RN, MUL8_RN, Q16.16 helpers per Pixel Arithmetic V0.1 reference algorithms (no float, no SIMD).
- FILL ignores SRC_* / PALETTE_ADDR / ALPHA_KEY for output.
- Unsupported blend/dither/clip/ext → `FAULT_UNSUPPORTED_FEATURE` (not silently COPY).
- Surface dimensions are harness metadata (DESIGN_QUESTION anticipated in task; resolved as harness registration without ISA change).

## 7. Arithmetic Verification

| Check | Cases | Result |
|---|---|---|
| RGB565 encode(decode(x))==x | 65536 | PASS |
| DIV255_RN x=0..65025 | 65026 | PASS |
| MUL8_RN | 256×256 | PASS |
| Q16 / signed round-div / lerp16 directed | directed | PASS |

## 8. Unit / Directed / Binary Results

- `golden_test_gpu_math`: PASS
- `golden_test_memory_image`: PASS
- `golden_test_surface`: PASS
- `golden_test_command_header`: PASS
- `golden_test_fill_directed` (FILL-001..010): PASS
- `golden_fill_basic_fixture_run`: PASS
- `golden_fill_basic_frame_compare`: PASS (exact 512-byte match)
- `golden_fill_basic_frame_compare_mismatch`: PASS (detects intentional flip)

## 9. Build / CTest Commands

```text
python scripts/preflight.py                          exit 0
cmake -S . -B build/stage001                        exit 0
cmake --build build/stage001                        exit 0
ctest --test-dir build/stage001 --output-on-failure exit 0 (8/8)
```

Manual frame_compare:

```text
match: exit 0
mismatch (byte 10 flipped): exit 1
```

## 10. Compiler / Version

- Compiler: GNU 14.2.0 (MinGW-w64) / Ninja
- Python: 3.10.0
- CMake: 3.31.5

## 11. Warnings / Limitations

- Stage 001 implements FILL + RGB565 + COPY only; other blend modes / formats / dither / clip / extension commands report unsupported.
- Directed tests restrict DST_XY non-negative and in-bounds (stage limitation, not full ISA).
- Full-ISA negative destination coordinates / clipping not implemented yet.
- Surface width/height not recoverable from command bytes alone; harness must register surfaces.

## 12. Blockers

NONE

## 13. Design Questions

NONE open. Surface-metadata dependency is implemented as harness registration per TASK_001 §15.

## 14. Task Deviations

- Golden public headers live under `include/golden/` (task structure) rather than flat `include/` in architecture doc — allowed by task (“private decomposition may differ”).
- CTest registers one executable per test translation unit (avoids multiple `main`).
- Fixture generation is a build-time custom target (`golden_cli generate-fill-basic`) writing into `tests/frames/fill_basic/`.

## 15. Suggested Next Stage

**Stage 002 — Golden BLIT & Basic Texture Path** (pending REVIEW_001).
