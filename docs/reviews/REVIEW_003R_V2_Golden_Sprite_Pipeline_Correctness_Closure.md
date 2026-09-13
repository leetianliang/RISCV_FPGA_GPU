# REVIEW_003R_V2 — Golden Sprite Pipeline Correctness Closure

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Previous 003R checkpoint: `4a6b28e717f8d9547310d1ad87c82c281e24fe26`  
> Rework implementation commit: `470d7b365343bc42a2987d874152c963840549e6`  
> Report follow-up commit: `362a3e7cf415f93355c2af9de50f543b3e0e193f`  
> Decision: **FAIL / CONTINUE 003R**  
> Architecture redesign required: **NO**  
> Permission to start TASK_004 Tile Renderer: **NO**

## 1. Executive Summary

The second 003R rework closes several concrete implementation defects:

- Q16 origin conversion now uses wide arithmetic and rejects unrepresentable fast-BLIT origins.
- SurfaceView validation now uses the registered linear-resource width and 64-bit row/address arithmetic.
- BLIT_EXT Flip flags now have an explicit contract: reject the flags; encode flip in UV coefficients.
- CLIP enable/disable pairing has a directed test.
- FILL + extension Clip has a directed test.
- Premultiplied Alpha has a much stronger exact directed matrix.
- Exact directed tests were added for selected Bilinear fractions, Clamp/Repeat, Bayer dither and nearest Indexed8 palette lookup.
- The regression matrix is now updated.
- The report records a concrete implementation commit.

However, `TASK_003R` is still not complete enough to pass the Golden-correctness gate. The remaining gap is now concentrated mainly in **verification completeness and oracle independence**.

The most important finding is that the newly added `golden_test_diff_suites` is called a differential suite, but its Alpha/Key path does not compute or compare an expected framebuffer; it only checks that execution succeeds.

Therefore:

# **REVIEW_003R_V2 = FAIL / CONTINUE 003R**

Do not start Tile yet.

## 2. Accepted Corrections

### Q16 / SRC_X boundary

The decoder now converts integer source origins through an `i64` intermediate and rejects values that cannot fit signed Q16.16.

**Status: CLOSED**

### Linear SurfaceView rule

`validate_view()` now uses the registered resource width, `u64` row-byte/address calculations, and checks the final addressed byte against both 32-bit physical space and the registered allocation.

**Status: CLOSED for the current model**

### BLIT_EXT Flip contract

BLIT_EXT now rejects `FLIP_X/Y`; reversal must be encoded in UV coefficients.

**Status: CLOSED**

### Clip/ext pairing

A directed test now proves the same extension behaves unclipped with `CLIP_EN=0` and clipped with `CLIP_EN=1`.

**Status: CLOSED**

### FILL + extension Clip

A directed positive test now exists.

**Status: CLOSED for the positive path**

### Premult matrix

The new matrix covers transparent/opaque source, Color-Mod alpha/RGB, Global Alpha, and Mod×Global.

**Status: MOSTLY CLOSED**

The requested Straight-vs-Premult equivalence vector is still absent.

### Exact directed feature oracles

New exact tests cover selected Bilinear fractions, source-rectangle-relative Clamp/Repeat, a 4×4 Bayer pattern, and nearest Indexed8 palette lookup.

**Status: ACCEPTED as partial coverage**

## 3. Blocking Findings

### E1 — Core Alpha/Key random path is not differential

`run_core_alpha_key()` randomizes state and checks only `execute_command(...).ok`; it never computes an independent expected framebuffer and never compares pixels.

Therefore it does not prove correctness for Straight Alpha, Global/Pixel Alpha, Color Key, Additive, or RGB565 per-write quantization.

**Required:** implement an independent Stage-002 pixel oracle and exact framebuffer comparison.

### E2 — Nearest-scaling differential is too narrow

The scale differential is effectively fixed to 4×4 → 8×8 RGB565 COPY with source origin 0 and tight strides.

**Required:** fixed-seed parameterized cases covering 1→N, N→1, up/downscale, non-integer ratios, odd sizes, nonzero SRC_X/Y, and padded strides.

### E3 — Clip differential/random suite is missing

The paired directed Clip test is useful, but there is still no independent randomized exact suite for empty/full/partial Clip, negative destination, scaled Clip, and UV-origin preservation.

### E4 — Indexed8 Bilinear oracle is missing

Nearest palette lookup is proven, but not:

```text
fetch 4 indices
→ palette lookup each
→ RGBA bilinear
```

The test must explicitly distinguish this from interpolating indices first.

### E5 — Bilinear oracle matrix is incomplete

The required fractions `0x0001`, `0x4000`, `0xC000` are missing, and there is no nontrivial 2×2 vector that proves horizontal-round-then-vertical-round ordering.

### E6 — Straight-vs-Premult equivalence vector is missing

Add one independently calculated equivalence case under conditions where the frozen rounding rules make equality expected.

### E7 — Dither verification is incomplete

The 4×4 R-channel test is useful, but still missing:

- translated draw proving global destination x/y indexing;
- dither disabled exact regression;
- non-RGB target Strict behavior;
- non-RGB target non-Strict behavior.

### E8 — Destination-format exact matrix is missing

Add exact byte-level coverage for RGB565 / ARGB8888 / XRGB8888 including COPY, Straight Alpha, and one scaled BLIT_EXT path. XRGB must explicitly store `BB GG RR FF`.

### E9 — Address-mode matrix is incomplete

Add Repeat cases equivalent to `-1`, `size`, `size+1`, and `2*size+k`, including nonzero source origin. Bilinear must apply address mode independently to all neighbors.

### E10 — Extension Strict/header matrix is incomplete

Current tests cover W12 Reserved and several fault-priority cases, but not the full requested matrix for `EXT_FLAGS`, W12–W15, malformed type/version/length, and Strict/non-Strict behavior.

Production code still groups nonzero `EXT_FLAGS` with type/version/length as `BAD_EXT_TYPE`; verify and align this with the frozen ISA Reserved policy.

### E11 — Memory-fault negative matrix is incomplete

Previously missing cases remain absent, including Bilinear neighbor failure, palette offset/boundary failure, destination allocation/write boundary failure, and exact source-stride/allocation boundary behavior.

### E12 — Report overstates closure

REPORT_003R marks the V1–V10 differential/oracle closure as CLOSED, but several mandatory suites are absent or only partial.

The next report must map every V1–V10 item to the exact test executable/function that satisfies it.

## 4. Non-Blocking Notes

- END_COMMIT handling is now acceptable: implementation commit `470d7b365343bc42a2987d874152c963840549e6`, followed by a documentation commit.
- The regression matrix file is now actually updated, but some rows currently label coverage more strongly than the tests justify.
- The approved raster-bounds decision remains valid and should be reused unchanged by Tile.

## 5. Decision

The architecture is now close to the desired gate. The remaining problems are mainly:

```text
verification incompleteness
+
overstated report claims
```

This is still the Golden correctness gate before Tile, so it should not be waived.

# **REVIEW_003R_V2 = FAIL / CONTINUE 003R**

## 6. Required Final Completion

Continue the same 003R task and finish:

1. real pixel-comparing core Alpha/Key/Add random differential;
2. parameterized nearest-scale differential;
3. Clip differential/random suite;
4. Indexed8 Bilinear independent oracle;
5. full Bilinear fraction + 2D rounding-order vectors;
6. Straight-vs-Premult equivalence vector;
7. translated/disabled/non-RGB dither cases;
8. destination-format exact matrix;
9. complete Clamp/Repeat period cases;
10. complete Extension Strict/header matrix;
11. complete sampler/memory negative matrix;
12. truthful regression-matrix/report update.

Keep all current passing tests.

## 7. Tile Gate

TASK_004 remains blocked.

The next submission should make this statement genuinely supportable:

> **The Immediate Golden pipeline has independent exact or differential verification for every nontrivial sampling, blending, clipping, palette and target-format semantic that Tile will reuse.**
