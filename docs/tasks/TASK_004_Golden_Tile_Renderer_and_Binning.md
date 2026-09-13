# TASK_004 — Golden Tile Renderer, Software Binning, WorkList & Immediate-vs-Tile Pixel-Exact

> Project: RISC-V–FPGA General-Purpose 2D GPU  
> Stage ID: `004`  
> Stage name: **Golden Tile Renderer, Software Binning, WorkList & Immediate-vs-Tile Pixel-Exact**  
> Review granularity: **one formal review after the complete stage**  
> Executor: Local AI Agent  
> Reviewer: Project Architect / ChatGPT  
> Entry decision: `REVIEW_003R_V3 = PASS WITH ACTIONS`  
> Baseline commit: `105c23c0989c74fcd6273b33181c6079e40f5d5e`  
> Priority: P0/P1  
> Status: READY  
> Formal next gate: **Tile Architecture Gate**

---

# 0. Completion Contract

This task uses a stricter completion model than previous stages.

A feature being implemented does **not** mean the requirement is complete.

For every Mandatory Acceptance ID in this document, the Agent must provide:

```text
Requirement
→ Implementation Evidence
→ Verification Evidence
→ Test Result
→ PASS
```

Rules:

> **No evidence = NOT DONE.**

> **Partial evidence = NOT DONE.**

> **“Covered by a similar test” without an exact mapping = NOT DONE.**

> **A test that only checks “no fault” is not pixel-correctness evidence unless the Acceptance ID explicitly asks only for fault behavior.**

> **A failing test may only be changed if the authoritative specification changed. Do not modify expected results merely to match the implementation.**

Before writing the final report, the Agent must re-read this entire task from the beginning and perform the Stage Self-Audit in Block 7.

If any Mandatory Acceptance ID has no concrete evidence, the Stage result must not be reported as PASS.

---

# 1. Stage Goal

Stage 004 introduces the first real Tile-Based rendering path.

The target architecture is:

```text
Immediate Draw Commands
        │
        ├───────────────► Immediate Golden Renderer
        │                         │
        │                         ▼
        │                 immediate_fb.raw
        │
        ▼
CPU Software Tile Binner
        │
        ├─ 64B Draw Descriptor Array
        ├─ Tile Header Array
        └─ WorkRef Array
                │
                ▼
         TILE_FRAME Command
                │
                ▼
       Golden Tile Renderer
                │
                ▼
           tile_fb.raw
                │
                ▼
       Exact Byte Comparison
```

The primary Stage-004 proof is:

> **For the same legal draw stream and initial framebuffer, Immediate Mode and Tile Mode must produce byte-identical final framebuffer output.**

This includes order-sensitive cases:

- Alpha;
- Additive;
- Color Key;
- Scaling;
- Bilinear;
- Palette;
- Dither;
- Clip;
- multiple overlapping sprites.

The Tile implementation must reuse the same sampling/pixel semantics as Immediate Mode.

The Tile path is not allowed to become a second independently coded GPU.

---

# 2. Frozen Architecture Decisions

The following decisions are already frozen and must not be silently changed:

```text
CPU software binning
FPGA/Golden Tile Renderer
Draw Descriptor = same 64B 2D draw encoding
Each submitted draw stored once in Draw Descriptor Array
WorkRef = 32-bit descriptor index
Per-tile WorkRefs preserve original global draw order
Tile Header / WorkList structures follow Command ISA V0.1 exactly
Default Tile size = 32×32
Golden architecture experiment may additionally run 16×16 and 64×64
Immediate and Tile share the same Pixel Backend semantics
Tile Compatibility Mode must be pixel-exact to Immediate Mode
Every logical RT write is quantized to DST_FORMAT before later draws can read it
```

Do not change any of these without a `DESIGN_QUESTION`.

---

# 3. Authoritative Specifications

Read before implementation:

1. `docs/RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md`
2. `docs/RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`
3. `docs/RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md`
4. `docs/RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md`
5. `docs/RISC-V_FPGA_2D_GPU_Golden_GPU_Software_Architecture_V0.1.md`
6. `docs/RISC-V_FPGA_2D_GPU_Verification_Plan_V0.1.md`
7. `docs/RISC-V_FPGA_2D_GPU_Register_Map_Memory_Map_V0.1.md`
8. `docs/reviews/REVIEW_003R_V3_Latest_Implementation.md`
9. `docs/testing/GOLDEN_REGRESSION_MATRIX.md`
10. `AGENTS.md`

The exact Tile Header / WorkRef / TILE_FRAME binary layouts must come from Command ISA V0.1.

Do not invent an easier alternate binary format.

---

# 4. Explicit Non-Goals

Do **not** implement in Stage 004:

- RTL Tile Renderer;
- FPGA synthesis;
- Command Ring timing;
- Texture Cache;
- RLE/decompression;
- Tile fast-clear optimization;
- Tile load elimination optimization;
- cycle-accurate performance model;
- Affine / Mode-7;
- Triangle;
- Z-buffer;
- multi-context scheduling;
- GUI viewer.

Correctness first.

For correctness, an active tile may always load its initial framebuffer pixels and store its valid final pixels.

Traffic optimization comes later.

---

# 5. Expected New Code Areas

Recommended structure:

```text
model/golden/include/golden/
├── tile_types.hpp
├── tile_binner.hpp
├── tile_renderer.hpp
└── tile_stats.hpp

model/golden/src/
├── tile_types.cpp
├── tile_binner.cpp
├── tile_renderer.cpp
└── tile_stats.cpp

model/golden/tests/tile/
├── test_tile_structures.cpp
├── test_tile_binner.cpp
├── test_tile_renderer.cpp
├── test_tile_equivalence.cpp
├── test_tile_faults.cpp
└── test_tile_random_diff.cpp

model/architecture/tile_model/
├── run_tile_sweep.py
├── workloads/
└── README.md

docs/testing/
└── TILE_REGRESSION_MATRIX.md

results/stage004_tile/
```

Exact file decomposition may differ.

Do not place Tile-specific pixel blend formulas in `tile_renderer.cpp`.

---

# 6. BLOCK 0 — Golden Verification Finalization

## Block Exit Rule

**Do not begin Tile pixel-equivalence implementation until all `GVF-*` IDs pass locally.**

Tile data-structure scaffolding may be prepared, but do not use Tile output as a correctness proof before Block 0 is green.

---

## GVF-01 — Remove tautological tests permanently

### Requirement

Fix the currently known always-true assertion in the extension/memory matrix.

Extend:

```text
scripts/check_test_integrity.py
```

so it also detects member-expression tautologies such as:

```cpp
!st.ok || st.ok
st.ok || !st.ok
```

and obvious equivalents where practical.

### Required Evidence

Implementation:

```text
scripts/check_test_integrity.py
```

Verification:

- intentionally create or simulate a forbidden sample and prove the checker rejects it;
- run checker over repository and receive PASS.

### Forbidden Shortcut

Do not simply delete the offending negative test.

Replace it with a deterministic expected result.

### Acceptance

`GVF-01 = PASS` only if no known tautological assertion remains and the checker catches the class of bug that escaped the previous regex.

---

## GVF-02 — Make nearest-scale oracle independent

### Requirement

The expected path in the scale differential must not call production helpers for:

```text
compute_axis_aligned_uv
compute_axis_aligned_uv_v
nearest_index_q16
```

Implement test-local formulas from Pixel Arithmetic V0.1.

Also genuinely test padded source stride.

### Required Cases

At minimum:

```text
1→N
N→1
upscale
downscale
non-integer ratio
odd size
nonzero SRC_X/Y
tight source stride
padded source stride
padded destination stride
```

### Acceptance

Actual framebuffer bytes exactly equal independently computed expected bytes.

---

## GVF-03 — Strengthen proof assertions

### Requirement

For any test claiming:

> “method X differs from incorrect method Y”

the test must calculate both X and Y.

Apply at minimum to:

- Indexed8 Bilinear palette-before-interpolation;
- Bilinear rounding-order proof;
- translated Bayer/dither proof.

### Forbidden Shortcut

No hard-coded fake “wrong path” values such as:

```text
wrong_result = 0
```

unless 0 is itself independently derived.

---

## GVF-04 — Complete destination-format matrix

### Required Target Formats

```text
RGB565
ARGB8888
XRGB8888
```

### Required Operations

For each supported target, provide exact evidence for:

```text
COPY
STRAIGHT_ALPHA
one scaled BLIT_EXT
```

For XRGB8888 exact bytes must use:

```text
BB GG RR FF
```

### Dither

For DITHER on non-RGB565 target:

- Strict behavior must assert one exact specified fault;
- Non-Strict behavior must assert the exact frozen behavior.

---

## GVF-05 — Complete extension / memory negative matrix

### Required Cases

At minimum:

```text
EXT_FLAGS behavior
W12 Reserved Strict/Non-Strict
W13 Reserved Strict/Non-Strict
W14 Reserved Strict/Non-Strict
W15 Reserved Strict/Non-Strict
bad extension type
bad extension version
bad extension length

bilinear required-neighbor memory failure
palette unmapped
palette highest valid entry
palette address overflow / invalid boundary
source stride/allocation invalid
destination allocation invalid
last exact legal byte
one byte beyond legal allocation
```

Each case must expect a deterministic result.

### Forbidden Shortcut

Do not accept:

```text
FAULT_A || FAULT_B
```

unless the authoritative specification explicitly permits either result.

---

## GVF-06 — Report/test-count hygiene

Update Stage-003R report metadata so:

```text
END_COMMIT
CTest count
commands
```

match the actual final repository state.

This is documentation cleanup, but required before Stage 004 final report.

---

# 7. BLOCK 1 — Tile Binary Structures & TILE_FRAME Decode

## Block Goal

Establish exact serialized Tile data structures without rendering yet.

---

## TDS-01 — TILE_FRAME opcode becomes implemented

Current TILE_FRAME classification must move from:

```text
defined but unsupported
```

to:

```text
implemented
```

for Golden Stage 004.

Decode the exact command according to Command ISA V0.1.

Do not change its wire layout.

---

## TDS-02 — Exact Tile Header serialization/parsing

Implement exact Tile Header binary parsing and serialization from the ISA.

Requirements:

- explicit Little-Endian;
- no compiler bitfields;
- no raw host-struct serialization;
- bounds checked;
- exact size/alignment assertions.

---

## TDS-03 — Exact WorkRef representation

Implement the frozen WorkRef representation.

Expected architectural meaning:

```text
32-bit Draw Descriptor index
```

WorkRef is not a host pointer.

Tests must prove:

```text
0
1
max valid index for test array
out-of-range index
```

---

## TDS-04 — Draw Descriptor Array uses existing 64B command encoding

The Tile Draw Descriptor Array must store the same 64B encoding used by Immediate draw commands.

Do not create `TileDrawDescV2`.

For a BLIT_EXT descriptor:

- the 64B descriptor remains in Draw Descriptor Array;
- `EXT_PTR` still points to the corresponding extension memory block.

---

## TDS-05 — Tile grid convention

Use row-major Tile IDs:

```text
tile_id = tile_y * tiles_x + tile_x
```

unless the Command ISA explicitly defines another formula.

Edge tiles use:

```text
valid_width  = min(tile_size, RT_WIDTH  - tile_x*tile_size)
valid_height = min(tile_size, RT_HEIGHT - tile_y*tile_size)
```

Do not render outside valid target pixels.

---

## TDS-06 — Invalid structure faults

Directed tests must cover:

```text
bad tile header bounds
work-list range outside array
descriptor index outside descriptor array
misaligned required pointer
descriptor memory unmapped
tile header memory unmapped
workref memory unmapped
target mismatch where ISA defines it
```

Use exact V0.1 fault codes.

---

# 8. BLOCK 2 — CPU Software Tile Binner

## Block Goal

Convert a legal ordered 2D draw stream into:

```text
Draw Descriptor Array
Tile Header Array
WorkRef Array
```

without changing draw semantics.

---

## BIN-01 — One descriptor per submitted draw

For each legal submitted draw command:

```text
descriptor_index == original draw index
```

unless the specification explicitly defines another indexing rule.

Zero-size/no-op draws may receive no WorkRefs but should not silently renumber later descriptors.

---

## BIN-02 — Raster bounds use approved rule

Binning must use the same effective raster rule already approved:

```text
effective_raster =
destination_rect
∩ optional_explicit_clip
∩ render_target_bounds
```

Clipping must not reset UV origin.

The binner only decides which tiles a draw may touch.

It must not change sampling coordinates.

---

## BIN-03 — Stable draw order

For each tile:

> WorkRefs must be in original global submission order.

Required order-sensitive test:

```text
Draw A
Draw B
Draw C
```

all overlap one tile.

The tile list must be:

```text
[A, B, C]
```

never sorted by:

- texture;
- opcode;
- x/y;
- descriptor address;
- blend mode.

---

## BIN-04 — Multi-tile coverage

Directed cases:

```text
1 pixel draw
draw exactly within one tile
draw crosses vertical tile boundary
draw crosses horizontal tile boundary
draw crosses four tiles
draw spans many tiles
draw entirely outside target
negative destination partially visible
explicit Clip reducing tile coverage
edge partial tile
```

---

## BIN-05 — Deterministic serialized output

For the same input stream and Tile size, serialized:

```text
descriptor array
tile headers
workrefs
```

must be byte-identical across repeated runs.

No unordered-container iteration may affect order.

---

## BIN-06 — Formal binary path

Unit tests may inspect C++ Tile structures.

Formal Tile equivalence tests must consume serialized memory structures through the same Golden memory model that TILE_FRAME uses.

A direct call such as:

```text
tile_renderer.render(vector<DrawObject>)
```

is not sufficient formal evidence by itself.

---

# 9. BLOCK 3 — Golden Tile Renderer

## Block Goal

Execute TILE_FRAME using serialized structures from `MemoryImage`.

---

## TR-01 — TILE_FRAME architectural path

Formal flow:

```text
64B TILE_FRAME command
→ decode
→ read Tile Header Array
→ read WorkRefs
→ read 64B Draw Descriptors
→ read Extension blocks when needed
→ render tiles
→ write framebuffer
```

Do not bypass binary parsing in formal tests.

---

## TR-02 — Shared Pixel Backend

Tile must reuse the same:

```text
texture sampling
palette
Color Key
Color Mod
effective alpha
blend
dither
format conversion
```

semantics as Immediate.

Preferred refactor:

```text
Shared Draw/Pixel Core
     ▲          ▲
     │          │
Immediate RT   Tile RT Adapter
```

### Forbidden Shortcut

Do not copy the Stage-003 blend/sampler code into a Tile-specific implementation.

Duplicated pixel arithmetic fails `TR-02`.

---

## TR-03 — Tile load semantics

For every active tile:

- load valid target pixels from framebuffer;
- decode according to destination format;
- initialize Tile Buffer state.

Correctness-first Stage 004 may always load active tiles.

Do not implement load-elision optimization yet.

Inactive tiles may be skipped.

---

## TR-04 — Compatibility quantization after every logical write

This is mandatory.

For RGB565 Compatibility Mode:

```text
blend result
→ logical RT write
→ RGB565 quantization
→ re-expand to canonical RGBA
→ store back into Tile Buffer
```

Therefore a later draw sees the same quantized destination it would see in Immediate Mode.

The same principle applies to supported destination formats.

### Important

Tile store to DDR is a memory transfer, not a new logical blend/write event.

Do not apply Dither a second time during Tile store.

Use a representation/store path that preserves the already-quantized Tile state exactly.

---

## TR-05 — Global coordinates preserved

Tile-local coordinates must never leak into graphics semantics.

Use global render-target coordinates for:

```text
Dither Bayer indexing
Clip
destination x/y
UV origin
debug mismatch reporting
```

Required dither test must cross a Tile boundary.

---

## TR-06 — Draw order preserved

Within each Tile, execute WorkRefs strictly in list order.

Alpha and Additive make this externally visible.

No Tile-local state sorting.

---

## TR-07 — Edge tiles

Do not read/write framebuffer pixels outside valid target dimensions.

Required target sizes include dimensions not divisible by:

```text
16
32
64
```

---

## TR-08 — Descriptor/extensions/palette memory

Tile execution must correctly access:

- 64B Draw Descriptor;
- Extension block;
- Texture;
- Palette;
- destination framebuffer.

All memory faults propagate deterministically.

---

# 10. BLOCK 4 — Immediate-vs-Tile Pixel-Exact Gate

This is the most important Stage-004 Block.

For each test:

```text
same initial memory
same draw order
same assets
same command semantics
```

run:

```text
Immediate renderer
Tile renderer
```

then compare final framebuffer bytes exactly.

No tolerance.

---

## EQ-01 — Basic FILL equivalence

Required:

```text
single Fill
multiple Fill
overlap
cross-tile Fill
edge-tile Fill
zero-size
```

---

## EQ-02 — Basic BLIT equivalence

Required:

```text
RGB565
ARGB8888 source
XRGB8888 source
source offsets
padded stride
cross-tile sprite
```

---

## EQ-03 — Order-sensitive equivalence

Required:

```text
Straight Alpha overlap
repeated Alpha overdraw
Additive overlap
Alpha + Additive mixed order
three or more draws on same pixels
```

Create a case where reordering WorkRefs produces a visibly different result.

The test must fail if WorkRef order is reversed.

---

## EQ-04 — Extended feature equivalence

At minimum:

```text
Color Key
Color Mod
Global Alpha
Pixel Alpha
Premult Alpha
Nearest Scaling
Bilinear Scaling
Clamp
Repeat
Clip
Indexed8 + Palette
RGB565 Dither
```

Do not require every cross-product.

Require at least one Tile-boundary-crossing case for:

- scaling;
- bilinear;
- clip;
- dither.

---

## EQ-05 — Destination format equivalence

Exact final framebuffer for:

```text
RGB565
ARGB8888
XRGB8888
```

---

## EQ-06 — Tile sizes

Golden equivalence must run with:

```text
16×16
32×32
64×64
```

Hardware default remains 32×32.

Tile-size sweep in Golden does not silently change the frozen hardware default.

---

## EQ-07 — Deterministic randomized draw streams

Required minimum:

```text
≥ 100 frames at Tile 16
≥ 100 frames at Tile 32
≥ 100 frames at Tile 64
```

Total:

```text
≥ 300 deterministic random Immediate-vs-Tile frames
```

Use fixed seeds.

Small frame sizes are acceptable for runtime.

Random feature distribution should include:

```text
FILL
BLIT
BLIT_EXT nearest
Straight Alpha
Additive
Color Key
Clip
some Bilinear
some Palette
```

A failure must report:

```text
seed
tile_size
command_index if known
first mismatch x/y
expected bytes/pixel
actual bytes/pixel
tile_id
workref list
```

---

## EQ-08 — Pathological Tile workloads

Required:

```text
many draws all in one tile
one draw covering many tiles
all tiles inactive
only last edge tile active
high overdraw stack
many zero-area/outside draws
```

---

## EQ-09 — Binary Tile fixtures

Add checked deterministic Tile fixtures, at minimum:

```text
tile_fill_basic/
tile_alpha_overlap/
tile_bilinear_cross_boundary/
tile_palette/
tile_dither_cross_boundary/
```

Each should contain enough data to replay:

```text
TILE_FRAME command
descriptor array
tile headers
workrefs
extensions
textures
palette
initial framebuffer
golden framebuffer
```

Expected framebuffer must originate from the already-approved Immediate Golden result, not from Tile itself.

Normal build/test must not overwrite it.

---

# 11. BLOCK 5 — Tile Statistics & Architecture Instrumentation

No cycle-accurate model in Stage 004.

Collect architecture-level counters.

---

## PROF-01 — Basic Tile counters

Required:

```text
tiles_total
tiles_active
tile_load_pixels
tile_store_pixels
tile_load_bytes
tile_store_bytes
draw_descriptor_count
workref_count
max_workrefs_per_tile
sum_workrefs
```

Derived:

```text
avg_workrefs_per_active_tile
```

---

## PROF-02 — Pixel workload counters

Reuse or align with Golden profiler naming where available:

```text
fragments/pixels attempted
pixels written
key discards
clip rejects
blend operations
texture samples
palette reads
bilinear sample count
```

Do not create incompatible duplicate names without reason.

---

## PROF-03 — Overdraw

Provide at least:

```text
per-pixel overdraw count
max overdraw
average overdraw on touched pixels
```

A heatmap image is not required yet.

Raw matrix/CSV is sufficient.

---

## PROF-04 — Logical vs estimated physical traffic

Keep separate:

```text
logical pixel/texture traffic
Tile framebuffer load/store traffic
estimated external traffic
```

Do not label an analytical estimate as measured DDR bandwidth.

---

## PROF-05 — No fake timing

Do not report:

```text
FPS
cycles
latency
GB/s actual
```

from the functional Tile Golden unless derived from explicitly labeled analytical assumptions.

Real cycle/stall data comes later from RTL/hardware counters.

---

# 12. BLOCK 6 — Tile Size Architecture Sweep

Use the functional Tile model and fixed workloads.

---

## EXP-01 — Fixed workloads

Create deterministic architecture workloads such as:

```text
W1 Sprite Grid
W2 High Overdraw Stack
W3 Alpha Storm
W4 Large Scaled Sprites
W5 Edge/Scatter Sprites
W6 Mixed Sprite Scene
```

Each workload must have a fixed seed/version.

---

## EXP-02 — Sweep Tile sizes

Run:

```text
16×16
32×32
64×64
```

Collect:

```text
tile count
active tile count
workref count
avg/max refs/tile
tile load/store bytes
estimated framebuffer traffic
overdraw
```

---

## EXP-03 — Machine-readable results

Write:

```text
results/stage004_tile/tile_sweep.csv
```

and optionally JSON.

The file should include:

```text
workload
tile_size
metrics
```

---

## EXP-04 — Human-readable summary

Write:

```text
results/stage004_tile/tile_sweep_summary.md
```

Summarize:

- where smaller Tile helps;
- where larger Tile reduces metadata/workrefs;
- memory-traffic tradeoff;
- why 32×32 remains the current default or why evidence suggests reevaluation.

Do not silently change frozen hardware Tile size from this experiment.

If evidence strongly contradicts 32×32, raise a DESIGN_QUESTION for a later architecture decision.

---

# 13. BLOCK 7 — Stage Self-Audit & Machine-Checkable Acceptance

This Block is mandatory before the Agent writes `REPORT_004`.

---

## AUD-01 — Acceptance manifest

Create:

```text
docs/tasks/STAGE_004_ACCEPTANCE.json
```

using the provided template.

For every Mandatory ID:

```text
status
implementation_evidence
verification_evidence
test_name
notes
```

must be filled.

No mandatory item may remain:

```text
TODO
PARTIAL
UNKNOWN
```

if the Agent reports PASS.

---

## AUD-02 — Acceptance checker

Create:

```text
scripts/check_stage004_acceptance.py
```

Standard-library Python only.

It must at least verify:

- every mandatory ID exists;
- every mandatory ID has `status = PASS`;
- evidence paths exist;
- required test-name fields are non-empty where verification is required;
- REPORT_004 contains every mandatory ID.

Optional:

- invoke `ctest -N` when build path is supplied and confirm named tests exist.

---

## AUD-03 — Re-read the task

Before writing the report:

> Re-read this complete TASK_004 document.

Then compare every MUST/REQUIRED statement against the Acceptance Manifest.

If anything lacks evidence:

> continue implementation; do not declare PASS.

---

# 14. Mandatory Acceptance IDs

The required IDs are:

```text
GVF-01 ... GVF-06

TDS-01 ... TDS-06

BIN-01 ... BIN-06

TR-01 ... TR-08

EQ-01 ... EQ-09

PROF-01 ... PROF-05

EXP-01 ... EXP-04

AUD-01 ... AUD-03
```

Total mandatory items:

```text
47
```

Every one must appear in the final Evidence Matrix.

---

# 15. Forbidden Shortcuts

The Agent must not:

```text
use Tile output to generate its own expected framebuffer
copy/paste a second pixel pipeline into Tile
sort WorkRefs for convenience
use host pointers as WorkRefs
replace 64B Draw Descriptors with custom C++ objects in formal tests
skip binary TILE_FRAME path in formal equivalence tests
change reference fixtures automatically during normal build/test
weaken old Golden tests
delete old regressions
change expected values merely because Tile disagrees
claim differential coverage from “no fault”
report analytical traffic as measured hardware bandwidth
start RTL Tile implementation
```

---

# 16. CMake / Test Integration

Recommended tests:

```text
golden_test_tile_structures
golden_test_tile_binner
golden_test_tile_faults
golden_test_tile_renderer_basic
golden_test_tile_order
golden_test_tile_equivalence_directed
golden_test_tile_equivalence_random
golden_test_tile_formats
golden_test_tile_dither
golden_test_tile_profile
golden_test_stage004_acceptance
```

Exact names may vary, but Acceptance Manifest must point to actual tests.

---

# 17. Required Build / Verification Commands

At minimum:

```bash
python scripts/preflight.py

cmake -S . -B build/stage004
cmake --build build/stage004

ctest --test-dir build/stage004 --output-on-failure

python scripts/check_test_integrity.py
python tools/fixture_validate/fixture_validate.py model/golden/tests/frames
python scripts/check_stage004_acceptance.py

git status --short
```

Normal build/test must not modify checked reference fixtures.

---

# 18. Cross-Compiler / Sanitizer

If clang++ is already available:

```text
configure
build
CTest
```

once.

If supported locally, run ASan/UBSan over at least:

```text
Tile structures
Tile binner
Tile directed equivalence
```

Do not install tools automatically.

Record `NOT RUN` with reason if unsupported.

---

# 19. Stage Report

Create:

```text
docs/reports/REPORT_004_Golden_Tile_Renderer_and_Binning.md
```

Required structure:

```markdown
# REPORT_004 — Golden Tile Renderer, Software Binning & Pixel-Exact

## 1. Result

## 2. START_COMMIT
105c23c0989c74fcd6273b33181c6079e40f5d5e

## 3. END_COMMIT
<exact SHA>

## 4. Block 0 Closure
GVF-01 ... GVF-06

## 5. Tile Architecture
Explain descriptor/header/workref flow.

## 6. TILE_FRAME Binary Path

## 7. Software Binner

## 8. Tile Renderer
Explain shared pixel backend / RT adapter.

## 9. Compatibility Quantization
Explain why every logical write remains Immediate-exact.

## 10. Directed Immediate-vs-Tile Results

## 11. Random Immediate-vs-Tile Results
Tile16:
Tile32:
Tile64:
Seeds/cases.

## 12. Tile Fault Tests

## 13. Binary Tile Fixtures

## 14. Tile Statistics

## 15. Tile Size Sweep

## 16. Architecture Conclusions

## 17. Full Acceptance Evidence Matrix
Every mandatory ID:
Requirement / Implementation / Verification / Result.

## 18. CTest
Exact count and command.

## 19. Fixture Integrity

## 20. Acceptance Checker

## 21. Cross-Compiler / Sanitizer

## 22. Warnings / Limitations

## 23. Blockers

## 24. Design Questions

## 25. Deviations

## 26. Suggested Next Stage
```

Do not write a generic “all covered” sentence in place of the Evidence Matrix.

---

# 20. Stage PASS Criteria

Stage 004 can report PASS only if:

```text
All 47 Mandatory Acceptance IDs = PASS
AND
full CTest = PASS
AND
reference fixtures unchanged by ordinary build/test
AND
Immediate == Tile exact for all directed cases
AND
Immediate == Tile exact for ≥300 required random frames
AND
Tile 16/32/64 all pass
AND
no duplicated Tile-specific pixel arithmetic
AND
TILE_FRAME formal path uses serialized structures
AND
Tile size sweep results are produced
AND
Acceptance checker passes
AND
frozen specs are unmodified
```

If one mandatory ID is incomplete:

> Stage result is not PASS.

---

# 21. Expected Exit State

After Stage 004:

```text
Immediate Golden Renderer          stable
Tile Software Binner               implemented
Draw Descriptor Array              implemented
Tile Headers                       implemented
32-bit WorkRefs                    implemented
TILE_FRAME Golden execution        implemented
Tile Buffer Compatibility Mode     implemented
Immediate == Tile                  pixel-exact verified
Tile 16/32/64                      verified
Tile metrics                       implemented
Tile size architecture sweep       completed
```

Not yet implemented:

```text
Texture Cache
RTL Tile Renderer
Command Ring hardware
Display hardware
performance counters in FPGA
Affine
3D
```

---

# 22. Expected Next Stage

Subject to REVIEW_004:

> **Stage 005 — RTL Foundation: Packages, Interfaces, Register File, Memory-Service Skeleton & FILL Pipeline**

Likely goals:

```text
SystemVerilog package/types
frozen internal interfaces
MMIO register file
memory-service request skeleton
FILL frontend
first RTL pixel/write path
unit simulation against Golden vectors
```

Do not begin Stage 005 before REVIEW_004.
