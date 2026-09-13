# REVIEW_004 — Golden Tile Renderer, Software Binning & Immediate-vs-Tile Pixel-Exact

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Stage baseline: `105c23c0989c74fcd6273b33181c6079e40f5d5e`  
> Stage implementation commit: `7752f059006e5d388420c9dd5dbb208698cb6507`  
> Reviewed HEAD: `d71915e233e090be1292cd48bd017963c8ced1ae`  
> Decision: **FAIL / REWORK**  
> Architecture redesign required: **NO**  
> Permission to start Stage 005 RTL: **NO**

## 1. Executive Summary

Stage 004 contains useful first-step Tile work: Tile Header / WorkRef structures, a CPU software binner, serialized descriptor/header/workref replay, row-major tile traversal, and a shared Immediate pixel path.

However, the core Stage-004 milestone is not yet implemented.

The current `execute_tile_frame()` does not contain a real Tile Buffer. It performs per-tile replay of Draw Descriptors and calls the Immediate renderer clipped to the current tile rectangle, writing directly to the framebuffer after every logical draw.

Therefore the current equivalence result proves roughly:

```text
Immediate renderer
==
Immediate renderer replayed per tile with an extra scissor
```

It does not yet prove the intended Tile-Based load/render/store memory architecture.

Several mandatory acceptance blocks were also marked PASS with generic evidence despite being absent or only partial.

Overall decision:

# **FAIL / REWORK**

Keep Stage 004 open and do not start Stage 005.

## 2. Block Status

| Block | Status |
|---|---|
| Block 0 — Golden Verification Finalization | PARTIAL / FAIL |
| Block 1 — Tile Binary Structures | PARTIAL |
| Block 2 — Software Tile Binner | PARTIAL PASS |
| Block 3 — Golden Tile Renderer | FAIL |
| Block 4 — Immediate-vs-Tile Gate | FAIL |
| Block 5 — Tile Statistics | FAIL |
| Block 6 — Tile Size Sweep | FAIL / UNTRACEABLE |
| Block 7 — Acceptance Self-Audit | FAIL |

## 3. Critical Findings

### F1 — No real Tile Buffer

The implementation does not perform:

```text
Framebuffer → Tile Buffer load
Tile-local destination reads/writes
per-logical-write format quantization inside Tile state
Tile Buffer → Framebuffer store
```

Instead it calls `GoldenGPU::execute_decoded()` for every WorkRef with an extra tile clip and writes directly to DDR-backed framebuffer state.

Required: implement a Tile Render Target adapter / Tile Color Buffer. Load each active tile once, render WorkRefs into tile-local state, then store the valid tile once.

### F2 — TILE_FRAME render-target authority is wrong

The ISA freezes TILE_FRAME `DST_BASE / DST_STRIDE / DST_FORMAT` as authoritative.

Current code overrides descriptor base/stride but keeps each Draw Descriptor's destination format.

`STRICT_TARGET_MATCH` is also not implemented.

Required: TILE_FRAME target format/base/stride must drive Tile rendering. With `STRICT_TARGET_MATCH=1`, mismatching descriptor target fields must return `TILE_TARGET_MISMATCH`.

### F3 — TILE_FLAGS / RT_STATE are silently ignored

`LOAD_COLOR_DEFAULT`, `STORE_COLOR`, `STRICT_TARGET_MATCH`, Tile clear/load flags and unsupported depth state are not behaviorally enforced.

Unsupported Stage-004 states must either be implemented or deterministically rejected. They must not be silently ignored.

### F4 — TILE_FRAME wire representation is incomplete

The frozen TILE_FRAME includes W14 `DEPTH_BASE` and W15 `DEPTH_STRIDE`, but `TileFrameCmd` does not preserve them.

Depth need not be rendered in Stage 004, but the exact wire representation should still be modeled and unsupported enabled depth state rejected.

Tile Header W3 Reserved also needs the required validation.

### F5 — Formal Tile path uses host-only `desc_count_hint`

Descriptor bounds depend on an out-of-band `desc_count_hint` that is not part of TILE_FRAME.

Formal replay should derive descriptor-array bounds from registered memory/resource metadata, not a semantic host argument.

### F6 — Only two Tile test executables were added

The whole Tile stage adds essentially:

```text
golden_test_tile
golden_test_tile_eq_random
```

A green total CTest count mostly consists of old Golden tests and does not demonstrate all Stage-004 acceptance IDs.

### F7 — The 300-frame random equivalence is FILL-only

The random generator creates only FILL_RECT commands with optional Straight Alpha.

It does not randomize the required Stage-004 feature distribution:

```text
BLIT
BLIT_EXT
Additive
Color Key
Clip
Bilinear
Palette
textures
```

So `EQ-07` is not satisfied despite the 300-frame count.

### F8 — Order-sensitive test does not test WorkRef order

The current order-sensitive test compares Immediate `[A,B]` against Immediate `[B,A]`.

It does not mutate serialized Tile WorkRefs and execute TILE_FRAME.

Required: serialize `[A,B,C]`, verify Tile output, then reverse WorkRefs and prove Tile output changes.

### F9 — Extended Tile equivalence is mostly absent

The Stage required Tile equivalence for BLIT, formats, key/mod/alpha/premult/additive, scaling, bilinear, address modes, Clip, Palette and Dither.

Dedicated Tile tests mostly cover FILL and Straight-Alpha FILL overlap.

`EQ-02` through `EQ-05` are therefore not established.

### F10 — Required Tile binary fixtures are absent

No checked fixtures were added for required cases such as:

```text
tile_fill_basic
tile_alpha_overlap
tile_bilinear_cross_boundary
tile_palette
tile_dither_cross_boundary
```

`EQ-09` is not satisfied.

### F11 — Tile fault matrix is incomplete

Required exact faults for malformed header/work-list/descriptor/pointer/target-match states are not covered by a dedicated complete Tile fault suite.

### F12 — TileStats is incomplete

Current counters include only basic tile/workref and byte counts.

Missing required metrics include:

```text
tile_load/store_pixels
avg refs/active tile
pixels attempted/written
key discards
clip rejects
blend operations
texture samples
palette reads
bilinear samples
per-pixel overdraw
max/average overdraw
logical vs estimated physical traffic
```

### F13 — Tile load/store byte counters do not correspond to actual Tile transactions

The renderer has no Tile load/store implementation, but increments one-load/one-store byte estimates per active tile.

These values are analytical estimates, not events from the functional Tile path.

### F14 — Tile-size sweep is not reproducibly generated

The repository contains CSV/summary result files, but no Stage-004 sweep implementation / deterministic workload generator was added.

There is no repository path that demonstrates how the CSV was produced from the functional model.

### F15 — Acceptance manifest uses generic placeholder-like evidence

Most IDs use generic evidence such as:

```text
model/golden: Stage-004 implementation
ctest build/stage004
golden_test_tile / golden_test_tile_eq_random / full ctest
```

for unrelated requirements.

This violates the task rule that every acceptance ID must map to concrete implementation and verification evidence.

### F16 — Acceptance checker is too weak

The checker only verifies that:

```text
status == PASS
evidence fields are non-empty
test_name is non-empty
ID string appears in report
```

It does not verify evidence paths exist, named tests exist, all authoritative IDs are present exactly once, or evidence is requirement-specific.

This allowed the generic manifest to pass.

### F17 — REPORT_004 does not contain the required 47-row evidence matrix

The report lists all IDs but does not provide per-ID:

```text
Requirement
Implementation Evidence
Verification Evidence
Result
```

It also leaves END_COMMIT as “see git log” rather than an exact SHA.

## 4. Block-0 Status

`GVF-01`: PASS — integrity checker now detects member-expression tautologies.

`GVF-02`: PASS — scale oracle now uses local formulas and real padded source stride.

`GVF-03`: NOT CLOSED — the Stage-004 diff did not update the previously weak proof/oracle tests.

`GVF-04`: NOT CLOSED — the destination-format/dither file identified previously was not updated in Stage 004.

`GVF-05`: PARTIAL — extension/memory matrix improved, but complete required coverage is still absent.

`GVF-06`: FAIL — report still lacks exact END_COMMIT.

Therefore Block 0 was not actually green before Tile implementation began.

## 5. What Should Be Kept

Retain:

```text
TileHeader / WorkRef basic types
row-major tile IDs
software binner core
stable WorkRef append order
serialized Draw Descriptor / Header / WorkRef memory path
shared Immediate pixel backend
tile-rectangle extra clip mechanism
16/32/64 parameterization
```

The missing architectural layer is a real Tile Render Target / Tile Buffer.

## 6. Rework Order

Continue TASK_004 in this order:

1. actually close GVF-03..06;
2. implement exact TILE_FRAME RT_STATE and target authority;
3. complete Tile wire fields and state/flag validation;
4. remove `desc_count_hint` from formal semantics;
5. implement real Tile Color Buffer and RT adapter;
6. implement real tile load / logical writes / tile store;
7. add strict target-match and Tile fault matrix;
8. build extended directed Immediate-vs-Tile tests;
9. replace FILL-only random workload with mixed feature streams;
10. add required Tile binary fixtures;
11. implement full Tile/overdraw/traffic instrumentation;
12. add a reproducible Tile sweep generator and fixed workloads;
13. rebuild acceptance manifest with specific per-ID evidence;
14. strengthen acceptance checker;
15. write a real 47-row evidence matrix in REPORT_004.

## 7. Re-submission Gate

Do not re-submit Stage 004 merely because the CTest number increases.

The next review must be able to verify from repository code that:

```text
actual Tile Buffer exists
Tile load/render/store is real
TILE_FRAME target state is authoritative
STRICT_TARGET_MATCH works
WorkRef order is verified through Tile execution
extended features cross tile boundaries exactly
mixed-feature 300-frame equivalence passes
Tile fixtures exist
profiling/overdraw counters exist
sweep is reproducible
47 acceptance IDs have concrete evidence
acceptance checker rejects generic fake evidence
```

## 8. Decision

# **REVIEW_004 = FAIL / REWORK**

Stage 005 RTL remains blocked.

The current implementation is a useful:

> software binner + per-tile Immediate replay prototype

but not yet the planned:

> Tile-Based Golden Renderer and Tile memory-architecture model.
