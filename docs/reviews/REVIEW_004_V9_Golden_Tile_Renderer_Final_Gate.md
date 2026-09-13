# REVIEW_004_V9 — Golden Tile Renderer Final Gate

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Previous reviewed HEAD: `d43f4f1f7c2389979b1b5752aa819c5be99fbfdb`  
> Stage implementation commit: `956f70bf0ca9052ecf2e460c72b949ce1e29fee5`  
> Reviewed HEAD / report commit: `f034f01ba913c44ce749e7756bd682008f75d08a`  
> Decision: **PASS WITH ACTIONS**  
> Architecture redesign required: **NO**  
> Permission to start Stage 005 RTL: **YES**

---

# 1. Executive Summary

Stage 004 has reached the Tile Architecture Gate.

The final rework closes the blocking findings from REVIEW_004_V8:

- TILE_FRAME array alignment is now checked before memory access and returns deterministic `BAD_ALIGNMENT`.
- strict target format/base/stride mismatch tests now expect exact `TILE_TARGET_MISMATCH`.
- partial Tile Header bounds, WorkList range, Reserved Strict/Non-Strict behavior and alignment cases were added.
- deterministic `nearest_scale_cross_tile`, `clip_cross_tile`, and `clamp_out_of_range` cases were added.
- Tile fixture validation now parses geometry, headers and WorkRefs rather than checking file sizes only.
- a real `golden_test_tile_sweep` CTest now drives W1–W6 × 16/32/64 through the functional model.
- the acceptance checker now treats missing `ctest -N` as a failure in final mode.
- REPORT_004 now contains the full 47-row Requirement/Evidence matrix and an exact implementation END_COMMIT.
- Agent reports a final local result of `71/71 PASS`.

The central proof required by Stage 004 is now credible:

```text
ordered Draw2D stream
→ Immediate Golden

same stream
→ software binner
→ serialized Draw Descriptor / Tile Header / WorkRef
→ TILE_FRAME
→ internal Tile load/render/store
→ shared pixel backend

final framebuffer bytes: exact match
```

The reviewer therefore closes Stage 004 as:

# **PASS WITH ACTIONS**

Stage 005 RTL may begin.

The actions below are hardening/synchronization items. They do not require reopening Stage 004 unless they expose a functional regression.

---

# 2. Gate-Critical Closures

## Q1 — Exact alignment handling: CLOSED

`execute_tile_frame()` now validates:

```text
DRAW_DESC_BASE  % 64 == 0
TILE_HEADER_BASE % 16 == 0
WORK_LIST_BASE   % 4 == 0
```

before array/resource access.

Misalignment returns:

```text
FAULT_BAD_ALIGNMENT
```

The Draw Descriptor 64B alignment is directly frozen by Command ISA V0.1.

This review additionally approves the Stage-004/Stage-005 compatibility interpretation:

```text
Tile Header Array Base = 16B aligned
Work List Base         = 4B aligned
```

for Competition V0.1.

**Reviewer decision: APPROVED / FROZEN FOR RTL COMPATIBILITY.**

Stage 005 RTL must implement the same 64/16/4 alignment rules and exact fault code.

---

## Q2 — TDS-06 / TR-08 fault closure: PASS

The current fault suite now establishes deterministic evidence for the Stage-required classes, including:

```text
unmapped descriptor memory
unmapped Tile Header memory
unmapped Work List memory
descriptor index out of range
misaligned Descriptor/Header/WorkList base
destination allocation failure
STRICT_TARGET_MATCH format mismatch
STRICT_TARGET_MATCH base mismatch
STRICT_TARGET_MATCH stride mismatch
partial Tile Header array OOB
WorkList offset/count OOB
RT_STATE Reserved Strict behavior
Header W3 Strict / Non-Strict behavior
TILE_FLAGS Reserved Strict / Non-Strict behavior
unsupported depth Tile flags
bad grid configuration
```

The previous `FAULT_A || FAULT_B` shortcut was removed for target mismatch.

**Status: CLOSED**

---

## Q3 — EQ-04 boundary/extended feature closure: PASS

The directed Tile equivalence suite now includes explicit cases for:

```text
XRGB8888 source
XRGB8888 destination
Color Mod
Global Alpha
Per-Pixel Alpha
Premultiplied Alpha
Nearest scaling
Bilinear scaling
Repeat
Indexed8 + Palette
nearest scaling crossing a Tile boundary
Clip crossing a Tile boundary
out-of-range Clamp
```

Checked Tile fixtures additionally retain durable cross-Tile evidence for:

```text
Bilinear
Dither
```

The corrected Premult test installs the prepared source asset in the actual Immediate and Tile GPUs used by the comparison.

**Status: CLOSED**

---

## Q4 — Binary Tile fixture gate: PASS

Formal checked Tile fixtures are now:

```text
tile_fill_basic
tile_alpha_overlap
tile_bilinear_cross_boundary
tile_palette
tile_dither_cross_boundary
```

The replay path uses W4/W5/W6 from serialized `command.bin` and does not patch architectural command pointers.

The Tile validator now checks:

```text
manifest version fields
TILE_FRAME opcode
array base alignment
surface/grid/tile geometry
command vs manifest base/stride/geometry
Descriptor N×64B structure
Tile Header N×16B structure
WorkRef N×4B structure
header count vs grid
per-header WorkRef ranges
WorkRef descriptor-index bounds
framebuffer sizes
palette/texture/extension structural sizes
```

and is registered as:

```text
golden_tile_fixture_validate
```

**Status: CLOSED for Stage-004 Gate**

---

## Q5 — PROF / EXP evidence: PASS

The previous fake mapping from Sweep requirements to `golden_test_tile_eq_random` has been removed.

A real CTest now exists:

```text
golden_test_tile_sweep
```

It invokes the functional sweep and exercises:

```text
W1 Sprite Grid
W2 High Overdraw
W3 Alpha Storm
W4 Large Scaled Sprites
W5 Edge/Scatter
W6 Mixed Scene
```

for:

```text
Tile16
Tile32
Tile64
```

The output contains 18 rows and preserves workload identity/version/seed.

The PROF/EXP acceptance IDs now map to this actual sweep test.

**Status: CLOSED**

---

## Q6 — Acceptance contract: PASS

The Stage acceptance system now contains:

```text
47 authoritative mandatory IDs
Requirement text
Implementation evidence
Verification evidence
CTest test_name
PASS status
```

The final checker:

- has the authoritative 47-ID list;
- rejects missing/duplicate/misordered IDs;
- checks repository evidence paths;
- parses `ctest -N`;
- rejects unknown declared Golden test names;
- fails when the Stage build/Test database is unavailable in final mode;
- verifies an exact 40-hex END_COMMIT;
- requires the Requirement column in REPORT_004.

REPORT_004 now contains the complete 47-row matrix.

**Status: CLOSED**

---

# 3. Architecture Gate Assessment

The following Stage-004 architecture is now accepted as the functional Golden baseline for RTL:

```text
RISC-V / CPU
    │
    │ ordered generic Draw2D API
    ▼
Software Tile Binner
    │
    ├── 64B Draw Descriptor Array
    ├── 16B Tile Header Array
    └── 32-bit WorkRef Array
    │
    ▼
64B TILE_FRAME
    │
    ▼
Golden Tile Renderer
    │
    ├── framebuffer Tile load / clear / default
    ├── internal Tile Memory
    ├── shared Texture/Sampler/Palette path
    ├── shared Key/Mod/Alpha/Blend path
    ├── DST_FORMAT quantization per logical RT write
    ├── global Dither coordinates
    └── Tile store
    │
    ▼
Final framebuffer
```

The Stage-005 RTL implementation should preserve this partition rather than redesigning the semantics.

---

# 4. Final Verification Evidence

REPORT_004 records the following final local verification:

```text
ctest --test-dir build/stage004 --output-on-failure
71/71 PASS

python scripts/check_stage004_acceptance.py
PASS

python tools/fixture_validate/validate_tile_fixtures.py
PASS

python tools/fixture_validate/fixture_validate.py
PASS

python scripts/check_test_integrity.py
PASS

python model/architecture/tile_model/run_tile_sweep.py
18 rows: W1–W6 × Tile16/32/64
```

The reported implementation END_COMMIT is:

```text
956f70bf0ca9052ecf2e460c72b949ce1e29fee5
```

The reviewed repository HEAD is the subsequent report-only commit:

```text
f034f01ba913c44ce749e7756bd682008f75d08a
```

No published GitHub commit-status/CI checks were present at review time, so `71/71 PASS` remains **Agent-reported local evidence**, not independently reproduced CI evidence.

This distinction does not block the Stage gate because the code/evidence mapping was statically reviewed and the Stage workflow has consistently treated exact Agent local test logs as admissible verification evidence in the absence of CI.

---

# 5. Non-Blocking Actions

## A1 — Synchronize Tile alignment into the controlled specifications

Before Stage-005 parser/RTL interface freeze, update the next controlled Command ISA/System Architecture revision to explicitly state:

```text
DRAW_DESC_BASE   64B
TILE_HEADER_BASE 16B
WORK_LIST_BASE    4B
misalignment → BAD_ALIGNMENT
```

The current decision document is sufficient to start RTL; the controlled spec should catch up.

---

## A2 — Harden 32-bit Tile-array address arithmetic

The Golden path still computes at least the Tile Header address in 32-bit arithmetic:

```text
tile_header_base + tile_id * 16
```

For ordinary competition memory maps this is not an issue, but a base near the 4GiB boundary could wrap before the memory lookup.

Before final RTL/Golden sign-off, use 64-bit intermediate arithmetic for:

```text
Tile Header address
array-end calculations
```

and fault deterministically if the final physical address exceeds 32-bit space.

This is a boundary hardening action, not a reason to reopen the current architecture gate.

---

## A3 — Strengthen Tile fixture dependency validation when convenient

The Tile fixture validator now closes the Stage requirement.

A useful follow-up hardening is to parse Descriptor feature flags and require the corresponding checked resource files when a fixture actually references:

```text
Extension
Palette
Texture
```

Also reject zero-length Descriptor arrays when WorkRefs are present.

This is recommended before using fixtures as long-term public compatibility vectors.

---

## A4 — Make the sweep runner portable if Linux CI is introduced

`run_tile_sweep.py` currently resolves:

```text
golden_tile_sweep.exe
```

which matches the current Windows local workflow.

If Linux CI is introduced, resolve the platform executable name instead of hardcoding `.exe`.

This does not affect the verified Windows Stage-004 flow.

---

# 6. Stage-005 Entry Conditions

Stage 005 may now start.

The RTL stage must treat the following as frozen inputs rather than design suggestions:

```text
64B command / descriptor encoding
TILE_FRAME wire layout
32-bit physical address
64B / 16B / 4B Tile-array alignment
WorkRef = 32-bit descriptor index
row-major Tile ID
stable WorkRef ordering
TILE_FRAME target authority
STRICT_TARGET_MATCH behavior
LOAD / DEFAULT / CLEAR / STORE rule
shared Immediate/Tile pixel semantics
RGBA8888 canonical internal color semantics
RGB565 compatibility quantization rule
global Dither coordinates
16/32/64 Golden Tile-size evidence
default implementation target Tile32
```

RTL verification should begin against existing Golden fixtures and Immediate-vs-Tile vectors rather than inventing new expected pixels in RTL tests.

---

# 7. Decision

# **REVIEW_004_V9 = PASS WITH ACTIONS**

**Stage 004 is closed.**

**Stage 005 RTL is unblocked.**

Do not reopen Stage 004 for the non-blocking hardening actions unless a later change reveals a functional incompatibility with the accepted Golden behavior.
