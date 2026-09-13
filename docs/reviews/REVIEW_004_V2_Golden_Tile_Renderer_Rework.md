# REVIEW_004_V2 — Golden Tile Renderer Rework Review

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Previous reviewed HEAD: `d71915e233e090be1292cd48bd017963c8ced1ae`  
> Rework implementation commit: `3cd45815bf626e8ff287bfa11de3b7d3b40b4334`  
> Reviewed HEAD: `dab3608e4534c969233ec2ec1b57eec091707d6b`  
> Decision: **FAIL / CONTINUE REWORK**  
> Architecture redesign required: **NO**  
> Permission to start Stage 005 RTL: **NO**

## 1. Executive Summary

The latest rework is a meaningful improvement. The previous review's largest architectural defect — no Tile-local render target — has been partially corrected. `execute_tile_frame()` now creates a scratch render-target region, copies each active tile from framebuffer into that region, retargets Draw Descriptors to the scratch surface, executes the shared Golden pixel path, and stores the valid tile back to framebuffer.

The rework also adds TILE_FRAME depth fields W14/W15, RT_STATE decoding, target format override, STRICT_TARGET_MATCH checking, actual serialized WorkRef-order testing through TILE_FRAME, and additional Tile statistics fields. These changes should be retained.

However, Stage 004 still does **not** satisfy its mandatory acceptance contract. The checked report itself marks major Stage-004 requirements as `PARTIAL`, while still declaring the Stage `PASS`. The acceptance manifest continues to mark all IDs PASS using generic evidence, and the acceptance checker remains too weak to detect this contradiction.

Several functional Tile issues also remain, including repeat-frame failure due to scratch-region registration, incomplete RT_STATE/TILE_FLAGS semantics, continued host-only `desc_count_hint`, missing extended feature equivalence, missing Tile fixtures, incomplete profiler semantics, and unreproducible tile-size sweep data.

Therefore:

# **REVIEW_004_V2 = FAIL / CONTINUE REWORK**

Do not start Stage 005.

## 2. What Is Now Accepted

### A1 — Tile-local scratch render target exists

The latest path is no longer simple per-tile replay directly into the final framebuffer. It now performs framebuffer pixel bytes → scratch tile target → shared Golden draw/pixel path → scratch tile target → framebuffer pixel bytes.

**Status: ACCEPTED as architecture foundation.**

### A2 — Target format override added

The draw descriptor is retargeted to the Tile scratch target and its destination format is overridden by the TILE_FRAME RT format.

**Status: ACCEPTED, subject to remaining RT_STATE issues below.**

### A3 — STRICT_TARGET_MATCH has a real Tile-path test

The new Tile test checks a Draw Descriptor / TILE_FRAME target-format mismatch and expects `TILE_TARGET_MISMATCH`.

**Status: ACCEPTED for the covered mismatch case.**

### A4 — WorkRef order is now tested through TILE_FRAME

The new test actually reverses serialized Tile WorkRefs and executes the Tile path, rather than comparing two Immediate sequences.

**Status: ACCEPTED.**

### A5 — W14/W15 modeled

`DEPTH_BASE` and `DEPTH_STRIDE` now exist in `TileFrameCmd`, and depth-enabled states are rejected as unsupported.

**Status: ACCEPTED for Stage-004 no-depth scope.**

## 3. Blocking Functional Findings

### G1 — A second TILE_FRAME on the same GoldenGPU instance fails

Severity: **CRITICAL — multi-frame architectural behavior**

`execute_tile_frame()` registers a fixed internal scratch MemoryImage region at `0x00800000` on every call. `MemoryImage::register_region()` rejects overlapping ranges. Therefore a second TILE_FRAME on the same GoldenGPU will hit the existing scratch range and fail unless the GPU/MemoryImage is reset between frames.

Required: create/reuse internal Tile scratch once, resize safely, or move scratch outside architectural MemoryImage. Add a same-GPU two-frame regression.

### G2 — `TileColorBuffer` exists but is not the actual Tile execution state

Severity: **MEDIUM — architecture/evidence mismatch**

A `TileColorBuffer` class was added, but Tile execution actually uses a GPU-visible `MemoryImage` scratch region. That can be acceptable as an implementation mechanism, but the report/evidence must describe what is really used, and internal scratch traffic must not be confused with external DDR traffic.

Either adopt a dedicated internal Tile buffer abstraction or explicitly define the scratch region as internal/non-DDR state and remove dead code.

### G3 — `LOAD_COLOR_DEFAULT` and `TILE_DONT_LOAD_COLOR` remain ignored

Severity: **HIGH — encoded state silently ignored**

`RT_STATE.LOAD_COLOR_DEFAULT` is decoded but not consumed; `TILE_DONT_LOAD_COLOR` is defined but not used in execution. Current behavior is effectively `CLEAR_COLOR → clear`, otherwise `always load framebuffer`.

Implement exact semantics or explicitly reject unsupported non-default combinations. Do not silently accept ineffective state bits.

### G4 — `make_tile_frame_cmd()` modifies RT_STATE semantics

Severity: **HIGH — wire-encoding authority**

The helper automatically sets `STORE_COLOR` when no upper RT_STATE bits are set. This prevents faithful encoding of a requested RT_STATE whose `STORE_COLOR=0` and other control bits are zero.

The ISA serializer must preserve `rt_state` exactly. Convenience defaults belong in a higher-level helper, not the wire encoder.

### G5 — RT_STATE and TILE_FLAGS Reserved bits are not fully validated

Severity: **HIGH — Strict semantics**

The ISA reserves `RT_STATE[31:9]` and `TILE_FLAGS[31:4]`. The current path does not establish a complete Strict/non-Strict rule for them. Tile Header W3 is rejected unconditionally while other command Reserved behavior is Strict-sensitive.

Add exact `H_STRICT=0/1` tests and align behavior with the frozen ISA.

### G6 — Host-only `desc_count_hint` still exists and is still used

Severity: **HIGH — formal architectural path**

The public API still allows `execute_tile_frame(..., desc_count_hint)` and an existing Tile test still passes a nonzero value. The default path can derive capacity from registered descriptor memory, but the semantic side channel remains.

Remove it from the formal Tile execution API/tests. Bounds must come from GPU-visible registered memory.

### G7 — TILE_FRAME validation is still incomplete

Severity: **HIGH**

Still missing or insufficiently demonstrated: grid/surface/tile consistency, stride vs width×BPP, destination allocation coverage, tile-header coverage, work-list arithmetic overflow, unsupported Tile flags, and Reserved RT state.

## 4. Pixel-Exact / Verification Findings

### G8 — Random Tile equivalence is still FILL/Alpha-heavy

Severity: **CRITICAL — EQ-07 remains incomplete**

The rework did not replace the prior random generator. The 300-frame suite still primarily creates FILL_RECT with optional Straight Alpha. It does not satisfy the required mixed feature distribution covering BLIT, BLIT_EXT, Additive, Color Key, Clip, Bilinear, Palette and textures.

### G9 — Extended directed Tile equivalence is still largely missing

Severity: **CRITICAL**

The report itself says full BLIT/Bilinear/Palette Tile equivalence remains pending. Mandatory acceptance IDs covering BLIT/formats/key/mod/alpha/premult/additive/scaling/bilinear/address modes/Clip/Palette/Dither therefore cannot all be PASS.

### G10 — Global-coordinate Dither semantics are not explicit

Severity: **HIGH — Tile semantic boundary**

Tile execution translates destination and Clip into Tile-local coordinates and invokes the shared Golden pixel path. The pixel path feeds local `dx/dy` to dithering. With 16/32/64 tiles the Bayer 4×4 phase happens to align because tile origins are multiples of four, but this is accidental rather than an explicit global-coordinate contract.

Make global RT coordinates explicit in the RT write path and add a dither case crossing a tile boundary.

### G11 — No checked Tile binary fixtures yet

Severity: **HIGH — EQ-09 still incomplete**

The required checked fixtures such as `tile_fill_basic`, `tile_alpha_overlap`, `tile_bilinear_cross_boundary`, `tile_palette`, and `tile_dither_cross_boundary` are still absent.

### G12 — No complete Tile fault suite

Severity: **HIGH**

`tile_ext` adds target-match and WorkRef-order coverage, but there is still no complete dedicated fault matrix for header/work-list/descriptor bounds, misalignment, unmapped resources, bad RT state, target mismatch and Reserved fields with exact expected faults.

## 5. Statistics / Architecture Experiment Findings

### G13 — Tile profiler is still incomplete and some counters are semantically wrong

Severity: **HIGH**

New counters exist, but `blend_ops` is incremented once per WorkRef/descriptor and `pixels_written` is increased by an entire valid tile area for every WorkRef, regardless of actual draw coverage, Clip, Color Key or blend behavior.

These are not valid pixel workload counters.

Still missing key required metrics such as key discards, clip rejects, texture samples, palette reads, bilinear samples, per-pixel overdraw, max/average overdraw and logical-vs-external traffic separation.

Instrumentation must be attached to real shared pixel/sampler events.

### G14 — Tile-size sweep remains non-reproducible

Severity: **HIGH — EXP block still fails**

`model/architecture/tile_model/` still contains only `.gitkeep`. CSV/summary files exist, but no deterministic generator/workload definitions reproduce them from repository code.

## 6. Acceptance / Report Findings

### G15 — REPORT_004 contradicts its own PASS claim

Severity: **CRITICAL — completion contract violation**

The report declares `PASS` while explicitly marking F6–F7, F9–F10 and F11–F14 as `PARTIAL`. TASK_004 says every mandatory acceptance ID must be complete before PASS. The report therefore proves the stage is not complete.

### G16 — Acceptance Manifest is still generic for many IDs

Severity: **CRITICAL — AUD-01 still not satisfied**

Many IDs still use evidence such as `model/golden tile+golden implementation` and `ctest stage004` rather than concrete implementation/test paths.

### G17 — Acceptance checker was not strengthened

Severity: **CRITICAL — AUD-02 still not satisfied**

The checker still only validates `status == PASS`, non-empty evidence fields/test name, and presence of each ID string in the report. It does not validate the authoritative 47-ID set exactly, evidence paths, actual CTest names, duplicate/missing IDs, or generic placeholder evidence.

### G18 — REPORT_004 still has no exact END_COMMIT

Severity: **LOW but repeated**

The report still says `See git log -1` instead of recording the exact SHA. Reviewed HEAD is `dab3608e4534c969233ec2ec1b57eec091707d6b`.

## 7. Current Gate Assessment

Meaningful progress:

```text
Software binner                  good foundation
serialized WorkRefs              good foundation
scratch Tile target              now exists
RT format authority              improved
strict target mismatch           implemented/tested
WorkRef order                    now tested through Tile
W14/W15                          modeled
```

Still blocking Stage 005:

```text
repeat TILE_FRAME correctness
full Tile state semantics
formal removal of host side-channel
mixed-feature Tile equivalence
Tile fixtures
Tile fault matrix
valid profiler/overdraw
reproducible architecture sweep
real per-ID evidence system
```

Therefore:

# **REVIEW_004_V2 = FAIL / CONTINUE REWORK**

## 8. Recommended Final Rework Order

1. Fix reusable internal Tile scratch storage / repeated TILE_FRAME.
2. Preserve exact RT_STATE bits; remove serializer auto-default.
3. Implement or reject LOAD_COLOR_DEFAULT / DONT_LOAD / remaining flags.
4. Complete Strict Reserved-state validation.
5. Remove `desc_count_hint` from formal API/tests.
6. Complete Tile fault matrix.
7. Build directed Tile equivalence for every major Sprite semantic.
8. Upgrade 300-frame random test to mixed features.
9. Add Tile binary fixtures.
10. Move profiler instrumentation to real pixel/sampler events and add overdraw.
11. Add reproducible tile-size sweep tool/workloads.
12. Replace generic acceptance evidence with per-ID paths/tests.
13. Strengthen `check_stage004_acceptance.py`.
14. Write a real 47-row report matrix and exact END_COMMIT.

Only then should Stage 005 RTL begin.
