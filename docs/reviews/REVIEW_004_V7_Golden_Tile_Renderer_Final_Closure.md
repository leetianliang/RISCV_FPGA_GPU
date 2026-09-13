# REVIEW_004_V7 — Golden Tile Renderer Final Closure Review

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Previous reviewed implementation: `2f1fdc718902adf72ca381b3941e6c24dc9a87fa`  
> Reviewed implementation commit: `cea792ceab1f5b62cce4a686e656fee3b243c776`  
> Reviewed HEAD / report commit: `d9f434bd99421824dd43e8dd9f2d316d1b5b5c6e`  
> Decision: **FAIL / CONTINUE STAGE 004 REWORK**  
> Architecture redesign required: **NO**  
> Permission to start Stage 005 RTL: **NO**

## 1. Executive Summary

The latest rework closes a large fraction of REVIEW_004_V6 correctly.

Accepted closures include:

- EQ-07 now includes Clip and Indexed8+Palette and has explicit coverage counters.
- TILE_FRAME fixture replay no longer patches W4/W5/W6; it registers binary structures at addresses encoded in `command.bin`.
- per-pixel overdraw is now exported in `TileStats`.
- sweep CSV preserves workload IDs/names/version/seed and the summary has been regenerated.
- REPORT_004 is now clean and records the current implementation commit.
- the acceptance manifest has been substantially improved and mostly points to real repository paths/tests.
- the checker now has an authoritative 47-ID list and validates evidence paths more strictly.

These are meaningful improvements.

However, mandatory Stage-004 acceptance is still incomplete in three major areas:

1. the dedicated Tile fault matrix is still far below TDS-06/TR-08;
2. the directed Immediate-vs-Tile equivalence matrix still lacks mandatory semantic/format cases;
3. the acceptance/report machinery still does not satisfy the TASK_004 final evidence contract.

A fourth issue remains around Tile fixture validation: the checked Tile fixtures are replayed, but the existing fixture validator still does not validate their nested metadata/layout.

Therefore:

# **REVIEW_004_V7 = FAIL / CONTINUE STAGE 004**

The core Tile architecture is accepted. Do not redesign it.

---

## 2. Accepted Closures

### N1 — EQ-07 mixed random coverage is now materially complete

The random suite now has eight feature classes and explicit counters for:

```text
FILL
Straight Alpha
Additive
BLIT_EXT nearest
BLIT_EXT bilinear
Color Key
Clip
Indexed8 + Palette
```

It fails if any class is absent.

**Status: CLOSED**

### N2 — Pure binary Tile fixture pointer authority

`run-tile-fixture` now deserializes the checked TILE_FRAME command and derives descriptor/header/work-list addresses from W4/W5/W6.

It no longer overwrites those command words.

**Status: CLOSED**

### N3 — Per-pixel overdraw export

`TileStats` now includes:

```text
overdraw_matrix
overdraw_w
overdraw_h
```

and `execute_tile_frame()` copies the profiler's per-pixel counts into the persistent stats object.

**Status: CLOSED**

### N4 — Sweep traceability improved

The CSV now records:

```text
workload_id
workload_name
version
seed
```

and W4 is correctly identified as the scaled-sprite workload.

The summary was regenerated.

**Status: CLOSED**

### N5 — REPORT cleanup improved

The report is now a single clean document and records implementation END_COMMIT `cea792c`.

It records Agent-reported `69/69 PASS` rather than carrying stale prior test counts.

**Status: ACCEPTED as report hygiene**

---

## 3. Blocking Finding — Tile Fault Matrix

### N6 — `golden_test_tile_faults` still does not satisfy TDS-06 / TR-08

Severity: **CRITICAL**

The current dedicated fault test covers roughly:

```text
tile-size reconfiguration
grid mismatch
depth flag rejection
DONT_LOAD/default rule
RT_STATE Reserved Strict pair
Header W3 Strict case
```

It still lacks required exact cases including:

```text
Tile Header array out-of-bounds
WorkList range out-of-bounds
descriptor index out-of-bounds
misaligned descriptor/header/work-list pointer
unmapped descriptor base
unmapped Tile Header base
unmapped WorkRef base
STRICT_TARGET_MATCH base mismatch
STRICT_TARGET_MATCH stride mismatch
STRICT_TARGET_MATCH format mismatch
TILE_FLAGS Reserved Strict=0/1 pair
Header W3 Strict=0 acceptance pair
destination allocation boundary
```

TASK_004 explicitly made these mandatory.

The acceptance manifest nevertheless marks:

```text
TDS-06 = PASS
TR-08 = PASS
```

### Required correction

Expand `golden_test_tile_faults` into a table-driven exact fault matrix.

Every case must assert one exact `FaultCode`.

Do not mark TDS-06/TR-08 PASS until this matrix exists.

---

## 4. Blocking Finding — Directed Tile Equivalence

### N7 — `golden_test_tile_extended` was not expanded in this rework

Severity: **CRITICAL**

The V6 closure commit does not modify `test_tile_extended.cpp`.

Therefore the previous missing directed cases remain missing.

The dedicated Tile test still centers on:

```text
ARGB8888 destination + RGB565 BLIT
Premult command path
Clip cross-Tile
RGB565 Dither cross-Tile
```

Still missing dedicated Tile-path evidence includes at least:

```text
XRGB8888 source
XRGB8888 destination
Color Mod
Repeat address mode
```

and the formal acceptance matrix still lacks clean explicit directed mapping for all of:

```text
Global Alpha
Per-Pixel Alpha
Nearest scaling
Bilinear scaling
Clamp
Indexed8 + Palette
```

The Premult test-quality issue also remains: intended ARGB premult texture preparation occurs in a temporary GPU that is discarded before the actual comparison.

### Required correction

Refactor the directed helper to accept explicit source resources/assets and install identical assets into the actual Immediate and Tile GPUs.

Add the missing format/state/address-mode cases.

Do not use random coverage alone as the directed acceptance evidence for EQ-02/EQ-04/EQ-05.

---

## 5. Blocking Finding — Tile Fixture Validation

### N8 — Tile fixtures replay, but are still not validated by the fixture validator

Severity: **HIGH**

Five Tile fixture replay CTests now exist, which is good.

However no latest change extends:

```text
tools/fixture_validate/fixture_validate.py
```

to recurse into:

```text
frames/tile/<fixture>/
```

or understand TILE_FRAME opcode/layout.

No dedicated Tile fixture validator was added.

Therefore checked fixture metadata/structure can still be malformed while replay happens to succeed.

### Required correction

Add either:

```text
golden_tile_fixture_validate
```

or recursive Tile-aware validation.

At minimum validate:

```text
manifest version fields
TILE_FRAME opcode/64B command size
descriptor array 64B alignment
Tile Header array 16B alignment/count
WorkRef array 4B alignment/ranges
encoded base addresses
resource file presence/sizes
initial/golden framebuffer sizes
```

Register validator in CTest.

---

## 6. Acceptance-System Findings

### N9 — Checker improved, but still does not validate declared CTest names

Severity: **HIGH — AUD-02 incomplete**

The checker now has an authoritative ID list and validates many evidence paths.

However:

```text
BUILD = ROOT / build/stage004
```

is defined but never used.

The checker does not run/parse:

```text
ctest -N
```

and therefore does not verify that each `test_name` in the manifest actually exists.

The task explicitly required this final-gate behavior.

### Required correction

Accept a build directory or use `build/stage004`, parse `ctest -N`, and verify every concrete declared test name.

Wildcard evidence such as `golden_tile_fixture_*` must either expand to real tests or be disallowed in `test_name`.

---

### N10 — Checker still does not enforce a real 47-row report evidence matrix

Severity: **HIGH — AUD-03 incomplete**

The current REPORT_004 contains:

- a representative 7-row evidence table;
- one line listing all 47 IDs.

It does **not** contain the required full per-ID matrix:

```text
ID
Requirement
Implementation Evidence
Verification Evidence
Result
```

The checker only verifies that each ID string occurs somewhere in the report.

Thus Section 17 of TASK_004 is still not satisfied.

### Required correction

Generate a real 47-row evidence matrix from the acceptance manifest into REPORT_004.

The checker should parse/verify one row per authoritative ID, not just substring presence.

---

## 7. Manifest Evidence Quality

### N11 — Manifest is much better, but several PASS mappings are still too broad

Severity: **MEDIUM/HIGH**

Most synthetic `model/golden/<id>` placeholders were replaced.

However several acceptance items still point to a broad test that does not by itself establish the exact requirement.

Examples include TDS/BIN/TR requirements mapped generically to `golden_test_tile`.

Most importantly, `TDS-06` is mapped to `golden_test_tile_faults` even though that test does not contain the mandatory fault matrix, and EQ-02/EQ-04/EQ-05 are mapped to `golden_test_tile_extended` despite the missing directed cases.

This is an evidence-validity issue, not merely a formatting issue.

### Required correction

After the missing tests are added, map each acceptance ID to the exact test that really proves it.

---

## 8. Local Test Evidence

REPORT_004 records:

```text
Total Tests: 69
100% tests passed
```

for the current implementation state.

This is useful Agent-reported local evidence.

The reviewed GitHub commit has no published commit-status checks, so the result was not independently reproduced through CI in this review.

Do not interpret `69/69` as proof that all 47 acceptance requirements are complete; several missing requirements have no test registered yet.

---

## 9. Gate Decision

Accepted architecture:

```text
CPU software binner
serialized Tile descriptors/headers/workrefs
TILE_FRAME binary path
internal Tile memory
Tile load/render/store
shared sampler/pixel backend
DST-format compatibility quantization
WorkRef ordering
global Dither coordinates
global per-pixel overdraw
LOAD/DONT_LOAD/CLEAR rule
Strict Reserved implementation
mixed-feature 300-frame random equivalence
binary Tile fixture replay
six-workload functional Tile sweep
```

Still blocking Stage 005:

```text
complete Tile fault matrix
complete directed Tile semantic/format matrix
Tile-aware fixture validation
CTest-name-aware acceptance checker
real 47-row report evidence matrix
```

Therefore:

# **REVIEW_004_V7 = FAIL / CONTINUE STAGE 004**

This is a final verification/evidence closure issue only.

---

## 10. Recommended Final Closure Order

1. Complete `golden_test_tile_faults` with all mandatory exact fault cases.
2. Complete `golden_test_tile_extended` for XRGB / Color Mod / Global+Pixel Alpha / nearest+bilinear / Clamp+Repeat / Indexed8+Palette.
3. Fix the Premult directed asset setup.
4. Add Tile-aware fixture validation and register it in CTest.
5. Make the acceptance checker validate actual CTest names via `ctest -N`.
6. Generate and verify a real 47-row REPORT_004 evidence matrix.
7. Re-run the complete Stage-004 command set on the final commit and record one exact final result.

After those items are complete, Stage 004 should be ready for PASS and Stage 005 RTL can begin.
