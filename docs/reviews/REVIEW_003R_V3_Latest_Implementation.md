# REVIEW_003R_V3 — Latest Golden Sprite Pipeline Review

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Reviewed commit: `105c23c0989c74fcd6273b33181c6079e40f5d5e`  
> Previous review baseline: `362a3e7cf415f93355c2af9de50f543b3e0e193f`  
> Decision: **PASS WITH ACTIONS**  
> Architecture redesign required: **NO**  
> Permission to plan/start Stage 004: **YES, with a mandatory Block 0 before Tile pixel-equivalence work**

---

## 1. Executive Summary

The latest rework materially improves the Golden verification story.

The following items are now real rather than merely claimed:

- a pixel-comparing Alpha/Additive differential test exists;
- nearest-scaling coverage is broadened across multiple source/destination sizes;
- a randomized Clip framebuffer comparison exists;
- exact directed tests exist for Bilinear fractions, Indexed8 palette sampling, Repeat, and Bayer dither;
- a stronger Premult matrix remains in place;
- the regression matrix maps more features to concrete tests.

The valid-path Immediate Golden renderer is now strong enough to serve as the basis for the next architecture stage.

However, a small number of verification defects remain. They are important enough to keep as mandatory actions, but they no longer justify blocking all Tile infrastructure work.

Therefore:

> **Stage 003R is accepted as PASS WITH ACTIONS.**

Stage 004 may begin, but its first internal block must close the actions below before Immediate-vs-Tile pixel-exact acceptance is allowed.

---

## 2. Accepted Improvements

### A. Core Alpha/Additive differential

`golden_test_core_alpha_diff` now builds an independent expected framebuffer and compares every destination pixel.

This is a major improvement over the previous no-fault-only random test.

**Accepted.**

### B. Parameterized scale regression

`golden_test_scale_param_diff` now exercises multiple up/down/non-integer shapes and nonzero source origins.

**Accepted as useful coverage, with independence/stride actions below.**

### C. Clip differential

`golden_test_clip_diff` independently constructs raster coverage and compares the final framebuffer.

This directly exercises the approved rule:

```text
destination ∩ explicit clip ∩ render-target bounds
```

with UV anchored to the original destination origin.

**Accepted.**

### D. Exact feature vectors

`golden_test_oracle_v2` adds useful exact checks for:

- required Bilinear fractions;
- 2D Bilinear sampling;
- Indexed8 + Palette Bilinear;
- Straight-vs-Premult equivalence;
- Repeat-period behavior.

`golden_test_dither_dst` adds translated dither and destination-format checks.

**Accepted as meaningful directed coverage.**

### E. Regression governance

`GOLDEN_REGRESSION_MATRIX.md` now points to the expanded test set.

**Accepted.**

---

## 3. Mandatory Actions

These actions must be completed in Stage-004 Block 0.

### PWA-1 — Tautological memory-negative assertion reintroduced

Severity: **HIGH**

`test_ext_mem_matrix.cpp` contains:

```cpp
EXPECT_TRUE(!st.ok || st.ok);
```

This expression is always true.

It is the same class of verification defect that 003R was specifically created to eliminate.

The current `check_test_integrity.py` fails to detect it because its regex does not cover member expressions such as `st.ok`.

Required:

- replace the assertion with one exact expected result;
- extend the integrity script to catch member-access tautologies;
- scan the whole Golden test tree again.

This must be fixed before Stage-004 pixel-exact Tile acceptance.

---

### PWA-2 — Nearest scale “independent oracle” still shares production helpers

Severity: **MEDIUM/HIGH**

`test_scale_param_diff.cpp` computes expected mapping using:

- `compute_axis_aligned_uv(...)`;
- `compute_axis_aligned_uv_v(...)`;
- `nearest_index_q16(...)`.

These are production Golden helpers, so a bug in a helper can be shared by both actual and expected paths.

Also, the `sstride_hint` argument is ignored and source stride is reconstructed as tight `tex_w * 2`, so padded source-stride coverage is not actually exercised.

Required:

- implement local test-only coefficient/mapping equations from the frozen formula;
- do not call production UV/nearest helpers in the expected path;
- actually test padded source stride.

---

### PWA-3 — Some “proof” assertions are weaker than the report claims

Severity: **MEDIUM**

Examples:

- Indexed8 Bilinear says “index-first would differ”, but uses a hard-coded `r_idx_first = 0` rather than calculating the actual index-first alternative.
- The Bilinear 2D test computes the correct horizontal-then-vertical result, but does not explicitly calculate an alternative ordering and prove the selected vector distinguishes the two.
- One translated-dither assertion contains an OR whose second clause is statically true for the selected Bayer positions, although the earlier per-pixel checks already provide useful evidence.

Required:

Strengthen these tests so every “proves X differs from Y” claim actually computes both X and Y and asserts inequality.

---

### PWA-4 — Destination-format acceptance matrix is incomplete

Severity: **MEDIUM**

The current destination-format test covers:

- COPY for RGB565 / ARGB8888 / XRGB8888;
- Straight Alpha for ARGB8888.

The requested matrix also needs:

- Straight Alpha into RGB565;
- Straight Alpha into XRGB8888;
- at least one scaled BLIT_EXT into each supported render-target format.

Also tighten non-RGB dither Strict behavior to one exact expected fault rather than accepting multiple unrelated faults.

Required before Tile target-format equivalence is declared complete.

---

### PWA-5 — Extension / memory negative matrix is still incomplete

Severity: **MEDIUM/HIGH**

The new extension test improves malformed type/version/length coverage and one Reserved word pair.

Still required:

- explicit `EXT_FLAGS` behavior;
- broader W12–W15 Reserved Strict/non-Strict coverage;
- real Bilinear-neighbor fault assertion;
- palette address boundary/overflow case;
- destination allocation boundary negative;
- exact deterministic fault codes.

The current Bilinear-neighbor case is especially important because it presently uses the tautological assertion described in PWA-1.

---

### PWA-6 — Report metadata is stale/inconsistent

Severity: **LOW**

The report header says:

```text
CTest 59/59 PASS
```

but the command transcript still says:

```text
53/53
```

and `END_COMMIT` is not recorded as a concrete SHA in the final report.

Required:

- record the current implementation commit exactly:
  `105c23c0989c74fcd6273b33181c6079e40f5d5e`
  or a later final report commit;
- update the actual test count consistently.

---

## 4. Gate Decision

The previous reviews blocked Tile because the Immediate Golden renderer lacked independent verification for major semantics.

That is no longer broadly true: the latest commit adds real framebuffer-comparing and exact directed tests for the core valid path.

The remaining findings are narrower:

```text
test-strength defects
negative-path completeness
some oracle independence
target-format matrix completeness
report hygiene
```

They should be closed before using Golden as the final judge for Tile equivalence, but they do not require another full stop on all architecture work.

Therefore:

# **REVIEW_003R_V3 = PASS WITH ACTIONS**

---

## 5. Stage-004 Entry Condition

Stage 004 may start with:

```text
Block 0 — Golden Verification Finalization
```

Block 0 must close PWA-1 through PWA-6.

After Block 0 is locally green, the agent may continue within Stage 004 into:

```text
Tile structures
Software binning
WorkList
Tile renderer
Immediate-vs-Tile exact comparison
```

No separate formal review is required between Block 0 and the rest of Stage 004 unless a specification conflict appears.

---

## 6. Review Notes for Future Task Format

Starting with TASK_004, each mandatory requirement should have:

```text
Acceptance ID
Implementation evidence
Verification evidence
Forbidden shortcut
Block exit condition
```

A stage may contain multiple internal Blocks but still receive one formal review at the end.
