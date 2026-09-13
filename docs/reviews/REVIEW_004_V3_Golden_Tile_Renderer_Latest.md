# REVIEW_004_V3 — Golden Tile Renderer Latest Rework Review

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Previous reviewed HEAD: `dab3608e4534c969233ec2ec1b57eec091707d6b`  
> Reviewed implementation commit / HEAD: `914867a296de71f8539bdf5f8f71baa13976e2d6`  
> Decision: **FAIL / CONTINUE REWORK**  
> Architecture redesign required: **NO**  
> Permission to start Stage 005 RTL: **NO**

---

# 1. Executive Summary

The latest Stage-004 rework closes several important issues from REVIEW_004_V2:

- the public `desc_count_hint` side channel is removed;
- repeated TILE_FRAME execution with the same Tile geometry is tested;
- `make_tile_frame_cmd()` now preserves RT_STATE bits exactly;
- explicit global dither origin offsets are added to the shared pixel path;
- the 300-frame random suite now includes FILL, Straight Alpha, Additive, BLIT and Color Key;
- a deterministic tile-sweep script is now checked in;
- the report is more honest and marks multiple areas PARTIAL rather than claiming everything is complete.

These are meaningful corrections.

However the Stage-004 completion contract still requires all mandatory IDs to be complete. The checked report itself still marks major requirements as PARTIAL:

```text
G9  Extended directed      PARTIAL
G11 Tile fixtures          PARTIAL
G12 Fault matrix           PARTIAL
G13 Profiler               PARTIAL
```

and several of those correspond directly to mandatory `EQ`, `TDS/TR`, and `PROF` acceptance IDs.

In addition, the acceptance manifest and checker were not repaired, and the new sweep script is an analytical placeholder rather than a sweep of the actual functional Tile model.

Therefore:

# **REVIEW_004_V3 = FAIL / CONTINUE REWORK**

Stage 005 remains blocked.

---

# 2. What Is Accepted

## H1 — `desc_count_hint` removed from formal API

`execute_tile_frame()` now derives descriptor capacity from the registered resource at `DRAW_DESC_BASE`.

**Status: CLOSED**

---

## H2 — RT_STATE wire encoding is now exact

`make_tile_frame_cmd()` now writes:

```text
W12 = rt_state
```

without automatically forcing `STORE_COLOR`.

The separate `tile_rt_state_store()` helper provides convenience without corrupting the low-level serializer.

**Status: CLOSED**

---

## H3 — Global dither origin is explicit

`Draw2DState` now carries:

```text
dither_ox
dither_oy
```

and the Tile path sets them to the global Tile origin before invoking the shared pixel path.

This is the correct direction for global Bayer indexing.

**Status: CLOSED at implementation level**

A dedicated cross-Tile Dither equivalence fixture/test is still required by EQ-04/EQ-09.

---

## H4 — Mixed random equivalence is improved

The 300-frame test now includes:

```text
FILL
Straight Alpha
Additive
RGB565 BLIT
Color Key BLIT
```

for Tile 16/32/64.

This is materially better than the previous FILL-only suite.

**Status: PARTIAL ACCEPT**

It still does not satisfy the full EQ-07 feature distribution.

---

## H5 — Repeated TILE_FRAME basic regression exists

A same-GoldenGPU two-frame regression is now present.

**Status: PARTIAL CLOSED**

It proves reuse for the same Tile size/format, but not resizing/reconfiguration safety.

---

# 3. Blocking Findings

## H6 — Internal scratch buffer still occupies an architectural GPU address

Severity:

> **HIGH — internal/external memory separation**

The Tile scratch target still uses fixed physical address:

```text
0x00800000
```

inside `MemoryImage` and is also registered in the Golden resource map.

The code now calls this “internal non-DDR”, but nothing prevents a legal user texture/resource from already occupying that address.

If a user resource exists at `0x00800000`, `has_region()` succeeds and Tile execution may treat that user-visible resource as internal scratch.

This can corrupt architectural memory and makes “internal” traffic indistinguishable from GPU-visible memory at the model level.

### Required correction

Preferred:

- use a separate internal Tile-buffer MemoryImage / storage object not addressable by GPU physical addresses.

Acceptable alternative:

- create an explicit internal-memory namespace/API that cannot collide with registered architectural resources.

Do not reserve an undocumented physical address silently.

---

## H7 — Scratch reuse does not handle Tile-size / format growth

Severity:

> **HIGH — multi-frame/reconfiguration correctness**

The first TILE_FRAME determines:

```text
scratch_size = tile_w × tile_h × BPP
```

The region is then reused whenever `0x800000` already exists.

If a later frame on the same GoldenGPU changes:

```text
Tile 16 RGB565
→ Tile 64 RGB565

or

Tile 32 RGB565
→ Tile 32 ARGB8888
```

the existing scratch allocation/resource metadata is not resized.

The current repeat-frame test uses the same 32×32 RGB565 configuration and therefore does not catch this.

### Required correction

Add at least:

```text
same GPU:
Tile16 → Tile32
Tile32 RGB565 → Tile32 ARGB8888
```

or formally forbid run-time Tile-format/size changes and enforce that restriction.

Given TILE_FRAME carries these fields per command, supporting/revalidating changes is preferable in Golden.

---

## H8 — `LOAD_COLOR_DEFAULT` is decoded but still not implemented

Severity:

> **HIGH — encoded architectural state ignored**

`decode_rt_state()` exposes:

```text
load_color_default
```

but `execute_tile_frame()` does not use it.

The DONT_LOAD branch simply zero-fills the Tile and comments that this represents a Stage-004 default.

That is not equivalent to implementing `LOAD_COLOR_DEFAULT`, and no exact semantic test establishes the intended state matrix.

### Required correction

Resolve from the frozen architecture:

```text
LOAD_COLOR_DEFAULT=0/1
TILE_DONT_LOAD_COLOR=0/1
TILE_CLEAR_COLOR=0/1
```

Then either implement the supported combinations exactly or reject unsupported combinations.

Do not mark G3 CLOSED until this state matrix has evidence.

---

## H9 — TILE_FLAGS unsupported/reserved states are still not validated

Severity:

> **HIGH**

The Tile Header defines:

```text
TILE_DONT_LOAD_COLOR
TILE_CLEAR_COLOR
TILE_LOAD_DEPTH
TILE_CLEAR_DEPTH
Reserved[31:4]
```

but the executor only consumes DONT_LOAD and CLEAR_COLOR.

There is no complete handling/rejection for:

- `TILE_LOAD_DEPTH`;
- `TILE_CLEAR_DEPTH`;
- Reserved TILE_FLAGS bits.

Depth being out of scope is fine, but enabled depth Tile flags must fail deterministically rather than be ignored.

---

## H10 — Strict/Non-Strict Reserved semantics are still not demonstrated

Severity:

> **MEDIUM/HIGH**

Current code rejects:

```text
RT_STATE[31:9] != 0
Tile Header W3 != 0
```

unconditionally.

The ISA's general Reserved policy is tied to Strict mode.

The Stage task explicitly asked for exact strict/non-strict semantics and tests.

### Required correction

Add paired:

```text
H_STRICT=0
H_STRICT=1
```

tests for Tile reserved fields and align behavior with the authoritative ISA.

If the project decides Tile safety rules are intentionally stricter than generic Reserved behavior, document this through an approved design/spec decision first.

---

## H11 — Grid configuration validation remains incomplete

Severity:

> **HIGH — TILE_FRAME structure validation**

`execute_tile_frame()` checks only that Tile/Grid dimensions are nonzero.

It does not require:

```text
grid_w == ceil(surface_w / tile_w)
grid_h == ceil(surface_h / tile_h)
```

or another explicit allowed relationship.

A too-small grid can silently leave part of the surface unprocessed; an oversized grid can consume extra Tile Headers.

### Required correction

Define and validate the frozen V0.1 relationship and add exact BAD_TILE_CONFIG tests.

---

# 4. Immediate-vs-Tile Verification Findings

## H12 — EQ-07 mixed random still does not meet the Stage-004 contract

Severity:

> **CRITICAL — mandatory acceptance incomplete**

The improved random generator covers:

```text
FILL
Straight Alpha
Additive
BLIT
Color Key
```

It still lacks required random presence of:

```text
BLIT_EXT nearest
Clip
some Bilinear
some Palette
```

The Stage task explicitly listed these feature classes for EQ-07.

### Required correction

Add deterministic resource setup for:

- extension blocks;
- ARGB/INDEX8 textures;
- palette memory.

Ensure all required feature classes occur in the fixed-seed corpus.

---

## H13 — Extended directed Tile equivalence remains incomplete

Severity:

> **CRITICAL — report itself marks G9 PARTIAL**

The Stage still needs dedicated Immediate-vs-Tile evidence for:

```text
ARGB8888 source
XRGB8888 source
Color Mod
Global Alpha
Pixel Alpha
Premult Alpha
Nearest BLIT_EXT
Bilinear BLIT_EXT
Clamp
Repeat
Clip
Indexed8 + Palette
RGB565 Dither crossing Tile boundary
ARGB8888 target
XRGB8888 target
```

Current Tile-specific tests do not establish the complete `EQ-02..EQ-05` matrix.

The report itself acknowledges Bilinear/Palette are pending.

---

## H14 — Required Tile binary fixtures are still absent

Severity:

> **CRITICAL — EQ-09 incomplete**

Only:

```text
model/golden/tests/frames/tile/README.md
```

was added.

The README claims fixtures can be generated by:

```text
golden_cli generate-tile-fixtures
```

but no such generator is present in the repository.

Required checked fixtures remain absent:

```text
tile_fill_basic
tile_alpha_overlap
tile_bilinear_cross_boundary
tile_palette
tile_dither_cross_boundary
```

### Required correction

Implement the generator/replay path and check in the actual fixture directories.

Register them in CTest.

---

## H15 — Complete Tile fault matrix still missing

Severity:

> **HIGH — report marks G12 PARTIAL**

Required exact fault cases still need a dedicated suite for:

```text
Tile Header array bounds
WorkList bounds
Descriptor bounds
misaligned required bases
unmapped header/workref/descriptor
grid mismatch
bad RT state
reserved Tile flags
reserved Tile Header
strict target base mismatch
strict target stride mismatch
strict target format mismatch
unsupported depth state
```

Target-format mismatch alone is insufficient.

---

# 5. Profiler Findings

## H16 — PROF block remains incomplete

Severity:

> **CRITICAL — report marks G13 PARTIAL**

The current `TileStats` does not satisfy the required profiler contract.

Still absent include:

```text
key_discards
clip_rejects
texture_samples
palette_reads
bilinear_samples
per-pixel overdraw
max_overdraw
average_overdraw
logical traffic
estimated/external Tile traffic separation
```

Additionally:

```text
blend_ops
pixels_written
```

are still updated at WorkRef/Tile granularity rather than true pixel events.

Therefore they cannot be used as trustworthy architecture/performance counters.

### Required correction

Add profiler hooks to the shared Pixel/Sampler path and pass a profiler context through Immediate/Tile execution.

Tile-specific load/store counters should count actual Tile transfer events.

Do not approximate pixel counters in the outer WorkRef loop.

---

# 6. Tile Sweep Findings

## H17 — `run_tile_sweep.py` is reproducible, but it does not use the functional Tile model

Severity:

> **HIGH — EXP-01/02 intent not met**

A script now exists, so the old “untraceable CSV” problem is improved.

However the script computes metrics from hand-written analytical formulas:

```text
span
active
workrefs
load_store
```

based only on draw counts and Tile size.

It does **not**:

- construct the fixed draw workloads;
- call the software binner;
- execute or inspect the Tile functional model;
- obtain metrics from `TileStats`.

TASK_004 explicitly requires using:

> the functional Tile model and fixed workloads.

### Required correction

Create actual deterministic workload command streams and run them through:

```text
bin_draws()
execute_tile_frame()
last_tile_stats()
```

or expose an executable that Python invokes.

The CSV must come from real model statistics, not substitute formulas.

---

# 7. Acceptance-System Findings

## H18 — Acceptance manifest is still invalid as evidence

Severity:

> **CRITICAL — AUD-01 still fails**

The manifest was not updated in this rework and still marks every mandatory ID PASS.

Many items continue to use generic strings such as:

```text
model/golden tile+golden implementation
ctest stage004
```

even for requirements the report itself marks PARTIAL.

This directly contradicts the task's completion contract.

### Required correction

Set incomplete IDs to:

```text
PARTIAL / TODO
```

until actually complete.

Every PASS item must point to concrete repository files/functions and concrete tests.

---

## H19 — Acceptance checker is unchanged and still too weak

Severity:

> **CRITICAL — AUD-02 fails**

The checker still verifies only:

```text
status == PASS
evidence list non-empty
test_name non-empty
ID text appears in report
```

It still does not validate:

- the exact authoritative 47-ID set;
- duplicate IDs;
- evidence file paths;
- named CTest tests;
- placeholder/generic evidence;
- report Evidence Matrix rows.

Therefore it can continue to certify false completeness.

---

## H20 — REPORT_004 still lacks exact END_COMMIT and full evidence matrix

Severity:

> **HIGH — AUD-03 / report contract**

The current report says:

```text
END_COMMIT:
See git log -1 after this report is committed
```

rather than recording:

```text
914867a296de71f8539bdf5f8f71baa13976e2d6
```

or a later report-finalization commit.

It also lists acceptance IDs rather than providing the required 47-row:

```text
ID
Requirement
Implementation Evidence
Verification Evidence
Result
```

matrix.

---

# 8. Current Assessment

The Stage-004 architecture is now substantially healthier:

```text
CPU software binning
→ serialized Tile structures
→ Tile scratch load
→ shared Pixel Backend
→ Tile-local quantized writes
→ Tile store
```

That core direction is accepted.

But Stage 004 is explicitly not complete yet because mandatory verification, profiling, fixture, experiment, and evidence-system requirements remain unfinished.

The checked report itself correctly admits several of these are PARTIAL.

Therefore the formal result is:

# **REVIEW_004_V3 = FAIL / CONTINUE REWORK**

Do not start RTL.

---

# 9. Recommended Final Closure Order

1. Replace fixed GPU-visible scratch with collision-free internal Tile storage; support resize/reconfiguration.
2. Finish RT_STATE / TILE_FLAGS state matrix and Strict semantics.
3. Validate Grid/Surface/Tile relationships and complete Tile fault suite.
4. Finish directed Immediate-vs-Tile feature matrix.
5. Add BLIT_EXT/Clip/Bilinear/Palette to 300-frame random corpus.
6. Implement checked Tile binary fixtures and fixture replay.
7. Move profiler counting to true pixel/sampler events and add overdraw.
8. Rework Tile sweep to drive the actual functional binner/renderer.
9. Rewrite the 47-ID acceptance manifest with concrete evidence and honest status.
10. Strengthen the acceptance checker.
11. Produce the full evidence matrix and exact END_COMMIT.

Once these are complete, Stage 004 should be ready for a final review and likely transition into Stage 005 RTL.
