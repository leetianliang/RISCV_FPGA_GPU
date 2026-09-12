# TASK_003 — Golden Feature-Complete Sprite Pipeline

> Project: RISC-V–FPGA General-Purpose 2D GPU  
> Stage ID: `003`  
> Stage name: **Golden Feature-Complete Sprite Pipeline**  
> Review granularity: one formal review after the whole stage  
> Executor: Local AI Agent  
> Reviewer: Project Architect / ChatGPT  
> Entry decision: `REVIEW_002 = PASS WITH ACTIONS`  
> Baseline commit: `d11c80110e39b7409f8aad65c00dc30572cb123a`  
> Priority: P0/P1  
> Mandatory: **Correction Block 0 must pass before new BLIT_EXT/scaling features are implemented**

---

# 1. Stage Objective

Finish the immediate-mode Golden sprite/pixel path to the point where it can serve as the authoritative reference for later Tile RTL/software work.

At Stage-003 exit, the Golden GPU should support:

```text
FILL_RECT
BLIT
BLIT_EXT

RGB565
ARGB8888
XRGB8888
INDEX8 + Palette

COPY
Straight Alpha
Premultiplied Alpha
Additive

Color Key
Color Modulate
Global Alpha
Per-Pixel Alpha

Nearest
Bilinear

Scale Up / Down
Flip X / Y
Clamp / Repeat
Clip Extension

RGB565 Ordered Dither

RGB565 / ARGB8888 / XRGB8888 Render Targets
```

The stage must preserve the central principle:

> command bytes define architectural behavior; harness metadata only supplies backing-resource bounds and test setup.

After this stage, the next major architectural milestone will be Tile-Based rendering.

---

# 2. Authoritative Specifications

Read and obey:

1. `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`
2. `RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md`
3. `RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md`
4. `RISC-V_FPGA_2D_GPU_Golden_GPU_Software_Architecture_V0.1.md`
5. `RISC-V_FPGA_2D_GPU_Verification_Plan_V0.1.md`
6. `RISC-V_FPGA_2D_GPU_Register_Map_Memory_Map_V0.1.md`
7. `REVIEW_002_Golden_2D_Core_BLIT_Alpha.md`
8. `AGENTS.md`

Do not modify frozen specification semantics.

If a conflict appears:

```text
DESIGN_QUESTION
```

and stop only the affected feature.

---

# 3. Explicit Non-Goals

Do NOT implement in Stage 003:

- Tile Binning;
- TILE_FRAME execution;
- Tile Buffer;
- Texture Cache;
- RLE/decompression;
- performance/cycle model;
- Affine/Mode-7;
- triangle rasterization;
- Z-buffer;
- Command Ring timing;
- FPGA RTL;
- board support;
- GUI framework.

Do not optimize Golden with SIMD or threading.

---

# 4. Correction Block 0 — REVIEW_002 Closure

Complete all items in this block before feature expansion.

---

## 4.1 Make command stride/format architecturally authoritative

Refactor resource/surface handling.

Recommended conceptual split:

```text
RegisteredResource
    base
    allocation_size
    width
    height
    optional debug name

Command Surface State
    base
    stride
    format

Execution SurfaceView
    RegisteredResource bounds
    +
    command stride/format
```

Equivalent architecture is allowed.

Required behavior:

- command `SRC_STRIDE` controls source row addressing;
- command `DST_STRIDE` controls destination row addressing;
- command `SRC_FORMAT` controls source decode;
- command `DST_FORMAT` controls render-target encode;
- registered harness metadata must not silently override those fields.

Validation must ensure the command-defined view fits in the registered allocation.

Mutation tests are mandatory.

---

## 4.2 Correct source coordinate type

Implement:

```text
SRC_X/Y = uint16
DST_X/Y = int16
```

Parser tests:

```text
0x7FFF → 32767
0x8000 → 32768
0xFFFF → 65535
```

Do not allocate huge textures only to test parsing.

Builder API should expose source coordinates as unsigned architectural values.

---

## 4.3 Fix and validate checked fixture metadata

Update the checked-in `fill_basic/manifest.json` base to the actual framebuffer base.

Create:

```text
tools/fixture_validate/fixture_validate.py
```

Python standard library only.

It should validate at least:

- manifest required fields;
- framebuffer raw size;
- command stream multiple-of-64;
- manifest command count;
- command destination base/stride/format against manifest where applicable;
- texture/palette file size vs declared layout.

Run this over every checked fixture in CTest or a dedicated validation target.

---

## 4.4 Restore monotonic regression coverage

Restore all Stage-001 directed behavior that was removed or replace it with explicit equivalent coverage.

Create a short mapping in REPORT_003:

```text
old regression
→ current regression
```

Do not reduce prior valid coverage.

---

## 4.5 Upgrade random regression to verify pixels

Implement an independent test oracle for a supported subset.

Preferred:

```text
simple_reference_renderer.cpp
```

inside test code only.

Rules:

- must not call production `GoldenGPU::execute_draw`;
- must not call the production pixel pipeline to compute expected pixels;
- can reuse frozen primitive formulas only if expected calculations remain independently structured;
- compare final RAW framebuffer bytes.

At minimum differential-random test:

- FILL COPY;
- BLIT COPY;
- RGB565;
- Global Alpha Straight;
- Additive;
- Color Key;
- randomized rectangles/stride padding;
- fixed seeds.

The test must fail on pixel mismatch.

---

## 4.6 Correct reserved-vs-unsupported fault classification

Implement reusable classification for:

### Pixel Format

```text
RGB565/ARGB8888/XRGB8888/INDEX8 = defined
0x4..0xF = BAD_FORMAT
```

### Filter

```text
NEAREST/BILINEAR = defined
0x2/0x3 = BAD_FILTER
```

### Blend

```text
COPY/STRAIGHT/PREMULT/ADD/MULTIPLY/XOR = defined
0x6..0xF = BAD_BLEND
```

### Address Mode

```text
CLAMP/REPEAT = defined
MIRROR reserved in V0.1
other reserved
```

Use exact fault semantics from the specs.

---

## 4.7 Correct XRGB write

XRGB8888 write must produce:

```text
0xFFRRGGBB
```

Little-endian bytes:

```text
BB GG RR FF
```

Add exact byte tests.

---

## 4.8 Make signed rounding helper truly overflow-safe

Rewrite `round_div_signed()` using quotient/remainder or another proof-safe formulation.

Must be correct for:

```text
INT64_MIN / 1
INT64_MIN / 2
INT64_MAX / 1
± ties
± non-ties
```

No signed overflow, no implementation-dependent unsigned-to-signed conversion for out-of-range values.

Expected values in tests must be constants or independently derived, not another call to the same function.

---

## 4.9 Complete Stage-002 missing acceptance tests

Add explicit:

- non-tight source stride;
- non-tight destination stride;
- command stride mutation;
- format mutation;
- alpha values 0/1/127/128/254/255;
- Global × Pixel Alpha staged rounding;
- validation fault leaves framebuffer unchanged where validation occurs pre-render.

---

## 4.10 Tooling cleanup

- remove unused CMake fixture helper;
- reject unknown CLI pixel-format strings;
- ensure report END_COMMIT is concrete;
- ensure final report statements match final HEAD.

---

## 4.11 Correction Block gate

Run:

```bash
cmake -S . -B build/stage003
cmake --build build/stage003
ctest --test-dir build/stage003 --output-on-failure
python tools/fixture_validate/fixture_validate.py model/golden/tests/frames
git diff -- model/golden/tests/frames
```

Expected:

- all tests PASS;
- normal build/test does not modify checked references;
- fixture validation PASS.

Only then begin Group A below.

---

# 5. Group A — Surface / Texture Architectural Refactor

Create a stable abstraction that later Tile mode can reuse.

Desired concepts:

```text
Resource Bounds
SurfaceView
TextureView
RenderTargetView
```

Avoid exposing host pointers.

All accesses ultimately use `MemoryImage`.

The interface should make it possible for Immediate and future Tile RT adapters to share the same Pixel Backend semantics.

Do not reproduce RTL ready/valid timing.

---

# 6. Group B — BLIT_EXT Extension Descriptor

Implement `BLIT_EXT` exactly from Command ISA V0.1.

Requirements:

- Base command remains 64B;
- `H_EXT_VALID=1`;
- `EXT_PTR` 64B aligned;
- extension fetched from `MemoryImage`;
- explicit little-endian descriptor parsing;
- extension type/version/length validation;
- Clip fields;
- signed Q16.16 U/V fields;
- no host struct packing dependency.

Do not invent extension fields.

---

## 6.1 Extension binary fixtures

Fixtures using BLIT_EXT must include:

```text
commands.bin
extensions.bin
...
```

with physical addresses matching command `EXT_PTR`.

The fixture runner must load extension bytes at the specified physical address.

---

# 7. Group C — Q16.16 UV and Scaling

Implement exact Pixel Arithmetic V0.1 semantics.

BLIT_EXT axis-aligned mapping:

```text
u = U0 + i*DU_DX
v = V0 + j*DV_DY
DV_DX = 0
DU_DY = 0
```

If the ISA supports all four terms in the extension, validate BLIT_EXT's axis-aligned restriction exactly.

---

## 7.1 Pixel-center mapping

Builder/helper for ordinary scale must use the frozen pixel-center convention:

```text
u(i)=SRC_X - 0.5 + (i+0.5)*SRC_W/DST_W
v(j)=SRC_Y - 0.5 + (j+0.5)*SRC_H/DST_H
```

Q16.16 coefficient construction must use the frozen signed round-nearest / tie rule.

No floating point in authoritative coefficient generation.

---

## 7.2 Scaling cases

Directed tests:

```text
1→1
1→N
N→1
2× upscale
non-integer upscale
2× downscale
non-integer downscale
odd sizes
```

---

# 8. Group D — Nearest Sampler

Implement:

```text
nearest_index = floor(coord + 0.5)
```

with exact Q16.16 behavior.

Half-way cases must choose the larger integer exactly as specified.

Test negative and positive Q16 values at parser/helper level even where final clipped sampling stays valid.

---

# 9. Group E — Texture Address Modes

Implement source-rectangle-relative:

```text
CLAMP
REPEAT
```

Address domain:

> source rectangle, not the entire texture/sheet.

Tests:

- left/right/top/bottom;
- one texel beyond;
- multiple periods for Repeat;
- non-zero `SRC_X/Y`.

Reserved address modes must fault appropriately.

---

# 10. Group F — Bilinear Sampler

Implement exact four-sample bilinear:

```text
x0=floor(u)
y0=floor(v)
fx=q[15:0]
fy=q[15:0]

C0 = LERP16(C00,C10,fx)
C1 = LERP16(C01,C11,fx)
C  = LERP16(C0,C1,fy)
```

Requirements:

- horizontal rounding first;
- vertical rounding second;
- interpolate all RGBA channels;
- address mode applied to each neighbor;
- no floating point.

Directed tests must include values where changing rounding order changes the result.

---

# 11. Group G — Flip X / Y

Implement BLIT and/or BLIT_EXT flip semantics exactly as frozen.

Tests:

```text
flip X
flip Y
flip XY
non-zero source origin
odd/even widths
```

Do not apply flip twice if coefficients already encode reversal.

Builder and executor responsibility must be clearly documented.

---

# 12. Group H — Clip Extension

Implement Clip with half-open rectangle semantics.

Required:

```text
actual raster = destination rect ∩ clip rect ∩ valid target bounds
```

Key rule:

> clipping does NOT reset UV mapping origin.

Therefore an originally offscreen/scissored pixel must not change the UV of remaining pixels.

Tests:

- clip left/right/top/bottom;
- empty clip;
- negative destination with valid Clip Extension;
- destination larger than target with clip;
- scaled BLIT clipped in the middle;
- compare clipped output against crop of unclipped reference.

---

# 13. Group I — Indexed8 + Palette

Implement source `INDEX8`.

Palette:

```text
256 × RGBA8888
PALETTE_ADDR physical memory
```

Rules:

- palette entries explicit LE `0xAARRGGBB`;
- Palette enable/state validated exactly;
- nearest: index fetch → palette decode;
- bilinear: fetch 4 indices → palette decode each → RGBA bilinear;
- never interpolate index values.

Fixtures include:

```text
texture_index8.raw
palette.bin
```

---

# 14. Group J — Color Modulate

Implement:

```text
S'r = MUL8_RN(Sr, Mr)
S'g = MUL8_RN(Sg, Mg)
S'b = MUL8_RN(Sb, Mb)
```

where:

```text
M = PRIMARY_COLOR
```

Color Key must happen before Color Mod.

Mod alpha participates as an extra opacity factor in exact staged Effective Alpha:

```text
source alpha
→ mod alpha
→ global alpha
→ coverage
```

Test a case where one-shot multiplication would differ from staged rounding.

---

# 15. Group K — Premultiplied Alpha

Implement `BLEND_PREMULT_ALPHA` according to Pixel Arithmetic V0.1.

Key requirement:

> source RGB already contains source-alpha multiplication; do not apply source alpha twice.

Extra opacity factors such as:

- Color Mod Alpha;
- Global Alpha;
- Coverage

must be handled exactly as specified.

Tests:

- A=0;
- A=128;
- A=255;
- global opacity;
- compare straight vs correctly premultiplied equivalent source.

---

# 16. Group L — RGB565 Ordered Dither

Implement the frozen 4×4 Bayer dither for RGB565 destination.

Use global render-target:

```text
x,y
```

not tile-local/source coordinates.

Tests:

- 4×4 exact expected pattern;
- translated rectangle proving global coordinate indexing;
- dither disabled exact regression;
- strict invalid format/state behavior for non-RGB565 destination;
- non-strict behavior according to spec.

---

# 17. Group M — Destination Formats

Complete render-target semantics for:

```text
RGB565
ARGB8888
XRGB8888
```

Rules:

- ARGB8888 logical bits preserved;
- XRGB read A=255;
- XRGB write X=0xFF;
- INDEX8 destination remains unsupported unless the specification explicitly requires it.

Test COPY, Straight Alpha and at least one scaled path into each supported destination format.

---

# 18. Group N — Shared Pixel Pipeline Order

By Stage-003 exit the canonical order must be explicit in code and tests:

```text
Texture / Constant
→ Format / Palette Decode
→ Sampling
→ Color Key
→ Color Mod
→ Effective Alpha
→ Destination Fetch
→ Blend
→ Dither / RT Format Convert
→ Write
```

Depth remains bypassed.

Do not allow front-end-specific copies of blend arithmetic.

---

# 19. Group O — BLIT vs BLIT_EXT Equivalence

For a 1:1 nearest draw without extension-only effects:

```text
BLIT framebuffer == BLIT_EXT framebuffer
```

Create randomized equivalence tests.

This is important because hardware will likely have separate fast and general front-ends sharing the backend.

---

# 20. Group P — Immediate Reference Fixtures

Keep all previous fixtures.

Add at least:

```text
blit_ext_scale_nearest/
blit_ext_scale_bilinear/
blit_flip_xy/
blit_clip/
indexed8_palette/
indexed8_bilinear/
color_mod/
premult_alpha/
rgb565_dither/
argb8888_target/
```

Each fixture must pass `fixture_validate.py`.

Reference files remain immutable under ordinary build/test.

---

# 21. Group Q — Independent Differential Random Testing

Extend the independent simple renderer / oracle.

Required random suites:

### Suite Q1 — BLIT/Fill core
Stage-002 supported subset.

### Suite Q2 — Nearest scaling
Random source/destination rectangles and valid scales.

### Suite Q3 — Clip
Random clip rectangles including empty/partial.

### Suite Q4 — Palette
Random Indexed8 texture + deterministic palette.

### Suite Q5 — Bilinear
Smaller case count, exact pixel compare.

Every failure prints:

```text
seed
case index
command index
x/y first mismatch
expected
actual
```

Seeds are fixed and reported.

---

# 22. Group R — Mutation Tests for Command Authority

Create tests designed specifically to catch ignored ISA fields.

Starting from a valid command, mutate one field at a time:

```text
SRC_STRIDE
DST_STRIDE
SRC_FORMAT
DST_FORMAT
SRC_X
SRC_Y
DST_X
DST_Y
FILTER
ADDR_MODE_U
ADDR_MODE_V
GLOBAL_ALPHA
COLOR_KEY
PALETTE_ADDR
EXT_PTR
```

Each mutation must either:

- produce the corresponding defined behavior; or
- produce the correct specified fault.

This group is mandatory before Tile implementation.

---

# 23. Group S — CLI / Fixture Runner Generalization

`golden_cli run-fixture <dir>` is strongly recommended.

The fixture manifest should describe:

```text
framebuffer resource
texture resources
palette
extension blocks
commands
expected output
```

The runner should load resources from manifest rather than requiring long CMake argument lists.

Python JSON parsing can be used in an outer runner, or C++ may retain explicit args if avoiding a JSON dependency. Do not add a third-party JSON library.

If a manifest-driven C++ runner would require an undesirable parser, a Python standard-library orchestrator invoking `golden_cli` is acceptable.

---

# 24. Group T — Frame Compare Diagnostics

Preserve exact RAW compare as the authoritative criterion.

Enhance diagnostics for supported formats:

```text
first mismatch byte
pixel x/y
expected logical color
actual logical color
```

For RGB565 decode using the frozen arithmetic.

Optional diff images remain out of scope unless generated with a dependency-free simple format.

---

# 25. Group U — Test Count / Coverage Manifest

Create:

```text
docs/testing/GOLDEN_REGRESSION_MATRIX.md
```

or equivalent.

Track permanent regression categories:

```text
Arithmetic
Command Decode
Memory
Formats
FILL
BLIT
BLIT_EXT
Key
Alpha
Additive
Premult
Scaling
Nearest
Bilinear
Clip
Flip
Palette
Dither
Fixtures
Random Differential
```

This file exists to prevent future agents from deleting older coverage during refactors.

Do not list tests that do not exist.

---

# 26. Build and Verification

Required:

```bash
python scripts/preflight.py

cmake -S . -B build/stage003
cmake --build build/stage003
ctest --test-dir build/stage003 --output-on-failure

python tools/fixture_validate/fixture_validate.py model/golden/tests/frames
git status --short
```

Normal build/test must leave reference fixtures unchanged.

If clang++ is available locally, run a second configure/build.

---

# 27. Sanitizer Run

If supported by the local GCC/Clang environment, add a non-default optional configuration for:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
```

Run the Golden unit/directed suite once.

Do not make sanitizers mandatory on unsupported Windows toolchains.

Record result in report.

---

# 28. Code Quality Requirements

- no float in authoritative pixel/UV arithmetic;
- no compiler bitfield serialization;
- no host-endian binary dependence;
- no signed-overflow UB;
- no silent fallback for invalid enums;
- no hidden state that changes draw semantics;
- no production helper used as the sole expected-value oracle for the same function under test;
- command fields must not be decoded and then ignored;
- deterministic seeds and artifacts.

---

# 29. Stage Report

Create:

```text
docs/reports/REPORT_003_Golden_Feature_Complete_Sprite_Pipeline.md
```

Required structure:

```markdown
# REPORT_003 — Golden Feature-Complete Sprite Pipeline

## 1. Result

## 2. START_COMMIT / END_COMMIT

## 3. REVIEW_002 Correction Closure
B1 ... B11, one row each.

## 4. Architectural Refactor
Explain command authority vs resource bounds.

## 5. Feature Matrix

## 6. BLIT_EXT / Extension Descriptor

## 7. Scaling / Q16.16

## 8. Nearest / Bilinear

## 9. Address Modes

## 10. Flip / Clip

## 11. Indexed8 / Palette

## 12. Color Mod / Alpha / Premult

## 13. Dither / Target Formats

## 14. Regression Preservation
Map Stage-001/002 tests to current tests.

## 15. Directed Test Results

## 16. Differential Random Results
Seeds/case counts.

## 17. Fixture Validation

## 18. Fixture Integrity
Prove ordinary build/test did not modify references.

## 19. BLIT-vs-BLIT_EXT Equivalence

## 20. CTest / Build Commands

## 21. Cross-Compiler / Sanitizer

## 22. Warnings / Limitations

## 23. Blockers

## 24. Design Questions

## 25. Deviations

## 26. Suggested Next Stage
```

---

# 30. Acceptance Criteria

Stage 003 passes only if:

### REVIEW_002 closure
- [ ] command stride/format are authoritative;
- [ ] source coordinates are uint16 semantics;
- [ ] fill_basic manifest is actually corrected;
- [ ] old regression coverage restored/equivalent;
- [ ] random regression checks pixels;
- [ ] reserved/unsupported faults distinguished;
- [ ] XRGB writes `0xFF`;
- [ ] `round_div_signed` is fully overflow-safe;
- [ ] missing stride/alpha tests added;
- [ ] tooling/report cleanup complete.

### Extended sprite path
- [ ] BLIT_EXT parses real extension bytes;
- [ ] Q16.16 scale mapping exact;
- [ ] nearest exact;
- [ ] bilinear exact;
- [ ] Clamp exact;
- [ ] Repeat exact;
- [ ] Flip X/Y exact;
- [ ] Clip exact and UV origin preserved;
- [ ] Indexed8 nearest exact;
- [ ] Indexed8 bilinear decodes palette before interpolation;
- [ ] Color Mod exact;
- [ ] mod alpha staged correctly;
- [ ] Premultiplied Alpha exact;
- [ ] RGB565 ordered dither exact;
- [ ] RGB565/ARGB8888/XRGB8888 targets conform;
- [ ] BLIT and equivalent BLIT_EXT match;
- [ ] mutation tests prove ISA fields are not ignored;
- [ ] immutable fixtures validated;
- [ ] differential random suites pass;
- [ ] all prior permanent regressions pass;
- [ ] CTest full suite PASS;
- [ ] frozen specs unmodified;
- [ ] report complete.

---

# 31. Expected Exit State

```text
Immediate Golden 2D/Sprite pipeline       feature-complete for competition work
Command authority                         corrected
FILL                                      stable
BLIT                                      stable
BLIT_EXT                                  implemented
RGB565/ARGB/XRGB                          implemented
INDEX8 + Palette                          implemented
Color Key                                 implemented
Color Mod                                 implemented
Global / Pixel Alpha                      implemented
Straight / Premult / Additive             implemented
Scale                                     implemented
Nearest / Bilinear                        implemented
Clamp / Repeat                            implemented
Flip                                      implemented
Clip                                      implemented
RGB565 Dither                             implemented
Independent differential regression       established
Reference fixture discipline              established

Tile Renderer                             not started
Texture Cache                             not started
Affine / Mode-7                           not started
RTL                                       not started
```

---

# 32. Expected Next Stage

Subject to REVIEW_003:

# **Stage 004 — Golden Tile Renderer, WorkList/Binning & Immediate-vs-Tile Pixel-Exact Verification**

Expected focus:

```text
Draw Descriptor Array
Tile Header
WorkRef
CPU Software Binner prototype
Tile load/render/store semantics
32×32 default Tile
Immediate == Tile pixel-exact
Tile-size sweep instrumentation
Overdraw / memory-traffic profiling
```

Do not begin Stage 004 until Stage 003 review.
