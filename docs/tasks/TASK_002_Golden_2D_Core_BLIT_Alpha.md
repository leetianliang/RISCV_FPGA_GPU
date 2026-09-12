# TASK_002 — Golden 2D Core: BLIT, Texture Formats, Color Key & Alpha

> Project: RISC-V–FPGA General-Purpose 2D GPU  
> Stage ID: `002`  
> Review granularity: **one formal review after the whole stage**  
> Entry decision: `REVIEW_001 = PASS WITH ACTIONS`  
> Baseline reviewed HEAD: `ed4a593d378f8d16046f8463ef04de68fed1ddbf`  
> Mandatory rule: **Correction Block 0 must pass before new feature work**

## 1. Stage goal

Turn the narrow Stage-001 FILL implementation into the first reusable 2D rendering pipeline:

```text
Command ISA
    ↓
Normalized 2D draw state
    ↓
Source texel fetch / format decode
    ↓
Color Key
    ↓
Alpha construction
    ↓
Destination read
    ↓
Blend
    ↓
DST-format quantization
    ↓
Render target write
```

At exit, binary command streams must support `FILL_RECT` and `BLIT` with RGB565/ARGB8888/XRGB8888 source formats, COPY, Color Key, Global Alpha, Per-Pixel Alpha, Straight Alpha and Saturating Additive Blend. Destination RGB565 is mandatory.

Do not implement BLIT_EXT, scaling, bilinear, Indexed8/Palette, Tile, Affine, Triangle, cycle-accurate modeling, RTL or FPGA board logic.

## 2. Authoritative documents

Read and obey:

1. `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`
2. `RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md`
3. `RISC-V_FPGA_2D_GPU_Golden_GPU_Software_Architecture_V0.1.md`
4. `RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md`
5. `RISC-V_FPGA_2D_GPU_Verification_Plan_V0.1.md`
6. `RISC-V_FPGA_2D_GPU_Register_Map_Memory_Map_V0.1.md`
7. `docs/reviews/REVIEW_001_Golden_Foundation_and_FILL.md`
8. `AGENTS.md`

Frozen specifications must not be edited.

# 3. Correction Block 0 — complete first

## 3.1 Make Golden references immutable during normal build/test

Normal `cmake --build` and `ctest` must never modify checked-in files under `model/golden/tests/frames/`.

Reference generation must be explicit/opt-in. Current-run actual outputs go under the build tree. Add evidence that a normal build/test leaves checked-in references unchanged.

## 3.2 Explicit little-endian command serialization

Add explicit 64B LE serialization/deserialization helpers. Do not use raw object-memory `memcpy` as the file format. Test known byte patterns, full round trip and wrong input size.

## 3.3 Synchronize `FaultCode`

Match the frozen register-map values through `BAD_ADDRESS`. Add representative numeric assertions/tests, especially `BAD_RING_CONFIG`, `BAD_TILE_CONFIG`, `MEMORY_ERROR`, `BAD_ADDRESS`.

## 3.4 Correct `H_STRICT` behavior

Apply strict-only checks only when the ISA says strict. Add strict/non-strict paired tests for Reserved fields and applicable `EXT_PTR` behavior.

## 3.5 Correct opcode/capability classification

Required behavior:

```text
known + implemented       -> execute
known + unimplemented     -> UNSUPPORTED_FEATURE
reserved/undefined opcode -> BAD_OPCODE
undefined class           -> BAD_CMD_CLASS
```

At Stage-002 exit: FILL and BLIT implemented; BLIT_EXT/TILE_FRAME recognized but unsupported; DRAW_2D `0x11` remains bad opcode.

## 3.6 Harden `MemoryImage`

- reject region end beyond `2^32`;
- distinguish `UNMAPPED` from `OUT_OF_RANGE`;
- do not span adjacent regions in one scalar/block access;
- add exact boundary tests.

Required examples:

```text
base=0xFFFFFFF0 size=0x10 -> legal
base=0xFFFFFFF0 size=0x11 -> reject
```

## 3.7 Make `round_div_signed` overflow-safe

Preserve round-to-nearest, ties-away-from-zero without `INT64_MIN` negation overflow. Add boundary-oriented tests.

## 3.8 Correct fixture metadata

Make `fill_basic` manifest use the actual framebuffer base/stride/etc. Avoid duplicated magic values that can disagree with commands.

## 3.9 Remove hidden Pillow requirement

Remove or replace the Pillow preview script with a standard-library-only path, or defer it to a later visualization-tool stage. Required Golden build/test remains standard-library-only.

## 3.10 Update Golden README

Document current build, CTest, FILL support, fixture policy and Stage-002 profile.

## 3.11 Correction checkpoint

Before new features:

```bash
cmake -S . -B build/stage002
cmake --build build/stage002
ctest --test-dir build/stage002 --output-on-failure
git status --short
```

Checked-in references must remain unchanged. Recommended checkpoint commit:

`fix(golden): close REVIEW_001 foundation actions`

# 4. Common 2D draw architecture

Refactor toward:

```text
64B command bytes
→ LE deserialize
→ header classifier
→ common 2D payload decode
→ normalized draw state
→ FILL or BLIT front-end
→ shared pixel pipeline
→ render target
```

Create an internal normalized state carrying the relevant source/destination bases, strides, XY/WH, formats, blend/filter flags, Color Key/Alpha flags, primary color, global alpha and key RGB. This is internal only; do not invent a new ISA.

Existing Stage-001 FILL behavior must remain bit-exact.

# 5. Generic format support

Required source decode:

- RGB565 -> bit replication, A=255;
- ARGB8888 logical `0xAARRGGBB`, little-endian bytes `B,G,R,A`;
- XRGB8888 -> RGB preserved, A=255.

Provide one audited bytes-per-pixel helper. `INDEX8=1` may be represented in the enum/helper but Indexed8 rendering remains unsupported.

For known-width surfaces, reject `stride < width * BPP`; use wide arithmetic for size/address validation.

# 6. BLIT command

Implement DRAW_2D opcode `0x01` using the common payload.

Stage-002 BLIT restrictions:

```text
SRC_W == DST_W
SRC_H == DST_H
FILTER = NEAREST
H_EXT_VALID = 0
```

Supported features:

- COPY;
- Color Key;
- Global Alpha;
- Per-Pixel Alpha;
- Straight Alpha;
- Saturating Additive Blend.

Deferred features such as FLIP, Palette, Premultiplied Alpha, Color Mod, Dither, Bilinear and Clip must return explicit unsupported/fault behavior rather than silently degrading.

# 7. Source address generation

For 1:1 BLIT:

```text
sx = SRC_X + local_x
sy = SRC_Y + local_y
addr = SRC_BASE + sy * SRC_STRIDE + sx * BPP
```

Use wide intermediate arithmetic and the physical-memory abstraction. Tests use valid in-bounds source rectangles.

If source/destination overlap semantics are not defined by the authoritative specs, do not invent memcpy/memmove semantics; use non-overlapping tests and report a `DESIGN_QUESTION` or reject overlap as unsupported.

# 8. Shared pixel pipeline

Implement an auditable scalar path with the specified order:

```text
Source format decode
→ Color Key
→ Alpha factor construction
→ Destination read when required
→ Blend
→ Destination format quantization
→ write
```

Do not duplicate separate blend math inside FILL and BLIT.

# 9. Color Key

Compare canonical decoded RGB to `COLOR_KEY_RGB`; ignore source alpha. Key comparison occurs before alpha/blend. Hit means discard and destination unchanged.

Tests must cover RGB565/ARGB8888/XRGB8888 hit/miss and interaction with alpha flags.

# 10. Effective Alpha

Follow the exact staged Pixel Arithmetic V0.1 sequence. For Stage 002, coverage is 255 and Color-Mod factor remains 255 because Color Mod is deferred.

Required semantics:

- `PIXEL_ALPHA_EN=0` -> source alpha factor 255;
- `PIXEL_ALPHA_EN=1` -> decoded source A;
- `GLOBAL_ALPHA_EN=0` -> global factor 255;
- `GLOBAL_ALPHA_EN=1` -> W14 global alpha.

Do not collapse staged rounding into a different one-shot formula.

# 11. COPY

COPY follows Pixel Arithmetic V0.1. Alpha flags must not accidentally convert COPY into alpha blending. For RGB565 target, source is quantized to RGB565 on write.

# 12. Straight Alpha

Implement exact `BLEND_STRAIGHT_ALPHA`, `PREMULT_SRC=0` semantics.

For RGB565 destination:

```text
read RGB565
→ decode RGBA(A=255)
→ exact source-over blend
→ encode RGB565
→ write
```

Repeated overdraw must therefore use the previously quantized destination value.

Boundary tests must include effective alpha 0,1,127,128,254,255.

# 13. Additive Blend

Implement exact `BLEND_ADD_SAT`, `PREMULT_SRC=0` semantics. Test non-saturating cases, per-channel saturation, full saturation, global alpha and per-pixel alpha scaling.

# 14. FILL through shared pixel semantics

Keep COPY behavior and extend FILL to Straight Alpha and Additive where existing specs unambiguously define them. Use constant `PRIMARY_COLOR` as source and the same destination-read/blend/quantize path.

Do not implement Color Mod or Premultiplied Alpha in this stage.

# 15. Required source/feature matrix

| Source | COPY | Key | Global Alpha | Pixel Alpha | Straight | Additive |
|---|---:|---:|---:|---:|---:|---:|
| RGB565 | yes | yes | yes | A=255 | yes | yes |
| ARGB8888 | yes | yes | yes | yes | yes | yes |
| XRGB8888 | yes | yes | yes | A=255 | yes | yes |

Tests need not cover every cartesian combination, but every semantic branch must be exercised.

# 16. Multi-command execution

Add `execute_stream(...)` or equivalent for an ordered sequence of `GpuCmd64` commands.

Required:
- submission order preserved;
- stop on first architectural error in the simple functional model;
- earlier completed commands remain applied;
- result identifies at least failing command index + fault.

Binary command-stream files are `N * 64B`, explicitly LE; reject file sizes not divisible by 64. No Command-Ring timing simulation.

# 17. Deterministic binary fixtures

Keep Stage-001 `fill_basic` immutable and passing.

Add at least:

```text
blit_rgb565_basic/
blit_colorkey/
blit_alpha_argb8888/
blit_overdraw_rgb565/
blit_additive/
```

Each fixture should contain the needed command stream, framebuffer, texture resource(s), manifest and immutable `golden_fb.raw`. Manifest physical addresses/sizes/formats/strides must match actual commands/resources.

Normal build/CTest must not rewrite expected references.

# 18. Directed BLIT tests

Required cases:

1. RGB565 1x1 COPY
2. RGB565 rectangle COPY
3. source XY offset
4. non-tight source stride
5. non-tight destination stride
6. ARGB8888 source -> RGB565
7. XRGB8888 source -> RGB565
8. Color Key hit
9. Color Key miss
10. Global Alpha 0
11. Global Alpha 255
12. Global Alpha 128
13. Per-Pixel Alpha 0/128/255
14. combined Global x Pixel Alpha
15. Straight Alpha representative destination extremes
16. repeated RGB565 alpha overdraw proving per-write quantization
17. Additive without saturation
18. Additive with saturation
19. COPY remains COPY even when alpha flags are set
20. unsupported/invalid state never silently falls back

# 19. Decoder/capability tests

Required:

```text
FILL_RECT       implemented
BLIT            implemented
BLIT_EXT        known but unsupported
TILE_FRAME      known but unsupported
DRAW_2D 0x11   BAD_OPCODE
unknown class   BAD_CMD_CLASS
```

Also include the strict/non-strict pairs from Correction Block 0.

# 20. Direct pixel tests

Add pure-function tests for:

- effective alpha;
- straight-alpha channel/result;
- additive saturation;
- Color Key comparison;
- RGB565 / ARGB8888 / XRGB8888 source decode.

Expected results should be computed directly from the specification formulas rather than by calling the production function under test.

# 21. Deterministic random regression

Add a small fixed-seed Stage-002 random regression over small surfaces and supported in-bounds FILL/BLIT features.

Requirements:
- fixed recorded seed;
- tens to hundreds of command sequences;
- reproducible failure output;
- no current-time seed;
- independent formula/invariant checks where practical.

This is correctness regression, not performance stress.

# 22. CLI generalization

Refactor `run-fill` toward a generic fixture/stream runner. Exact syntax is local, but it must read explicit LE command streams and execute them through `GoldenGPU` without hidden direct rendering shortcuts.

No GUI required.

# 23. Frame compare improvement

Keep exact-byte compare. If convenient, add optional width/height/stride/RGB565 metadata to report first mismatching x/y and pixel values. Use Python standard library only.

Exact RAW equality remains formal PASS.

# 24. Test-integrity evidence

The report must explicitly demonstrate that ordinary build/test does not modify checked-in Golden references. Use hashes, `git diff --exit-code` on fixture paths, or an equivalent robust mechanism.

# 25. Build / test requirements

Required commands:

```bash
python scripts/preflight.py
cmake -S . -B build/stage002
cmake --build build/stage002
ctest --test-dir build/stage002 --output-on-failure
git status --short
```

Also run exact frame compare and intentional mismatch. A second clang++ build is strongly recommended if the compiler is already installed.

No third-party dependency may be introduced in the required Stage-002 Golden/test/tool path.

# 26. Documentation/status

Update `model/golden/README.md` and `docs/PROJECT_STATUS.md`.

At completion, status should say:

```text
Current Stage: Golden 2D Core — BLIT, Texture Formats, Color Key & Alpha
Last Completed Stage: 002 (pending reviewer approval)
Current Gate: Golden Stage 002 awaiting review
Next Planned Stage: Extended Sprite Path / BLIT_EXT / Scaling / Palette / Bilinear, pending review
```

Do not claim reviewer approval before review.

# 27. Suggested internal commits

Formal review remains once at stage end. Suggested checkpoints:

```text
fix(golden): close REVIEW_001 foundation actions
refactor(golden): normalize 2d draw state and command classification
feat(golden): add source formats and blit copy path
feat(golden): add color key and alpha pipeline
feat(golden): add additive and shared fill pixel path
feat(golden): add command stream and immutable fixtures
test(golden): add stage002 directed and random regression
docs: add Stage 002 report
```

# 28. Stage report

Create:

`docs/reports/REPORT_002_Golden_2D_Core_BLIT_Alpha.md`

It must include:

- Result
- START_COMMIT / END_COMMIT
- REVIEW_001 A1-A11 closure table
- architecture implemented
- supported feature matrix
- files added/modified
- specification compliance
- unit tests
- FILL regression
- BLIT directed results
- Key/Alpha/Additive results
- binary fixtures
- random seed/case count/results
- reference-integrity evidence
- exact build/CTest commands and exit codes
- optional cross-compiler result
- warnings/limitations
- blockers
- design questions
- task deviations
- suggested next stage

# 29. Acceptance criteria

Stage 002 passes only if:

- [ ] A1-A11 REVIEW_001 corrections are closed
- [ ] ordinary build/test cannot rewrite checked-in Golden references
- [ ] explicit LE command binary I/O is tested
- [ ] fault values match spec
- [ ] strict mode semantics match spec
- [ ] supported vs invalid opcode classification is correct
- [ ] MemoryImage boundary/status semantics are corrected
- [ ] RGB565 source BLIT works
- [ ] ARGB8888 source BLIT works
- [ ] XRGB8888 source BLIT works
- [ ] source/destination stride and XY work
- [ ] COPY works
- [ ] Color Key works
- [ ] Global Alpha works
- [ ] Per-Pixel Alpha works
- [ ] combined alpha uses exact staged rounding
- [ ] Straight Alpha works
- [ ] Additive works
- [ ] repeated RGB565 overdraw quantizes after every write
- [ ] FILL Stage-001 behavior does not regress
- [ ] multi-command binary stream executes in order
- [ ] BLIT_EXT/TILE_FRAME are recognized as unsupported rather than invalid
- [ ] deterministic fixtures pass exact RAW compare
- [ ] fixed-seed random regression passes
- [ ] no third-party core verification dependency
- [ ] full CTest suite passes
- [ ] frozen specs remain unmodified
- [ ] Stage report is complete

# 30. Expected exit state

```text
Golden foundation              stable
Reference oracle policy        immutable by default
Command binary I/O             explicit little-endian
Fault / strict semantics       synchronized
FILL                           shared pixel path
BLIT                           implemented
RGB565 / ARGB / XRGB source    implemented
Color Key                      implemented
Global / Per-Pixel Alpha       implemented
Straight Alpha                 implemented
Additive                       implemented
RGB565 destination             implemented
Multi-command stream           implemented
Deterministic regression       expanded
Scaling/Bilinear/Palette       not started
Tile                           not started
RTL                            not started
```

Expected next stage after review:

**Stage 003 — Extended Sprite Path: BLIT_EXT, Scaling, Flip, Clip, Indexed8/Palette, Color Mod and Bilinear**
