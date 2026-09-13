# REVIEW_004_V6 — Golden Tile Renderer Latest Gate Review

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Previous reviewed HEAD: `c77e77a1fe3cf74ffc2532988905d57f9d8fa617`  
> Reviewed implementation / HEAD: `2f1fdc718902adf72ca381b3941e6c24dc9a87fa`  
> Decision: **FAIL / CONTINUE STAGE 004 REWORK**  
> Architecture redesign required: **NO**  
> Permission to start Stage 005 RTL: **NO**

## 1. Executive Summary

This rework closes several important REVIEW_004_V5 findings correctly. The LOAD_COLOR_DEFAULT rule is frozen, Strict-only Tile Reserved checks are implemented, `gpu.perf` restoration is RAII-protected, global overdraw coordinates are fixed, `clip_rejects` is propagated, the sweep generator now has behaviorally distinct W1–W6 paths, and the five Tile fixtures are registered as CTest replay tests.

The central Tile architecture is accepted and should not be redesigned.

However Stage 004 still does not meet its mandatory completion contract. The current blockers are verification/evidence closure rather than architecture:

- EQ-07 still lacks Clip + Indexed8/Palette random coverage.
- Directed Tile equivalence still misses mandatory semantics/formats.
- The Tile fault matrix is still incomplete.
- Tile fixture replay mutates the checked TILE_FRAME command before execution.
- Tile fixture metadata is still not validated by the generic validator.
- PROF-03 still does not expose per-pixel overdraw counts.
- The acceptance manifest still contains synthetic evidence.
- The checker still does not validate paths/tests/the authoritative 47-ID set.
- REPORT_004 is stale and contradictory and has no current exact test result for this HEAD.

Therefore:

# **REVIEW_004_V6 = FAIL / CONTINUE STAGE 004**

Do not begin Stage 005 RTL.

## 2. Accepted Corrections

### L1 — LOAD_COLOR_DEFAULT design rule frozen
The decision document now records the REVIEW_004_V5 rule and may be treated as Stage-004 Golden authority. Later controlled ISA/System Architecture text should synchronize it.

### L2 — Tile Reserved handling follows H_STRICT in implementation
RT_STATE Reserved bits, Tile Header W3 and TILE_FLAGS Reserved bits are checked only in Strict mode.

### L3 — Profiler pointer lifetime fixed
`execute_tile_frame()` now uses an RAII guard to restore `gpu.perf` on all exits.

### L4 — Global overdraw coordinates fixed
Profiler writes now use global RT coordinates via Tile origin offsets.

### L5 — `clip_rejects` now instrumented and propagated
Raster clipping increments the counter and TileStats carries it.

### L6 — Six sweep workload generators are behaviorally distinct
W1 Sprite Grid, W2 High Overdraw, W3 Alpha Storm, W4 scaled sprites, W5 edge/scatter, and W6 mixed scene now have distinct command-generation logic.

### L7 — Tile fixtures are active CTest replay tests
CMake registers five `golden_tile_fixture_*` tests invoking `golden_cli run-tile-fixture`.

## 3. Blocking Equivalence Findings

### M1 — EQ-07 random corpus still lacks Clip and Indexed8/Palette
Severity: **CRITICAL**

The deterministic generator still has only six random kinds: FILL, Straight Alpha FILL, Additive FILL, BLIT_EXT nearest, BLIT_EXT bilinear, and RGB565 BLIT/Color Key. It does not generate Clip or Indexed8+Palette, although TASK_004 explicitly requires both.

Required: add deterministic random classes for BLIT_EXT+Clip and INDEX8+Palette, plus coverage assertions proving all mandatory feature classes occurred.

### M2 — Directed Tile equivalence matrix still misses mandatory cases
Severity: **CRITICAL**

The dedicated extended Tile test still centers on ARGB destination BLIT, PREMULT command path, Clip across Tile boundary, and RGB565 Dither. It still lacks dedicated Tile evidence for at least XRGB8888 source, XRGB8888 target, Color Mod, and Repeat. The formal acceptance matrix also needs explicit mapping for Global Alpha, Per-Pixel Alpha, nearest/bilinear scaling, Clamp and Indexed8+Palette.

The Premult test still prepares intended premultiplied ARGB data in a temporary GPU that is discarded before the actual Immediate/Tile comparison, so the prepared texture is not the texture actually tested.

Required: install source assets into the actual Immediate and Tile GPU memories and add the missing cases.

## 4. Fault Verification Findings

### M3 — Strict Reserved code changed, but paired tests were not added
Severity: **HIGH**

`test_tile_faults.cpp` was not updated in this rework. There are still no Strict=0/Strict=1 paired tests for RT_STATE Reserved bits, Tile Header W3 and TILE_FLAGS Reserved bits.

### M4 — Required Tile fault matrix remains incomplete
Severity: **CRITICAL**

The dedicated fault suite still does not cover the full required matrix: header bounds, WorkList range, descriptor index, misalignment, unmapped descriptor/header/workref memory, strict target base mismatch, strict target stride mismatch, strict target format mismatch, and full reserved-state matrix.

Required: make `golden_test_tile_faults` table-driven with one exact FaultCode per case.

## 5. Tile Fixture Findings

### M5 — Tile fixture replay mutates serialized TILE_FRAME pointers
Severity: **HIGH**

`run-tile-fixture` deserializes `command.bin` and then overwrites W4/W5/W6 before execution. This can hide corruption or inconsistency in the checked command and violates the intended binary-authority replay path.

Required: register fixture memory at the addresses encoded in `command.bin`; do not modify command words during replay.

### M6 — Tile fixture validation still bypasses nested Tile fixtures
Severity: **HIGH**

The generic fixture validator scans only direct children of `frames/`, while Tile fixtures live under `frames/tile/<fixture>/`. It also understands only FILL/BLIT/BLIT_EXT, not TILE_FRAME.

Required: extend validation recursively with TILE_FRAME semantics or add a dedicated Tile fixture validator.

## 6. Profiler Findings

### M7 — PROF-03 still lacks exported per-pixel overdraw counts
Severity: **HIGH**

The internal PixelEventSink owns a per-pixel overdraw vector, but that sink is stack-local to `execute_tile_frame()`. `TileStats` exposes only `max_overdraw` and `avg_overdraw_touched`; the required raw per-pixel overdraw count/matrix is not provided after execution.

Required: persist/export the overdraw vector or a raw matrix/CSV.

## 7. Sweep Findings

### M8 — Workload logic is fixed, but CSV naming/traceability is still wrong
Severity: **MEDIUM**

The C++ tool now generates distinct workloads, but `run_tile_sweep.py` still discards the emitted `workload=W...` token and falls back to `kind0`…`kind5`. The committed CSV therefore still loses stable W1–W6 identity. The C++ display name for W4 also remains `W4_blit_mix` even though it is now the scaled-sprite workload.

Required: preserve `workload_id`, `workload_name`, version and seed.

### M9 — Sweep summary is stale
Severity: **MEDIUM**

`tile_sweep_summary.md` was not changed in the latest rework, despite W4/W5/W6 changing materially. Regenerate the summary from the current six-workload CSV.

## 8. Acceptance-System Findings

### M10 — Acceptance manifest still contains synthetic evidence
Severity: **CRITICAL**

Many PASS entries still use values such as `model/golden/gvf-01` and `ctest build/stage004 GVF-01`, which are not actual repository paths/functions or real CTest names.

Required: replace every PASS row with concrete implementation paths/functions and concrete tests.

### M11 — Acceptance checker remains too weak
Severity: **CRITICAL**

The checker verifies only PASS status, non-empty evidence/test fields, and whether each ID string appears somewhere in REPORT_004. It still does not verify the authoritative exact 47-ID set, duplicates, evidence-path existence, placeholder evidence, declared CTest names, or a structured 47-row report matrix.

Required: implement the TASK_004 checker contract and validate against `ctest -N`.

## 9. Report / Test-Evidence Findings

### M12 — REPORT_004 was not updated for this rework
Severity: **CRITICAL**

The current report still records END_COMMIT `2a85349`, contains stale OPEN/PARTIAL items, and simultaneously contains old `64/64` and `66/66` CTest claims. It does not describe the reviewed HEAD `2f1fdc718902adf72ca381b3941e6c24dc9a87fa`.

Required: rewrite REPORT_004 cleanly after implementation is final.

### M13 — No current exact test result exists for the reviewed HEAD
Severity: **HIGH**

The latest commit adds five new fixture CTests and changes Tile/profiler/sweep behavior, but no current report records a full test run for this HEAD. GitHub has no published status checks for the commit.

Required: run and record preflight, full build/CTest, fixture validation, Tile fixture replay/validation, integrity checker, acceptance checker, tile sweep and `git status`.

## 10. Gate Decision

Accepted core:
- Software Binner
- serialized Descriptor/Header/WorkRef
- WorkRef ordering
- separate internal Tile memory
- Tile load/render/store
- shared sampler/pixel backend
- RT target authority
- global Dither coordinates
- global overdraw coordinates
- frozen LOAD default rule
- Strict Reserved implementation
- RAII profiler lifetime

Still blocking Stage 005:
- full mandatory Tile equivalence matrix
- Clip + Palette random coverage
- complete Tile fault matrix
- pure binary Tile fixture replay
- Tile fixture metadata validation
- per-pixel overdraw export
- real acceptance evidence/checker
- current clean final report/test result

Therefore:

# **REVIEW_004_V6 = FAIL / CONTINUE STAGE 004**

This is now a closure/verification failure, not an architecture failure.

## 11. Recommended Final Closure Order

1. Add Clip + Indexed8/Palette into deterministic EQ-07 random corpus.
2. Complete XRGB / Color Mod / Repeat and remaining directed Tile equivalence.
3. Fix the Premult directed-test asset setup.
4. Add Strict/non-Strict Reserved paired tests.
5. Complete the full Tile fault matrix.
6. Stop patching TILE_FRAME pointers in fixture replay.
7. Add Tile-aware fixture validation.
8. Export per-pixel overdraw data.
9. Preserve W1–W6 names/version/seed in sweep CSV and update summary.
10. Replace synthetic Acceptance evidence with actual paths/tests.
11. Strengthen `check_stage004_acceptance.py`.
12. Run the entire final verification suite on the final commit.
13. Rewrite REPORT_004 from scratch with exact final SHA/test count/evidence matrix.

Only after these items are complete should Stage 004 be marked PASS and Stage 005 RTL begin.
