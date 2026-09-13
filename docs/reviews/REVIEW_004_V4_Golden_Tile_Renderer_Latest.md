# REVIEW_004_V4 — Golden Tile Renderer Latest Review

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Previous reviewed HEAD: `914867a296de71f8539bdf5f8f71baa13976e2d6`  
> Reviewed implementation commit / HEAD: `bb810d35704f4e9e7ad3ff93624a0e282674bcd7`  
> Decision: **FAIL / CONTINUE REWORK**  
> Architecture redesign required: **NO**  
> Permission to start Stage 005 RTL: **NO**

---

# 1. Executive Summary

The latest Stage-004 rework makes real progress:

- `desc_count_hint` is gone from the public Tile execution API;
- TILE_FRAME grid dimensions are validated;
- RT_STATE is serialized exactly;
- depth Tile flags are rejected rather than silently ignored;
- Tile-size reconfiguration receives a regression test;
- the random Tile equivalence suite now includes FILL, Straight Alpha, Additive, BLIT and Color Key;
- the Tile sweep is now driven through a compiled Golden Tile executable rather than a pure Python estimate;
- the acceptance manifest now at least marks several unfinished items as `PARTIAL`.

The core architecture remains suitable:

```text
software binning
→ serialized descriptors / headers / workrefs
→ TILE_FRAME
→ tile-local scratch render target
→ shared Golden pixel backend
→ framebuffer store
```

However Stage 004 still does not satisfy its own completion contract.

The repository/report explicitly leaves mandatory equivalence, fixture and profiler items partial. In addition, the attempted “internal scratch” fix is not actually internal yet, the sweep workloads are mislabeled/incomplete, and the acceptance checker was weakened so that mandatory `PARTIAL` items still produce an overall PASS.

Therefore:

# **REVIEW_004_V4 = FAIL / CONTINUE REWORK**

Stage 005 remains blocked.

---

# 2. Accepted Improvements

## J1 — Formal descriptor-count side channel removed

`execute_tile_frame()` now derives descriptor capacity from the resource registered at `DRAW_DESC_BASE`.

**Status: CLOSED**

---

## J2 — Exact RT_STATE serialization

`make_tile_frame_cmd()` now preserves the caller's RT_STATE exactly rather than forcing `STORE_COLOR`.

**Status: CLOSED**

---

## J3 — Grid validation added

The implementation now checks:

```text
grid_w == ceil(surface_w / tile_w)
grid_h == ceil(surface_h / tile_h)
```

and returns `BAD_TILE_CONFIG` on mismatch.

**Status: CLOSED for this rule**

---

## J4 — Tile depth flags are explicitly rejected

`TILE_LOAD_DEPTH` / `TILE_CLEAR_DEPTH` no longer silently disappear; Stage-004 correctly rejects unsupported depth operation.

**Status: CLOSED for no-depth Stage-004 profile**

---

## J5 — Mixed random suite improved

The deterministic 300-frame Tile 16/32/64 corpus now contains:

```text
FILL
Straight Alpha
Additive
RGB565 BLIT
Color Key
```

This is materially stronger than the previous FILL-heavy test.

**Status: PARTIAL ACCEPT**

---

## J6 — Functional-model sweep executable added

`golden_tile_sweep` now runs the real binner + TILE_FRAME path and exports actual current `TileStats`.

This closes the previous pure-Python analytical-only implementation problem.

**Status: PARTIAL ACCEPT**

The workload definition and metrics remain insufficient; see blocking findings.

---

# 3. Blocking Architecture / Semantic Findings

## J7 — “Internal Tile scratch” still uses architectural MemoryImage and a fixed physical address

Severity:

> **HIGH**

`GoldenGPU` now defines an `InternalTileMem` vector and comments that it is:

```text
Internal (non-architectural) tile scratch — not addressable via MemoryImage
```

but `execute_tile_frame()` does not use that object.

The actual rendering scratch is still implemented by:

```text
scratch_base = 0x7F000000
MemoryImage::register_region(...)
resources_[scratch_base] = tile_scratch_internal
```

The new internal vector therefore does not remove the hidden physical-address reservation.

This creates several problems:

1. Tile scratch is still visible through the same MemoryImage address namespace as GPU architectural memory.
2. A user resource at that physical address prevents Tile execution.
3. A raw MemoryImage region at that address without a matching RegisteredResource can be removed by the Tile scratch path.
4. Internal Tile traffic remains structurally indistinguishable from architectural memory traffic.

### Required correction

Use the already-created `InternalTileMem` or another separate internal storage object as the real Tile RT backing.

The shared pixel backend may need an RT adapter that can target either:

```text
architectural SurfaceView
or
internal TileSurfaceView
```

without assigning an external GPU physical address to internal scratch.

Do not leave `InternalTileMem` as dead architecture scaffolding while formal Tile execution still uses `MemoryImage`.

---

## J8 — LOAD_COLOR_DEFAULT semantics are invented, not frozen by the current ISA text

Severity:

> **HIGH — specification authority**

The ISA currently defines a bit named:

```text
LOAD_COLOR_DEFAULT
```

but the checked specification does not define what color/value it loads or its exact interaction with `TILE_DONT_LOAD_COLOR` / `TILE_CLEAR_COLOR`.

The implementation currently defines a new rule:

```text
DONT_LOAD without LOAD_COLOR_DEFAULT → UNSUPPORTED
DONT_LOAD + LOAD_COLOR_DEFAULT       → zero-fill
```

and REPORT_004 marks this behavior CLOSED.

That is an implementation decision, not a source-defined V0.1 semantic.

### Required correction

Raise a `DESIGN_QUESTION` and explicitly freeze one rule before treating this behavior as Golden authority.

Recommended decision document/spec revision should define the full matrix:

```text
LOAD_COLOR_DEFAULT
TILE_DONT_LOAD_COLOR
TILE_CLEAR_COLOR
STORE_COLOR
```

including precedence and the actual default color value.

Until then H8/G3 must not be described as architecturally closed.

---

## J9 — Reserved-field behavior remains inconsistent with Strict policy

Severity:

> **MEDIUM/HIGH**

The ISA states that nonzero Reserved fields should report `FAULT_RESERVED_NONZERO` in Strict mode.

Current Tile execution rejects at least:

```text
RT_STATE[31:9]
Tile Header W3
TILE_FLAGS[31:4]
```

unconditionally.

No strict/non-strict paired Tile tests were added in the latest rework.

### Required correction

Either:

1. make these checks conditional on `H_STRICT`, matching the existing project interpretation; or
2. deliberately freeze stronger Tile-specific validation in a documented specification decision.

Then add paired tests for Strict=0 and Strict=1.

---

# 4. Tile Equivalence Findings

## J10 — Mandatory EQ-07 random feature distribution is still incomplete

Severity:

> **CRITICAL**

The 300-frame random corpus now includes five useful classes, but still omits required random coverage for:

```text
BLIT_EXT nearest
Clip
Bilinear
Indexed8 + Palette
```

The acceptance manifest correctly marks `EQ-07 = PARTIAL`.

### Required correction

Add deterministic extension / palette resource pools and guarantee all required feature classes occur across the fixed-seed corpus.

Do not merely make the random generator capable of choosing them; add coverage counters/assertions so a seed change cannot accidentally remove a feature class.

---

## J11 — Directed Immediate-vs-Tile feature matrix remains incomplete

Severity:

> **CRITICAL**

The manifest now honestly marks `EQ-02`, `EQ-03`, `EQ-04`, and `EQ-05` as PARTIAL.

Still required are concrete Tile-path comparisons for major Stage-003 semantics, including:

```text
ARGB8888 source
XRGB8888 source
Color Mod
Global Alpha
Per-Pixel Alpha
Premultiplied Alpha
BLIT_EXT nearest scaling
Bilinear scaling
Clamp
Repeat
Clip
Indexed8 + Palette
cross-Tile RGB565 dither
ARGB8888 render target
XRGB8888 render target
```

These must be Tile-specific tests, not references to existing Immediate-only tests.

---

## J12 — Required checked Tile binary fixtures still do not exist

Severity:

> **CRITICAL — EQ-09**

`model/golden/tests/frames/tile/` still contains only `README.md`.

No checked replay artifacts exist for the required cases.

The README mentions:

```text
golden_cli generate-tile-fixtures
```

but there is still no checked implementation of that command.

### Required correction

Add at least:

```text
tile_fill_basic/
tile_alpha_overlap/
tile_bilinear_cross_boundary/
tile_palette/
tile_dither_cross_boundary/
```

Each fixture must contain real serialized TILE_FRAME / descriptors / headers / workrefs / assets and an Immediate-generated expected framebuffer.

Add CTest replay/compare tests and keep regeneration opt-in only.

---

# 5. Fault Verification Findings

## J13 — `golden_test_tile_faults` is useful but far from the required fault matrix

Severity:

> **HIGH**

The new fault test covers only a small set:

```text
Tile-size reconfiguration
Grid mismatch
Depth flag rejection
DONT_LOAD/default behavior
```

It does not yet prove the Stage-required exact faults for:

```text
Tile Header array out-of-bounds
WorkList range out-of-bounds
Descriptor index out-of-bounds
misaligned descriptor/header/work-list bases
unmapped descriptor base
unmapped header base
unmapped WorkRef base
strict target base mismatch
strict target stride mismatch
strict target format mismatch
RT_STATE Reserved strict/non-strict
TILE_FLAGS Reserved strict/non-strict
Tile Header W3 strict/non-strict
destination allocation boundary
```

### Required correction

Create a table-driven Tile fault matrix and assert one exact fault per case.

The current report's `H15 CLOSED` is too strong.

---

# 6. Profiler Findings

## J14 — PROF block remains incomplete and current counters are still semantically inaccurate

Severity:

> **CRITICAL**

The current outer Tile loop still performs logic equivalent to:

```text
blend_ops += 1 per WorkRef
pixels_written += valid_tile_area per WorkRef
```

Those are not pixel events.

For example, a 1×1 sprite in a 32×32 Tile can still account for a Tile-sized `pixels_written` increment.

Additionally, `tile_load_pixels/bytes` are incremented after the load/clear/default branch even when the Tile was cleared or DONT_LOAD initialized, so they can count a framebuffer load that did not occur.

Still missing required counters include:

```text
key_discards
clip_rejects
texture_samples
palette_reads
bilinear_samples
per-pixel overdraw
max overdraw
average overdraw
logical pixel/texture traffic
external Tile load/store traffic separation
```

### Required correction

Instrument actual shared pixel/sampler events.

A practical architecture is a nullable `GoldenPerfSink` / `PixelEventSink` passed into the shared backend, with Tile additionally owning per-pixel overdraw state.

Do not infer pixel counters from WorkRef count × Tile area.

---

# 7. Tile Sweep Findings

## J15 — Sweep now uses the functional model, but workload identity is wrong/incomplete

Severity:

> **HIGH**

The new C++ sweep tool is a real improvement, but its workloads do not match the names written by Python.

In the C++ tool:

```text
kind 1 → Straight-Alpha fills
kind 0 → ordinary fills
kind 2 → 128 ordinary fills
```

while Python labels:

```text
kind 0 → W3_alpha_storm
kind 1 → W1_sprite_grid
kind 2 → W2_high_overdraw
```

So at least W1 and W3 are mislabeled.

Furthermore Stage 004 called for six architecture workloads:

```text
W1 Sprite Grid
W2 High Overdraw Stack
W3 Alpha Storm
W4 Large Scaled Sprites
W5 Edge/Scatter Sprites
W6 Mixed Sprite Scene
```

The current functional sweep implements only three fill-based workload families. There is no scaled-sprite, edge-scatter, or mixed-texture workload.

### Required correction

Define each workload once, with explicit stable ID/name/seed/version shared by C++ output and Python.

Implement all six workloads as real draw streams.

---

## J16 — Current sweep summary overstates what the data supports

Severity:

> **MEDIUM/HIGH**

The current CSV shows all nine tested configurations have:

```text
tile_load_pixels = 4096
tile_store_pixels = 4096
```

meaning every surface pixel is loaded/stored for all current workloads.

Yet the summary concludes that small sprite grids demonstrate finer active-tile tracking and uses those results to support the 32×32 default.

Those claims are not strongly supported by the current three mislabeled, all-active workloads.

### Required correction

Regenerate the summary only after the six workload suite exists.

Include at least:

```text
active tile ratio
workref duplication
avg/max refs per active tile
load/store traffic
overdraw
```

and make conclusions traceable to actual CSV rows.

---

# 8. Acceptance-System Findings

## J17 — Acceptance checker was weakened to allow mandatory PARTIAL items

Severity:

> **CRITICAL — direct violation of TASK_004 Completion Contract**

The checker now accepts:

```python
status in ("PASS", "PARTIAL")
```

for mandatory items and merely prints a warning for `PARTIAL`.

It then prints:

```text
stage004 acceptance: PASS
```

when there are no other syntax errors.

This directly contradicts TASK_004:

> all mandatory acceptance IDs must be PASS before Stage PASS.

### Required correction

Mandatory `PARTIAL` must make the final Stage acceptance checker fail.

If a separate “progress checker” is useful, create a different mode:

```text
--allow-partial
```

but default/final acceptance must reject PARTIAL.

---

## J18 — Evidence entries remain generic placeholders

Severity:

> **HIGH — AUD-01**

Many manifest rows still contain evidence such as:

```text
model/golden implementation EQ-02
ctest build/stage004
golden_test_tile* / stage004 suite
```

These are not concrete file/function/test mappings.

The task explicitly required requirement-specific evidence.

### Required correction

For every ID, record actual paths and tests, e.g.:

```text
EQ-04
implementation:
  model/golden/src/tile_binner.cpp
  model/golden/src/golden_gpu.cpp
verification:
  model/golden/tests/tile/test_tile_extended.cpp
test_name:
  golden_test_tile_extended
```

Do not mark an ID PASS until its named verification exists.

---

## J19 — Acceptance checker still does not validate evidence

Severity:

> **HIGH — AUD-02**

The checker still does not verify:

- exact authoritative 47-ID set;
- duplicates;
- repository-relative evidence paths exist;
- named CTest tests exist;
- placeholder strings are rejected;
- REPORT_004 contains a real per-ID evidence row.

### Required correction

Implement the checker specified by TASK_004 rather than only checking non-empty fields.

---

## J20 — REPORT_004 still has no exact END_COMMIT and no 47-row evidence matrix

Severity:

> **MEDIUM/HIGH — AUD-03**

The report still says:

```text
END_COMMIT:
See git log -1
```

instead of recording the concrete SHA.

The report also lists the 47 IDs in one line rather than providing the required per-ID:

```text
Requirement
Implementation Evidence
Verification Evidence
Result
```

matrix.

The reviewed implementation commit is:

```text
bb810d35704f4e9e7ad3ff93624a0e282674bcd7
```

or a later documentation-only finalization commit should be recorded explicitly.

---

# 9. Test Result Evidence

REPORT_004 records:

```text
66/66 PASS
```

for the local CTest suite.

This review did not independently execute that local build, and the reviewed GitHub commit has no published commit-status checks.

Therefore:

> **66/66 is accepted as Agent-reported local test evidence, not independently reproduced CI evidence.**

This distinction should remain in the final report.

---

# 10. Gate Decision

Core architecture status:

```text
Tile command structures            good foundation
CPU software binner                good foundation
real load/render/store path        established
shared pixel backend               established
RT target authority                established
WorkRef order                      established
global dither origin               established
grid validation                    established
```

Still incomplete mandatory Stage-004 work:

```text
full Sprite Immediate==Tile matrix
mixed BLIT_EXT/Clip/Bilinear/Palette random
checked Tile fixtures
complete Tile fault matrix
true pixel/sampler/overdraw profiler
six-workload functional Tile sweep
strict Reserved semantics
real acceptance evidence/checker
exact report commit/evidence matrix
```

Therefore:

# **REVIEW_004_V4 = FAIL / CONTINUE REWORK**

Do not start Stage 005 RTL yet.

---

# 11. Recommended Final Rework Order

1. Make Tile scratch truly internal and remove hidden physical-address reservation.
2. Freeze LOAD_COLOR_DEFAULT / DONT_LOAD / CLEAR semantic matrix.
3. Resolve Strict Reserved semantics and complete the Tile fault matrix.
4. Add dedicated extended Immediate-vs-Tile directed tests.
5. Extend the 300-frame corpus with BLIT_EXT, Clip, Bilinear and Palette.
6. Add and replay the five required checked Tile fixtures.
7. Add true shared pixel/sampler profiler hooks + per-pixel overdraw.
8. Implement all six functional sweep workloads and regenerate CSV/summary.
9. Make mandatory PARTIAL fail the acceptance checker.
10. Replace generic manifest entries with real file/test evidence.
11. Generate the full 47-row report evidence matrix and exact END_COMMIT.

After these items are complete, Stage 004 should be ready for its final Gate review.
