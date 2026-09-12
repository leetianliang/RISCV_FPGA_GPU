# TASK_003R — Golden Sprite Pipeline Correctness Closure

> Project: RISC-V–FPGA General-Purpose 2D GPU  
> Task ID: `003R`  
> Stage type: **Corrective Gate / Rework Stage**  
> Executor: Local AI Agent  
> Reviewer: Project Architect / ChatGPT  
> Entry decision: `REVIEW_003 = FAIL / REWORK`  
> Baseline commit: `f55f5ad8209e9badc4681b383b66ac1c4a75b744`  
> Priority: P0  
> Formal review: one review after the entire corrective stage  
> Tile implementation: **FORBIDDEN in this task**

---

# 1. Objective

Turn the Stage-003 immediate renderer from:

> feature-rich but insufficiently proven

into:

> **authoritative, independently verified Golden behavior suitable to judge Tile/RTL implementations**

This task is a correctness/verification closure.

Do not add new product features beyond what Stage 003 already claims.

The target feature set remains:

```text
FILL
BLIT
BLIT_EXT

RGB565 / ARGB8888 / XRGB8888 / INDEX8
Palette

COPY
STRAIGHT_ALPHA
PREMULT_ALPHA
ADD_SAT

Color Key
Color Mod
Global Alpha
Pixel Alpha

Nearest
Bilinear
Clamp
Repeat
Scale
Flip
Clip
RGB565 Dither
RGB565/ARGB/XRGB Render Targets
```

The purpose is to make those claims true and demonstrable.

---

# 2. Authoritative Sources

Read before changes:

1. `docs/RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`
2. `docs/RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md`
3. `docs/RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md`
4. `docs/RISC-V_FPGA_2D_GPU_Golden_GPU_Software_Architecture_V0.1.md`
5. `docs/RISC-V_FPGA_2D_GPU_Verification_Plan_V0.1.md`
6. `docs/reviews/REVIEW_003_Golden_Feature_Complete_Sprite_Pipeline.md`
7. `docs/testing/GOLDEN_REGRESSION_MATRIX.md`
8. `AGENTS.md`

Do not change a frozen numerical rule to fit current code.

---

# 3. Explicit Non-Goals

Do NOT implement:

- TILE_FRAME;
- Tile Binner;
- Tile Renderer;
- WorkRef;
- Texture Cache;
- Affine;
- Triangle;
- RTL;
- board code;
- cycle model;
- GUI.

Do not optimize for speed.

---

# 4. Correction Group R1 — Premultiplied Alpha

This is the highest-priority correction.

Refactor the pixel path so the following values are conceptually separate:

```text
sampled_src
color_modulated_src_rgb
source_alpha
extra_opacity
effective_alpha
source_contribution
destination
output
```

Avoid mutating one `src` object and later losing whether Color Mod has already been applied.

---

## 4.1 Required exact equations

For Premultiplied source:

```text
Aextra:
E0 = 255
E1 = MUL8_RN(E0, A_mod)
E2 = MUL8_RN(E1, A_global)
E3 = MUL8_RN(E2, A_coverage)
Aextra = E3

Aeff = MUL8_RN(S_a, Aextra)

S'_c = MUL8_RN(S_c, M_c)       when COLOR_MOD_EN
S'_c = S_c                      otherwise

Scontrib_c = MUL8_RN(S'_c, Aextra)

O_c =
SAT_U8(
    Scontrib_c +
    MUL8_RN(D_c, 255 - Aeff)
)

O_a =
Aeff + MUL8_RN(D_a, 255 - Aeff)
```

Follow the authoritative arithmetic document if any notation differs.

---

## 4.2 Premult directed oracle tests

Do not derive expected pixels by invoking production blend code.

Required exact cases:

```text
transparent premult source over nonzero dst
50% premult source over nonzero dst
opaque premult source over nonzero dst
global alpha <255
color-mod alpha <255
color-mod RGB
global + mod combination
```

Required equivalence case:

Create a straight RGBA source and a correctly premultiplied equivalent.

Where mathematically applicable, compare:

```text
Straight path
vs
Premult path
```

within the exact frozen rounding semantics.

---

# 5. Correction Group R2 — Memory Fault Propagation

Refactor sampler APIs so failures cannot be discarded.

Recommended concepts:

```cpp
struct SampleResult {
    ExecResult status;
    Rgba8888 color;
};
```

or equivalent.

Every call to:

```text
SurfaceView::read_raw
SurfaceView::read_rgba
MemoryImage::read32 palette
```

must be checked.

---

## 5.1 Required failure cases

Directed tests:

```text
source rectangle exceeds RegisteredResource
source stride causes row access outside allocation
bilinear neighbor cannot be fetched
palette base unmapped
palette base + index*4 overflows/unmapped
destination write exceeds allocation
```

Expected behavior must be a deterministic architectural fault, not zero/black fallback.

Framebuffer must not be modified after a pre-render validation failure.

For mid-command memory failure, document the current synchronous Golden partial-write rule if the ISA does not specify rollback.

---

# 6. Correction Group R3 — BLIT_EXT / Clip Semantics

Separate:

```text
H_EXT_VALID
CLIP_EN
BLIT_EXT opcode
```

They are not synonyms.

Required:

- BLIT_EXT can execute with `CLIP_EN=0`;
- extension UV is still consumed;
- clip fields are ignored when `CLIP_EN=0`;
- clip fields are applied when `CLIP_EN=1`;
- `make_blit_ext_cmd()` must preserve caller Clip intent instead of forcing true.

Tests:

```text
same BLIT_EXT ext, clip flag OFF → unclipped
same BLIT_EXT ext, clip flag ON  → clipped
```

---

# 7. Correction Group R4 — Axis-Aligned BLIT_EXT

Enforce the architectural boundary:

```text
BLIT_EXT:
DV_DX = 0
DU_DY = 0

AFFINE_BLIT:
full matrix later
```

Reject BLIT_EXT cross terms regardless of Strict mode.

Tests:

```text
strict=0 cross term → reject
strict=1 cross term → reject
```

Do not implement affine in this task.

---

# 8. Correction Group R5 — SurfaceView Validation

Create one reusable validation routine for command-defined resource views.

It should validate:

```text
format defined
BPP valid
stride valid
resource logical width/height
allocation size
maximum addressed byte
32-bit address range
```

For standard linear surfaces, enforce the frozen stride rule.

Do not rely on eventual per-pixel failures to validate obviously malformed surfaces.

Apply before rasterization.

---

## 8.1 Required view tests

```text
tight stride
padded stride
stride one byte too small
stride < BPP
huge stride
last legal row
last legal pixel
one byte past allocation
ARGB stride
XRGB stride
INDEX8 source stride
```

---

# 9. Correction Group R6 — Remove Tautological Tests

Search the test tree for assertions that are structurally incapable of failing.

At minimum remove/fix:

```text
!x || x
... || true
... && false
```

Also inspect “weak” assertions such as:

```text
value != 0
value > 50
```

when the frozen formula allows an exact expected result.

Replace them with exact expected values whenever practical.

Add a simple standard-library script:

```text
scripts/check_test_integrity.py
```

It need not be a full parser.

It should flag obvious forbidden textual patterns in `model/golden/tests/`.

Run it in CTest.

---

# 10. Correction Group R7 — Base Header / Extension Fault Priority

Refactor command execution to:

```text
Phase 1:
decode base header
validate version/length/class/opcode/header flags/ext pointer syntax

Phase 2:
if extension required/valid:
    fetch 64B extension

Phase 3:
validate extension header/reserved/state

Phase 4:
decode payload
execute
```

Required tests combine multiple faults to prove priority, for example:

```text
BAD_VERSION + unmapped EXT_PTR → BAD_VERSION
misaligned EXT_PTR → BAD_EXT_PTR without memory fetch
valid header + unmapped valid-aligned EXT_PTR → MEMORY_ERROR
```

---

# 11. Correction Group R8 — Extension Strict Semantics

Review all:

```text
EXT_FLAGS
W12-W15 Reserved
other Reserved fields
```

against Command ISA.

Implement paired tests:

```text
H_STRICT=0
H_STRICT=1
```

Do not assume all extension errors are Strict-dependent; distinguish:

- malformed extension type/version/length;
- Reserved-nonzero;
- unsupported semantic state.

---

# 12. Correction Group R9 — Q16 / SRC_X Boundary Safety

No signed-overflow or undefined shift is allowed.

Audit code constructing:

```text
src_x << 16
src_y << 16
u0/v0
coefficient accumulation
nearest(q+0.5)
```

Use wide intermediate arithmetic.

Determine from frozen capability/max-dimension semantics whether source coordinates above signed-Q16 representable range are:

- impossible due platform max dimensions; or
- valid and require a non-Q16 fast BLIT address path.

If the specs do not resolve this, write a `DESIGN_QUESTION` instead of inventing behavior.

At minimum:

> eliminate C++ UB even for rejected inputs.

---

# 13. Correction Group R10 — FILL Extension / Clip

Review Command ISA and define exact behavior for:

```text
FILL_RECT + H_EXT_VALID
FILL_RECT + CLIP_EN
```

If Draw2D Extension applies:

- load and use it correctly.

If unsupported by frozen ISA:

- reject explicitly.

Do not return early and silently leave default clip state when Clip was requested.

---

# 14. Correction Group R11 — No-Clip Out-of-Bounds Rule

Resolve from authoritative specifications:

```text
destination fully in bounds
destination partly outside left/top
destination partly outside right/bottom
explicit Clip enabled
```

Record the result in:

```text
docs/decisions/DECISION_GOLDEN_RASTER_BOUNDS_V0.1.md
```

Only create this decision if the existing specifications leave a genuine ambiguity.

If a spec already clearly defines the behavior, do not invent a new decision—cite the source in REPORT_003R.

Immediate mode and future Tile mode must share this exact rule.

---

# 15. Verification Group V1 — Independent Stage-002 Oracle Expansion

Extend differential tests beyond COPY.

Independent oracle coverage:

```text
Color Key
Straight Alpha
Global Alpha
Per-Pixel Alpha
Global × Pixel staged alpha
Additive
RGB565 per-write quantization
```

Use fixed seeds.

Compare exact framebuffer bytes.

Do not use the production pixel-blend function as expected-value code.

---

# 16. Verification Group V2 — Nearest Scaling Differential Suite

Independent reference implementation for nearest scaling.

Required randomized dimensions:

```text
1→N
N→1
upscale
downscale
non-integer ratios
odd sizes
nonzero SRC_X/Y
padded strides
```

Compare exact pixels.

---

# 17. Verification Group V3 — Clip Differential Suite

Randomized Clip tests must include:

```text
empty
full
left
right
top
bottom
middle crop
negative destination
scaled draw + clip
```

Key invariant:

> clipping does not reset UV origin.

Compare against an independently rendered full draw cropped by the same raster rule where valid.

---

# 18. Verification Group V4 — Indexed8 / Palette Oracle

Independent tests:

```text
nearest Index8
palette alpha
nontrivial palette entries
palette index 0
palette index 255
```

For bilinear:

```text
fetch 4 indices
lookup each palette entry
interpolate RGBA
```

Explicitly prove:

> interpolating indices first would yield a different result.

---

# 19. Verification Group V5 — Bilinear Oracle

Add exact directed vectors with hand-computable colors.

Required fractions:

```text
fx/fy = 0
0x0001
0x4000
0x8000
0xC000
0xFFFF
```

At least one test must distinguish:

```text
horizontal round then vertical round
```

from an alternative one-shot/changed-order calculation.

Then add fixed-seed random bilinear exact compare on small textures.

---

# 20. Verification Group V6 — Address Mode Tests

Directed and randomized tests for:

```text
Clamp
Repeat
```

Source rectangle must have non-zero origin in some tests.

Required Repeat coordinates:

```text
-1
size
size+1
2*size+k
```

Address domain must be the source rectangle, not full texture.

---

# 21. Verification Group V7 — Flip Composition

Required exact cases:

```text
BLIT Flip X
BLIT Flip Y
BLIT Flip XY

BLIT_EXT scaled Flip X
BLIT_EXT scaled Flip Y
BLIT_EXT scaled Flip XY
```

If the frozen spec says BLIT_EXT flip is encoded in UV coefficients rather than applied by backend flags, enforce that rule explicitly and test the encoder.

A flag must never be silently accepted while having no effect.

---

# 22. Verification Group V8 — Dither Exact Tests

Do not rely only on `rgb565_dither` fixture.

Create a 4×4 expected Bayer output using direct specification arithmetic.

Test:

```text
exact 4×4 pattern
same color translated by 1 pixel
global RT coordinate indexing
DITHER disabled
non-RGB target strict
non-RGB target non-strict
```

---

# 23. Verification Group V9 — Destination Format Tests

Exact tests for:

```text
RGB565
ARGB8888
XRGB8888
```

At minimum:

- COPY;
- Straight Alpha;
- one scaled BLIT_EXT.

For XRGB verify exact bytes:

```text
BB GG RR FF
```

---

# 24. Verification Group V10 — Premult / Color Mod Interaction

Explicitly cover:

```text
premult + no color mod
premult + RGB color mod
premult + mod alpha
premult + global alpha
premult + mod alpha + global alpha
```

This group must catch the Stage-003 double-mod bug.

---

# 25. Regression Matrix Update

Update:

```text
docs/testing/GOLDEN_REGRESSION_MATRIX.md
```

For every category, identify:

```text
directed oracle test
random/differential test if applicable
fixture regression
```

Do not use a fixture alone as the only proof for a nontrivial arithmetic feature.

---

# 26. Fixture Policy

Keep all existing checked fixtures unless a fixture is proven incorrect.

If fixing a Golden arithmetic bug changes a checked fixture:

1. first add a directed independent oracle that proves the old fixture wrong;
2. document the reason;
3. explicitly regenerate only affected fixture(s);
4. include the binary diff/hash change in REPORT_003R.

Do not bulk-regenerate all fixtures without justification.

This is especially relevant to:

```text
premult_alpha
```

which is expected to change if it encoded the Stage-003 incorrect arithmetic.

---

# 27. Test Naming / Organization

Recommended additions:

```text
golden_test_premult_exact
golden_test_sampler_errors
golden_test_bilinear_exact
golden_test_address_modes
golden_test_clip_exact
golden_test_palette_exact
golden_test_dither_exact
golden_test_target_formats
golden_test_ext_faults

golden_test_random_core_diff
golden_test_random_scale_diff
golden_test_random_clip_diff
golden_test_random_palette_diff
golden_test_random_bilinear_diff
```

Exact grouping can vary.

---

# 28. Required Build / Test Commands

At minimum:

```bash
python scripts/preflight.py

cmake -S . -B build/stage003r
cmake --build build/stage003r

ctest --test-dir build/stage003r --output-on-failure

python tools/fixture_validate/fixture_validate.py model/golden/tests/frames
python scripts/check_test_integrity.py

git status --short
```

All must pass.

---

# 29. Cross-Compiler

If clang++ is already installed, perform one additional configure/build/test.

Do not install it automatically.

Record the result.

---

# 30. Optional Sanitizers

If locally supported:

```text
ASan
UBSan
```

Run at least unit + directed tests.

Because this task specifically closes UB/bounds issues, sanitizer evidence is valuable.

If unavailable on the Windows host, record `NOT RUN` with reason.

---

# 31. Required Stage Report

Create:

```text
docs/reports/REPORT_003R_Golden_Sprite_Pipeline_Correctness_Closure.md
```

Required sections:

```markdown
# REPORT_003R — Golden Sprite Pipeline Correctness Closure

## 1. Result

## 2. START_COMMIT
f55f5ad8209e9badc4681b383b66ac1c4a75b744

## 3. END_COMMIT
<exact SHA>

## 4. REVIEW_003 Closure Table
C1 ... C15

## 5. Premult Arithmetic Proof
Show equations and directed vectors.

## 6. Memory Fault Propagation

## 7. BLIT_EXT / Clip / Axis Semantics

## 8. SurfaceView Validation

## 9. Extension Fault Priority

## 10. Q16 Boundary Resolution

## 11. Regression Integrity
No tautological tests.

## 12. Differential Test Suites
Q/Core, Scale, Clip, Palette, Bilinear.

## 13. Feature Oracle Matrix

## 14. Changed Golden Fixtures
Why each changed.

## 15. CTest Result

## 16. Fixture Validation

## 17. Test Integrity Script Result

## 18. Cross-Compiler / Sanitizer

## 19. Blockers

## 20. Design Questions

## 21. Specification Changes
Normally NONE.

## 22. Suggested Next Stage
Stage 004 Tile Renderer.
```

---

# 32. Acceptance Criteria

TASK_003R passes only if all are true:

## Core correctness

- [ ] Premult formula is bit-exact to Pixel Arithmetic V0.1.
- [ ] Color Mod is applied exactly once.
- [ ] Premult destination attenuation uses `255-Aeff`.
- [ ] All source/palette read errors propagate.
- [ ] Palette address overflow/unmapped is detected.
- [ ] BLIT_EXT does not imply Clip.
- [ ] Builder does not force Clip.
- [ ] BLIT_EXT cross terms are rejected regardless of Strict.
- [ ] SurfaceView command stride/format validation is complete.
- [ ] extension/header fault priority is deterministic.
- [ ] extension Reserved handling matches Strict semantics.
- [ ] Q16/SRC_X boundary has no UB.
- [ ] FILL extension/Clip behavior is explicit.
- [ ] no-Clip target-bound behavior is explicit.

## Verification integrity

- [ ] no tautological mutation assertions remain.
- [ ] test-integrity script passes.
- [ ] mutation tests have concrete expected outcomes.
- [ ] core blend/key random differential compares pixels.
- [ ] nearest-scale random differential compares pixels.
- [ ] clip random differential compares pixels.
- [ ] palette random differential compares pixels.
- [ ] bilinear random differential compares pixels.
- [ ] Repeat is independently verified.
- [ ] scaled Flip X/Y/XY is verified.
- [ ] Premult has independent exact vectors.
- [ ] Dither has independent exact 4×4 vectors.
- [ ] destination formats have exact byte tests.
- [ ] all Stage 001/002 permanent regressions still pass.

## Repository

- [ ] normal build/test does not modify checked references.
- [ ] only justified incorrect fixtures are regenerated.
- [ ] fixture validator passes.
- [ ] CTest complete suite passes.
- [ ] frozen specs remain unmodified unless a documented DESIGN_QUESTION was resolved through reviewer approval.
- [ ] REPORT_003R contains exact END_COMMIT.

---

# 33. Expected Exit State

After this task:

```text
Immediate Golden Sprite Pipeline
    functional coverage      complete for planned competition 2D path
    numerical correctness    independently demonstrated
    ISA authority            demonstrated
    memory fault behavior    deterministic
    regression integrity     trustworthy
    fixture oracle policy    trustworthy
```

At that point it is safe for:

```text
Tile Renderer
→ compare against Immediate Golden
```

---

# 34. Next Stage After Pass

Only after REVIEW_003R = PASS:

> **TASK_004 — Golden Tile Renderer, Software Binning, WorkList & Immediate-vs-Tile Pixel-Exact**

Do not implement any Tile code in TASK_003R.
