# REVIEW_004_V5 — Golden Tile Renderer Final-Gate Review

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Previous reviewed HEAD: `bb810d35704f4e9e7ad3ff93624a0e282674bcd7`  
> Stage implementation commit: `2a85349e83087dd15d8035688c02d2b921ecbd54`  
> Reviewed HEAD / report commit: `c77e77a1fe3cf74ffc2532988905d57f9d8fa617`  
> Decision: **FAIL / CONTINUE STAGE 004 REWORK**  
> Architecture redesign required: **NO**  
> Permission to start Stage 005 RTL: **NO**

## 1. Executive Summary

The latest Stage-004 rework contains substantial genuine progress.

The central architecture is now credible:

```text
CPU Software Binner
→ serialized Draw Descriptor / Tile Header / WorkRef
→ TILE_FRAME decode
→ separate internal Tile MemoryImage
→ Tile load / local rendering / Tile store
→ shared Golden sampler / blend / format path
```

The implementation also adds a shared PixelEventSink, checked Tile fixture files, BLIT_EXT nearest/bilinear cases in the 300-frame random suite, a Tile extended-equivalence test, a functional sweep executable, and a checker that again rejects mandatory PARTIAL items.

However Stage 004 still cannot pass its own completion contract.

The remaining blockers are concentrated in:

1. incomplete Tile equivalence coverage;
2. Tile fixtures that exist but are not replayed/validated by CTest;
3. profiler/overdraw semantic errors;
4. six-workload sweep claims that do not match the actual generator;
5. an Acceptance Manifest/checker that still accepts synthetic/nonexistent evidence.

There are also two architectural-spec issues: `LOAD_COLOR_DEFAULT` is still open, and Tile Reserved behavior still disagrees with the frozen Strict policy.

Therefore:

# **REVIEW_004_V5 = FAIL / CONTINUE STAGE 004**

Do not begin Stage 005 RTL.

## 2. Accepted Corrections

### K1 — Tile scratch is now genuinely separated from architectural framebuffer memory

`GoldenGPU` now owns separate architectural and Tile MemoryImage objects. The Tile scratch is no longer assigned an external framebuffer/texture physical address.

**Status: CLOSED**

### K2 — Tile execution can target an alternate RT memory object

The shared draw path can target an alternate destination memory/resource, allowing Tile to reuse the existing sampler/pixel backend without duplicating the pixel pipeline.

**Status: CLOSED**

### K3 — RT_STATE remains command-authoritative

The low-level TILE_FRAME builder preserves RT_STATE exactly.

**Status: CLOSED**

### K4 — Random suite now includes BLIT_EXT nearest and Bilinear

The deterministic random suite has expanded beyond FILL/BLIT to include BLIT_EXT nearest and bilinear.

**Status: ACCEPTED as progress**

It still does not fully satisfy EQ-07.

### K5 — Checked Tile fixture files physically exist

The five requested Tile fixture directories have been added.

**Status: PARTIAL**

They are not yet active executable regressions.

### K6 — Functional sweep invokes the real Tile model

The Python sweep launches the compiled Golden Tile executable, which runs `bin_draws()`, `execute_tile_frame()` and exports current `TileStats`.

**Status: PARTIAL ACCEPT**

The workload definitions remain invalid/incomplete.

## 3. Reviewer Resolution — LOAD_COLOR_DEFAULT Design Question

The repository correctly raised `DESIGN_QUESTION_TILE_LOAD_COLOR_DEFAULT.md`.

This review resolves it.

### K7 — Reviewer Design Decision: APPROVED WITH EXACT RULE

Freeze the Competition V0.1 compatibility rule as follows:

```text
Initialization priority:

1. TILE_CLEAR_COLOR = 1
   → initialize valid Tile pixels from CLEAR_COLOR
     through normal DST_FORMAT conversion.

2. Else TILE_DONT_LOAD_COLOR = 1:
      LOAD_COLOR_DEFAULT = 1
      → initialize valid Tile pixels from canonical 0x00000000
        through normal DST_FORMAT conversion.

      LOAD_COLOR_DEFAULT = 0
      → UNSUPPORTED_FEATURE in the V0.1 compatibility profile.

3. Else
   → load valid Tile pixels from the framebuffer.

STORE_COLOR = 1
→ store final valid Tile pixels to framebuffer.

STORE_COLOR = 0
→ do not update framebuffer on Tile retire.
```

Consequences:

```text
RGB565 default   → black 0x0000
ARGB8888 default → 0x00000000
XRGB8888 default → 0xFF000000
```

`LOAD_COLOR_DEFAULT=1` has no effect when a real framebuffer load occurs.

Update the decision file to:

```text
FROZEN — REVIEW_004_V5
```

and synchronize the rule into a later controlled ISA/System Architecture revision.

## 4. Architectural / Robustness Findings

### K8 — Tile Reserved behavior still violates the frozen Strict policy

Severity: **HIGH**

Command ISA V0.1 says nonzero Reserved fields should fault in Strict mode.

Current TILE_FRAME execution rejects RT_STATE[31:9], Tile Header W3 and TILE_FLAGS[31:4] unconditionally.

Required:

```text
H_STRICT = 1 → nonzero Reserved → RESERVED_NONZERO
H_STRICT = 0 → ignore Reserved fields/bits
```

Add paired tests for all three Tile Reserved locations.

### K9 — Tile profiler pointer is not restored on fault exits

Severity: **HIGH**

`execute_tile_frame()` installs a stack-local `PixelEventSink` into `gpu.perf`, then contains multiple direct fault returns. `gpu.perf` is restored only at the successful end-of-function path.

A Tile fault can therefore leave `gpu.perf` pointing at a destroyed stack object.

Required: use an RAII guard or a single cleanup path, and add:

```text
faulting TILE_FRAME
→ subsequent valid draw on same GoldenGPU
→ succeeds with no stale profiler access
```

## 5. Immediate-vs-Tile Equivalence Findings

### K10 — EQ-07 still lacks Clip and Indexed8/Palette

Severity: **CRITICAL**

The current random generator covers:

```text
FILL
Straight Alpha
Additive
BLIT_EXT nearest
BLIT_EXT bilinear
RGB565 BLIT / Color Key
```

It still does not generate:

```text
Clip
Indexed8 + Palette
```

even though TASK_004 requires both.

The manifest nevertheless marks EQ-07 PASS.

Required: add both classes and explicit coverage counters/assertions proving every required feature class occurred.

### K11 — Directed Tile equivalence matrix remains incomplete

Severity: **CRITICAL**

`golden_test_tile_extended` proves useful cases for ARGB8888 destination + RGB565 BLIT, PREMULT command path, Clip crossing a Tile boundary, and RGB565 Dither crossing a Tile boundary.

Still missing dedicated Tile-path evidence includes at least:

```text
XRGB8888 source
XRGB8888 destination
Color Mod
Global Alpha
explicit Per-Pixel Alpha BLIT
Nearest scaling
Bilinear scaling in a directed registered test
Clamp
Repeat
Indexed8 + Palette
```

Additional test-quality issue: the “Premult ARGB source” test prepares premultiplied ARGB pixels in a temporary GoldenGPU that is then discarded. The actual Immediate/Tile GPUs use separately initialized texture memory, so the test does not use the intended prepared premult texture.

Required: create the source texture in the actual Immediate and Tile GPU memory used by the comparison.

## 6. Tile Fixture Findings

### K12 — Tile fixture files exist, but are not executable regression tests

Severity: **CRITICAL — EQ-09**

The five fixture directories now contain binary artifacts, but CMake does not register Tile fixture replay tests.

There is no formal regression path equivalent to:

```text
load TILE_FRAME command.bin
load descriptors.bin
load tile_headers.bin
load workrefs.bin
load resources
execute TILE_FRAME
compare framebuffer against golden_fb.raw
```

Required: implement a dedicated Tile fixture replay path and register all five fixtures in CTest.

### K13 — Existing fixture validator does not validate Tile fixtures

Severity: **HIGH**

`fixture_validate.py` scans only direct children of `frames/` containing `manifest.json`.

The Tile fixtures live under:

```text
frames/tile/<fixture>/
```

so they are skipped.

The validator also accepts only DRAW_2D opcodes 0x00/0x01/0x02, not TILE_FRAME 0x10.

The inspected Tile manifest also lacks the validator-required `pixel_arith_version` field.

Therefore “fixtures generated + fixture_validate” is not evidence for EQ-09.

Required: add recursive Tile-aware validation or a dedicated Tile fixture validator.

## 7. Profiler Findings

### K14 — Per-pixel overdraw coordinates are wrong in Tile mode

Severity: **CRITICAL**

Tile translates draw coordinates to Tile-local coordinates and stores global Tile origin in `dither_ox/dither_oy`.

Framebuffer/Dither write coordinates correctly use:

```text
dx + dither_ox
dy + dither_oy
```

but the profiler records:

```text
note_write(dx, dy)
```

without the Tile origin.

Thus equal local coordinates in different Tiles collapse into the same overdraw slot.

Required:

```text
note_write(dx + global_tile_x,
           dy + global_tile_y)
```

or an explicit global RT coordinate path.

Add a two-Tile test that distinguishes local and global overdraw indexing.

### K15 — PROF-02 remains incomplete

Severity: **HIGH**

`PixelEventSink` declares `clip_rejects`, but the inspected raster/pixel path does not increment it, and `TileStats` does not expose a `clip_rejects` counter.

TASK_004 explicitly requires clip rejects.

Required: instrument Clip rejection at the actual coverage/raster decision point, propagate it to TileStats, and test exact counts.

## 8. Tile Sweep Findings

### K16 — The claimed six workloads are not actually six distinct workloads

Severity: **CRITICAL — EXP-01**

The sweep executable prints six names, but the generator implements:

```text
kind 0 → ordinary 8×8 fills
kind 1 → alpha fills
kind 2 → 128 ordinary fills
kind >= 3 → the same 4×4 RGB565 BLIT pattern
```

So W4, W5 and W6 are not distinct.

The checked CSV confirms `kind3`, `kind4` and `kind5` have identical metrics for every Tile size.

Also the required `W4 Large Scaled Sprites` is not implemented.

Required: implement six actual workloads:

```text
W1 Sprite Grid
W2 High Overdraw Stack
W3 Alpha Storm
W4 Large Scaled Sprites
W5 Edge/Scatter Sprites
W6 Mixed Sprite Scene
```

### K17 — Sweep CSV loses workload names

Severity: **MEDIUM**

The C++ executable emits `workload=W...`, but Python intentionally discards the `workload` token and falls back to `kind0`, `kind1`, etc.

Required: preserve stable workload ID/name and add a workload version/seed field.

## 9. Acceptance-System Findings

### K18 — Acceptance Manifest still contains synthetic/nonexistent evidence

Severity: **CRITICAL — AUD-01**

Many entries contain strings like:

```text
model/golden/gvf-01
ctest build/stage004 GVF-01
```

These are not real files/functions or actual CTest test names.

This violates TASK_004’s evidence contract.

Required: every PASS entry must name concrete repository paths/functions and concrete tests.

### K19 — Acceptance checker still does not validate evidence

Severity: **CRITICAL — AUD-02**

The checker now correctly rejects mandatory PARTIAL items, but still derives the ID set from the manifest itself and only checks that fields are non-empty and that the ID string appears in the report.

It does not verify:

- authoritative exact 47-ID set;
- duplicates/missing/extra IDs;
- evidence paths exist;
- evidence is not placeholder text;
- declared tests exist in CTest;
- REPORT_004 contains a real 47-row evidence matrix.

Required: implement the checker contract originally specified by TASK_004.

### K20 — REPORT_004 is internally inconsistent

Severity: **HIGH**

The current report contains a new PASS section followed by a stale appended prior report.

It simultaneously records:

```text
64/64 PASS
```

and:

```text
66/66 PASS
```

and still contains older PARTIAL statements below the new PASS claim.

It also says J8 is OPEN and J9 is PARTIAL while declaring Stage PASS.

Required: rewrite REPORT_004 cleanly, with one result, one exact END_COMMIT, one test count, and one full 47-row evidence matrix.

## 10. Local Test Evidence

The latest report records 64/64 PASS, while its stale appended section records 66/66.

This review did not independently execute the local build through the GitHub connector, and the reviewed HEAD has no published commit-status checks.

Treat these as Agent-reported local results, not independently reproduced CI evidence.

## 11. Gate Decision

The central Tile architecture is now sufficiently mature and should not be redesigned.

Accepted core:

```text
software binning
serialized Tile work structures
TILE_FRAME target authority
separate internal Tile memory
shared sampler/pixel backend
Tile load/render/store
WorkRef ordering
global dither coordinates
basic/mixed Immediate-vs-Tile equivalence
```

Mandatory Stage-004 closure still fails on:

```text
full Tile equivalence matrix
Clip/Palette random coverage
active replay/validation of checked Tile fixtures
correct global overdraw profiler
clip-reject profiling
true six-workload sweep
Strict Reserved semantics
real per-ID evidence
strong acceptance checker
clean final report
```

Therefore:

# **REVIEW_004_V5 = FAIL / CONTINUE STAGE 004**

Stage 005 RTL remains blocked.

## 12. Recommended Final Closure Order

1. Apply the reviewer-frozen LOAD_COLOR_DEFAULT rule and mark the decision FROZEN.
2. Fix H_STRICT behavior for Tile Reserved fields and add paired tests.
3. Add RAII restoration for `gpu.perf`.
4. Complete XRGB/Mod/GlobalAlpha/Scale/Bilinear/Clamp/Repeat/Palette directed Tile equivalence.
5. Add Clip + Indexed8/Palette to the 300-frame deterministic random corpus.
6. Register all five Tile fixtures as replay+compare CTests.
7. Extend fixture validation to Tile format and fix Tile manifests.
8. Fix global overdraw coordinates and implement clip-reject profiling.
9. Implement the real six distinct functional sweep workloads and regenerate CSV/summary.
10. Replace all synthetic Acceptance evidence with real paths/functions/tests.
11. Strengthen `check_stage004_acceptance.py` to validate the exact 47 IDs, paths and CTest names.
12. Rewrite REPORT_004 cleanly with one exact test count and a real 47-row evidence matrix.

After these are complete, Stage 004 should be ready to pass and Stage 005 RTL can begin.
