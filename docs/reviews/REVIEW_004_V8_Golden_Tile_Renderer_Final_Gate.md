# REVIEW_004_V8 — Golden Tile Renderer Final Gate Review

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Previous reviewed HEAD: `d9f434bd99421824dd43e8dd9f2d316d1b5b5c6e`  
> Reviewed implementation commit: `03c4d905b0ea2f6a15f375cf91b18872c5c2aafb`  
> Reviewed HEAD: `d43f4f1f7c2389979b1b5752aa819c5be99fbfdb`  
> Decision: **FAIL / CONTINUE STAGE 004 REWORK**  
> Architecture redesign required: **NO**  
> Permission to start Stage 005 RTL: **NO**

---

# 1. Executive Summary

This rework closes most of REVIEW_004_V7.

Substantial accepted progress:

- `golden_test_tile_extended` now covers XRGB source/target, Color Mod, Global Alpha, Per-Pixel Alpha, corrected Premult assets, nearest scaling, bilinear scaling, Repeat and Indexed8+Palette.
- `golden_test_tile_faults` now contains additional unmapped-memory, descriptor-bounds, target-mismatch and Reserved-field cases.
- a dedicated Tile fixture validator exists and is registered in CTest.
- REPORT_004 now contains a 47-row evidence table.
- the acceptance checker now owns the authoritative 47-ID list and can parse `ctest -N`.
- the report records Agent-local `70/70 PASS`.

The Tile architecture itself is accepted.

However four mandatory final-gate problems remain:

1. **TDS-06/TR-08 still do not implement or prove the required alignment and exact fault semantics.**
2. **EQ-04 still lacks guaranteed Tile-boundary / Clamp evidence after the directed-test rewrite.**
3. **the Tile fixture validator is still only a shallow structural validator.**
4. **AUD evidence has been made to pass by mapping Sweep/EXP requirements to an unrelated random-render test, while REPORT_004 still has no exact END_COMMIT.**

Therefore:

# **REVIEW_004_V8 = FAIL / CONTINUE STAGE 004**

This should be a small final closure. Do not modify the accepted Tile architecture.

---

# 2. Accepted Closures

## O1 — Directed semantic matrix substantially improved

`golden_test_tile_extended` now has concrete Immediate-vs-Tile cases for:

```text
XRGB8888 source
XRGB8888 destination
Color Mod
Global Alpha
Per-Pixel Alpha
Premult Alpha with real assets installed in both compared GPUs
Nearest scaling
Bilinear scaling
Repeat
Indexed8 + Palette
```

The previous temporary-GPU Premult asset bug is fixed.

**Status: ACCEPTED**

---

## O2 — Random mixed-feature matrix remains accepted

The 300-frame deterministic suite retains explicit coverage counters for:

```text
Fill
Straight Alpha
Additive
Nearest
Bilinear
Color Key
Clip
Palette
```

**Status: ACCEPTED**

---

## O3 — Tile fixture replay remains command-authoritative

The fixture replay registers descriptor/header/work-list memory at W4/W5/W6 addresses decoded from checked `command.bin`, without patching the command.

**Status: ACCEPTED**

---

## O4 — Per-pixel overdraw remains exported

`TileStats` persists the full overdraw matrix and dimensions.

**Status: ACCEPTED**

---

## O5 — 47-ID report table now exists

REPORT_004 now contains one row for each authoritative Acceptance ID.

**Status: PARTIAL ACCEPT**

Evidence relevance and exact final metadata still need correction below.

---

# 3. Blocking Finding — Alignment Semantics Are Still Missing

## P1 — Misaligned Draw Descriptor base is not rejected as an alignment fault

Severity: **CRITICAL**

Command ISA V0.1 explicitly freezes:

```text
Draw Descriptor Array Base: 64B aligned
```

TASK_004 TDS-06 explicitly requires:

```text
misaligned required pointer
Use exact V0.1 fault codes
```

But `execute_tile_frame()` still contains no Draw Descriptor base alignment check before resource lookup.

The new test named:

```text
// misaligned desc base
```

uses `0x30001` but expects:

```text
MEMORY_ERROR
```

because Header memory is intentionally missing.

That test does **not** prove alignment handling. It only proves a later missing-memory error.

### Required correction

Before any Tile-array access, validate at minimum the frozen Draw Descriptor alignment:

```text
DRAW_DESC_BASE % 64 == 0
```

and return the project's exact alignment fault (`BAD_ALIGNMENT` / frozen equivalent).

If Tile Header / Work List alignment requirements are frozen elsewhere, validate them too. If not yet frozen, document the V0.1 requirements before RTL.

Add a test where all backing memories are otherwise valid, change only alignment, and assert one exact fault.

Until this is fixed:

```text
TDS-06 != PASS
TR-08 != PASS
```

---

# 4. Blocking Finding — Fault Matrix Is Still Not Exact

## P2 — Target mismatch accepts two unrelated faults

Severity: **CRITICAL**

The task explicitly states that negative tests must expect deterministic results.

Current strict-target-format test accepts:

```cpp
TILE_TARGET_MISMATCH || BAD_RECT
```

This violates the exact-fault rule.

It also means the test cannot prove the intended fault-priority contract.

### Required correction

Choose the authoritative fault and assert exactly one value.

Given TILE_FRAME target validation is performed before raster execution, the intended target mismatch should be frozen and deterministic.

---

## P3 — Required target mismatch / bounds cases remain incomplete

Severity: **HIGH**

The expanded fault test now covers useful cases, but it still does not provide the full mandatory matrix.

Still missing or not separately proven:

```text
STRICT_TARGET_MATCH base mismatch
STRICT_TARGET_MATCH stride mismatch
partial Tile Header array out-of-bounds
WorkList offset/count extending beyond an otherwise mapped WorkList
misaligned required pointer with otherwise-valid memory
Header W3 non-Strict acceptance
TILE_FLAGS Reserved non-Strict acceptance
```

The current TILE_FLAGS test comments that non-Strict should ignore Reserved bits, but only executes/asserts the Strict fault.

### Required correction

Make these table-driven cases with one exact expected result per row.

This is the final TDS-06/TR-08 closure; do not add unrelated fault cases.

---

# 5. Blocking Finding — Tile-Boundary Feature Evidence Regressed

## P4 — Current directed test no longer proves all EQ-04 cross-Tile requirements

Severity: **HIGH**

TASK_004 requires at least one Tile-boundary-crossing case for:

```text
scaling
bilinear
clip
dither
```

The rewritten `golden_test_tile_extended` contains many valuable semantics, but the old explicit:

```text
clip_cross_tile
dither_cross_tile
```

cases are no longer present in that test.

The checked fixtures provide:

```text
tile_bilinear_cross_boundary
tile_dither_cross_boundary
```

so Bilinear and Dither have durable cross-Tile evidence.

However there is no corresponding checked Clip fixture, and the random test only proves that Clip commands occur—not that a deterministic Clip case crosses a Tile boundary.

Also the Repeat test exercises out-of-range Repeat, but there is no equivalent out-of-range Clamp test. Merely leaving AddressMode at default while UV remains in range does not exercise Clamp behavior.

### Required correction

Add only the missing deterministic proofs:

```text
nearest_scale_cross_tile
clip_cross_tile
clamp_out_of_range
```

or provide equivalent checked fixtures and map EQ-04 evidence to them.

Do not rebuild the entire extended test again.

---

# 6. Tile Fixture Validator Is Still Too Shallow

## P5 — Validator does not verify WorkList/Descriptor/Header relationships

Severity: **HIGH**

The new `validate_tile_fixtures.py` correctly checks:

```text
manifest presence/basic keys
64B TILE_FRAME command
command class/opcode
descriptor file N*64
header file N*16
workref file N*4
framebuffer sizes
```

But it does not yet validate several properties requested by the previous review:

```text
pixel_arith_version / complete version metadata
grid size ↔ Tile Header count
WorkRef ranges in every Tile Header
WorkRef descriptor indices < descriptor count
encoded base alignment
command surface/stride/tile values ↔ manifest
required resource files/sizes for palette/texture/extension fixtures
```

The current Tile fixture manifest also still omits `pixel_arith_version`.

### Required correction

Extend the validator to parse Tile Headers and WorkRefs and verify their cross-file bounds.

At minimum add `pixel_arith_version` and validate the command/manifest geometry.

Replay CTests remain useful, but replay success is not a substitute for fixture structural validation.

---

# 7. Acceptance Evidence Has a New Shortcut

## P6 — Sweep/Experiment IDs are mapped to an unrelated random-render CTest

Severity: **CRITICAL — Completion Contract violation**

The follow-up commit `d43f4f1` changed:

```text
PROF-04
PROF-05
EXP-01
EXP-02
EXP-03
EXP-04
```

from `golden_tile_sweep` evidence to:

```text
golden_test_tile_eq_random
```

only because `golden_tile_sweep` is an executable and not a registered CTest.

This is exactly the shortcut forbidden by TASK_004:

> “Covered by a similar test” without exact mapping = NOT DONE.

`golden_test_tile_eq_random` does not generate/validate:

```text
tile_sweep.csv
tile_sweep_summary.md
six-workload architecture experiment
```

### Required correction

Register real sweep verification, for example:

```text
golden_test_tile_sweep
```

which invokes the functional sweep for W1–W6 / Tile16/32/64 and verifies successful generation or expected key metrics.

Then map PROF/EXP IDs to that actual test.

Do **not** change evidence to an unrelated existing test merely to satisfy the CTest-name checker.

---

# 8. Acceptance Checker / Final Report Still Have a False-Pass Path

## P7 — Missing CTest database only produces WARN

Severity: **MEDIUM/HIGH**

The “strict final gate” checker currently does:

```text
ctest -N unavailable
→ WARN
→ skip test-name existence checks
```

For a final acceptance checker, inability to validate the test database should fail the Gate, not silently degrade.

### Required correction

Default final mode:

```text
missing build/stage004 or failed ctest -N → FAIL
```

A separate developer convenience option such as `--allow-no-build` may warn if desired.

---

## P8 — Exact END_COMMIT is still missing, and the checker fails to detect the placeholder

Severity: **HIGH**

Current REPORT_004 still says:

```text
See `git log -1` after this report commit.
```

TASK_004 requires an exact END_COMMIT.

The checker searches for literal:

```text
"See git log"
```

but the report contains Markdown backticks:

```text
"See `git log"
```

so the stale marker evades the check.

### Required correction

Record the exact final implementation/report commit after all closure changes.

Checker should validate SHA form, e.g.:

```regex
^`[0-9a-f]{40}`$
```

or an explicitly documented implementation SHA plus report SHA.

---

## P9 — 47-row report matrix still omits the Requirement field

Severity: **MEDIUM**

TASK_004 requires each row to contain:

```text
Requirement
Implementation Evidence
Verification Evidence
Result
```

Current generated matrix contains:

```text
ID
Result
Implementation
Verification
```

The Acceptance ID identifies the requirement indirectly, but the report contract explicitly requested the Requirement mapping.

### Required correction

Generate the matrix with a short requirement title/summary per ID.

This can be generated from an acceptance metadata table; no manual 47-row duplication is necessary.

---

# 9. Local Test Evidence

REPORT_004 records:

```text
70/70 PASS
```

plus local PASS for the acceptance checker and Tile fixture validator.

This review did not independently execute the local build. The latest GitHub commit has no published commit-status checks.

Therefore these are:

> **Agent-reported local results, not independently reproduced CI evidence.**

Also, a green 70-test suite does not close requirements for which the current test asserts the wrong/ambiguous condition (notably alignment and target-mismatch fault semantics).

---

# 10. Gate Decision

The architecture is accepted:

```text
Software Binner                  PASS
Tile binary structures           PASS
Internal Tile memory             PASS
Tile load/render/store           PASS
Shared Pixel Backend             PASS
Immediate-vs-Tile broad coverage PASS
300-frame mixed random           PASS
Per-pixel profiler               PASS
Six-workload model               PASS
Binary fixture replay            PASS
```

The remaining blockers are narrow:

```text
exact Tile fault/alignment semantics
three deterministic EQ-04 edge cases
deeper Tile fixture structural validation
truthful Sweep/EXP acceptance evidence
exact final report/checker metadata
```

Therefore:

# **REVIEW_004_V8 = FAIL / CONTINUE STAGE 004**

No architecture redesign. No new Stage-004 feature scope.

After the items above are closed, Stage 004 should be ready to PASS.

---

# 11. Minimal Final Closure Order

1. Implement exact Draw Descriptor alignment fault and fix the misalignment test.
2. Remove the `FAULT_A || FAULT_B` target-mismatch assertion; add target base/stride exact cases and remaining bounds/Strict pairs.
3. Add `nearest_scale_cross_tile`, `clip_cross_tile`, and `clamp_out_of_range`.
4. Deepen Tile fixture validator for version/geometry/header/workref/descriptor consistency.
5. Register a real sweep CTest and restore truthful PROF/EXP evidence mappings.
6. Make missing `ctest -N` fatal in final checker mode.
7. Record exact END_COMMIT and generate the 47-row matrix with Requirement titles.
8. Run the final complete command set once and submit for final Gate review.
