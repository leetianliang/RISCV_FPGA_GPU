# TASK_001 — Golden GPU Foundation & FILL

> Project: RISC-V–FPGA General-Purpose 2D GPU  
> Stage ID: `001`  
> Review granularity: **one review after the complete stage**  
> Executor: Local AI Agent  
> Reviewer: Project Architect / ChatGPT  
> Entry condition: `REVIEW_000 = PASS`, Gate A passed  
> Reviewed baseline commit: `610b0fb87a97d77243f480c00a9eaf498de2f717`  
> Priority: P0  
> Status: READY

## 1. Stage Objective

Build the first real executable slice of the PC Golden GPU:

```text
64B binary FILL_RECT command
        ↓
Command decode / validation
        ↓
Golden execution
        ↓
RGB565 framebuffer memory
        ↓
deterministic RAW framebuffer
        ↓
exact reference comparison
```

This stage also establishes bit-exact arithmetic helpers, a 32-bit physical-memory model, RGB565 surface access, C++20/CTest infrastructure, the first checked-in Golden vector, and strict build/test behavior.

The objective is not feature count. It is the first trustworthy executable-specification path.

## 2. Stage Review Rule

Complete all subtasks below before formal review. Internal commits/checkpoints are encouraged, but no review is required after each small task.

Stop the affected work and report a `BLOCKER` or `DESIGN_QUESTION` only when a specification conflict, architecture ambiguity, or correctness blocker prevents safe continuation.

## 3. Authoritative Sources

Read and follow:

1. `docs/RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`
2. `docs/RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md`
3. `docs/RISC-V_FPGA_2D_GPU_Golden_GPU_Software_Architecture_V0.1.md`
4. `docs/RISC-V_FPGA_2D_GPU_Verification_Plan_V0.1.md`
5. `docs/RISC-V_FPGA_2D_GPU_Register_Map_Memory_Map_V0.1.md`
6. `AGENTS.md`

Do not modify frozen specifications.

## 4. Explicit Non-Goals

Do not implement BLIT, texture sampling, Color Key, Alpha blending, Additive, Bilinear, Indexed8, Palette, Tile rendering/binning, Command Ring DMA, Present/VSYNC, Fence timing, cache/performance models, GUI, RTL or FPGA board logic.

## 5. Expected Golden Structure

```text
model/golden/
├── CMakeLists.txt
├── include/golden/
│  ├── gpu_types.hpp
│  ├── gpu_isa.hpp
│  ├── gpu_math.hpp
│  ├── memory_image.hpp
│  ├── surface.hpp
│  ├── command_decoder.hpp
│  └── golden_gpu.hpp
├── src/
│  ├── gpu_math.cpp
│  ├── memory_image.cpp
│  ├── surface.cpp
│  ├── command_decoder.cpp
│  └── golden_gpu.cpp
└── tests/
   ├── unit/
   ├── directed/
   └── frames/fill_basic/
```

Private decomposition may differ, but public semantics must not.

## 6. Subtask 001.1 — Build and Test Infrastructure

Create real CMake targets:

```text
golden_core
golden_unit_tests
golden_directed_tests
```

Use C++20 and CTest. No third-party dependencies.

Enable at least `-Wall -Wextra -Wpedantic` for GCC/Clang, or comparable MSVC warnings.

Compile/test failure is fatal from this stage onward.

## 7. Subtask 001.2 — Core Types

Implement fixed-width architectural types with `<cstdint>`.

Required concepts:

- canonical RGBA8888;
- `PixelFormat`;
- `BlendMode`;
- `FilterMode`;
- `AddressMode`;
- minimal fault/result representation.

Numeric enum values already frozen by Command ISA must match exactly.

Canonical color is logical `0xAARRGGBB`. Use explicit pack/extract helpers; do not use compiler bitfields.

## 8. Subtask 001.3 — Bit-Exact Arithmetic Foundation

Implement scalar reference helpers exactly from Pixel Arithmetic V0.1:

```text
sat_u8
div255_rn
mul8_rn
rgb565_decode
rgb565_encode
floor_q16_16
frac_q16_16
round_div_signed
lerp16
```

Rules:

- no floating point;
- use sufficiently wide intermediates;
- no SIMD/intrinsics/threading/approximations;
- implementation should closely mirror the specification formulas.

`lerp16` and Q16 helpers are primitives only; Bilinear/Scaling rendering remains out of scope.

## 9. Subtask 001.4 — Arithmetic Verification

Required exhaustive tests:

- all 65,536 RGB565 values: `encode(decode(x)) == x`;
- `DIV255_RN` for every `x = 0..65025`;
- `MUL8_RN` for all `256 × 256` pairs.

Required directed tests:

- Q16 positive/negative/integer/fraction cases;
- signed round-div positive/negative/tie cases;
- `lerp16` at `0`, `0x8000`, `0xFFFF`;
- RGB565 black, white, primary colors and nontrivial values.

Tests must be deterministic.

## 10. Subtask 001.5 — MemoryImage

Implement a region-based/sparse Golden physical memory model with 32-bit physical addresses. Do **not** allocate 4 GiB.

Required:

- register named region;
- base + explicit byte size;
- backing byte storage;
- overlap rejection;
- little-endian read/write 8/16/32;
- block read/write;
- deterministic unmapped/out-of-range error;
- address arithmetic checked with a wider intermediate.

Do not silently wrap addresses.

## 11. Subtask 001.6 — Surface

Implement `SurfaceDesc` with at least:

```text
base
stride
width
height
format
```

Stage-001 required destination format: **RGB565**.

Implement `read_pixel()` and `write_pixel()` through `MemoryImage`, using exact RGB565 arithmetic.

Surface access rejects coordinates outside its declared width/height.

## 12. Subtask 001.7 — Exact 64B Command Representation

Implement exact 64-byte serialization/deserialization. A safe representation such as `std::array<uint32_t,16>` is recommended.

Requirements:

- 16 × 32-bit words;
- little-endian;
- no compiler bitfield packing;
- static assertions where applicable.

## 13. Subtask 001.8 — Header Decode / Minimal Validation

Decode:

```text
CMD_CLASS
OPCODE
VERSION
LENGTH_DW
HDR_FLAGS
SEQUENCE_ID
USER_TAG
EXT_PTR
```

Stage 001 executes only `DRAW_2D / FILL_RECT`, Version 1, Length 16 DWORDs.

Test:

- valid FILL;
- bad version;
- bad length;
- unknown class/opcode;
- relevant reserved/header-invalid cases;
- extension request unsupported by the current Stage-001 capability.

Distinguish malformed encoding from valid-but-not-yet-implemented capability.

## 14. Subtask 001.9 — FILL Payload Decode

Decode FILL exactly from the ISA, including:

```text
DST_BASE
DST_STRIDE
DST_XY
DST_WH
DRAW_STATE
PRIMARY_COLOR
```

Stage-001 execution profile:

```text
DST_FORMAT = RGB565
BLEND_MODE = COPY
DITHER_EN = 0
CLIP_EN = 0
H_EXT_VALID = 0
```

Fields explicitly ignored by the FILL ISA must not affect output.

Do not silently treat unsupported blend/filter/clip behavior as COPY.

## 15. Subtask 001.10 — FILL Golden Execution

Implement the architectural path:

```text
serialized 64B command
→ decode
→ validate
→ execute FILL_RECT
→ Surface::write_pixel
→ MemoryImage framebuffer
```

Required semantics:

- half-open rectangle;
- zero width/height is legal no-op;
- `PRIMARY_COLOR` uses canonical `0xAARRGGBB`;
- COPY semantics;
- RGB565 conversion exactly per Pixel Arithmetic V0.1.

For Stage-001 directed tests, destination x/y must be non-negative and the rectangle must fit the registered surface. This is a stage test limitation, not a redefinition of the full ISA.

Do not invent Clip behavior.

If a required surface property is not derivable from binary command state, use explicit Golden test-harness surface metadata rather than changing ISA. Report architecture ambiguity as `DESIGN_QUESTION`.

## 16. Subtask 001.11 — Minimal GoldenGPU Facade

Provide a minimal architectural facade similar to:

```cpp
class GoldenGPU {
public:
    ExecResult execute_command(...);
};
```

It must execute serialized command input and return explicit success/fault status.

No hidden direct-render shortcut may replace the formal binary path.

## 17. Subtask 001.12 — Directed FILL Tests

At minimum:

- `FILL-001`: 1×1 fill;
- `FILL-002`: interior rectangle;
- `FILL-003`: full-surface fill;
- `FILL-004`: two overlapping COPY fills, later wins;
- `FILL-005`: zero width;
- `FILL-006`: zero height;
- `FILL-007`: non-tight valid stride;
- `FILL-008`: black/white/red/green/blue/nontrivial color;
- `FILL-009`: ISA-ignored source fields changed, output unchanged;
- `FILL-010`: invalid command header causes expected fault and no framebuffer modification.

## 18. Subtask 001.13 — First Binary Golden Fixture

Create:

```text
model/golden/tests/frames/fill_basic/
├── manifest.json
├── command.bin
├── initial_fb.raw
└── golden_fb.raw
```

Recommended target: `16×16 RGB565`.

Use a nontrivial interior rectangle.

The fixture must be generated by a documented helper/path. Do not hand-edit expected framebuffer bytes without documented derivation.

Manifest includes at least:

```text
format_version
isa_version
pixel_arith_version
width
height
stride
framebuffer_format
command_count
description
```

## 19. Subtask 001.14 — Frame Compare Tool

Implement:

```text
tools/frame_compare/frame_compare.py
```

Python standard library only.

Required:

- size comparison;
- exact byte comparison;
- mismatch count;
- first mismatch byte offset;
- exit 0 on exact match;
- non-zero on mismatch/error.

Test both matching and intentionally mismatching files.

## 20. Subtask 001.15 — Binary Fixture Runner

Provide either a small `golden_cli` or an equivalent directed-test runner that proves a real serialized 64B command can be read/executed and produce an output framebuffer.

A test that only creates an internal C++ Fill object is insufficient.

## 21. Subtask 001.16 — CTest Integration

The following must execute the whole Stage-001 test suite:

```bash
ctest --test-dir build/stage001 --output-on-failure
```

Any compile or test failure is fatal.

## 22. Subtask 001.17 — Project Status

At stage completion update `docs/PROJECT_STATUS.md` to state:

```text
Current Stage: Golden GPU Foundation & FILL
Last Completed Stage: 001 (pending reviewer approval)
Current Gate: Golden Stage 001 awaiting review
Next Planned Stage: BLIT / basic texture path, pending review
```

Do not claim reviewer approval before review.

## 23. Required Verification Matrix

| Area | Required evidence |
|---|---|
| Build | C++20 compile success |
| Arithmetic | exhaustive RGB565 |
| Arithmetic | exhaustive DIV255 |
| Arithmetic | exhaustive MUL8 |
| Arithmetic | directed Q16/LERP |
| MemoryImage | endian + bounds + overlap |
| Surface | RGB565 exact read/write |
| Command | valid/invalid header |
| FILL | directed cases |
| Binary path | `.bin` command executed |
| Frame compare | exact PASS + intentional mismatch FAIL |
| CTest | all registered tests PASS |

## 24. Required Commands

From repository root run at minimum:

```bash
python scripts/preflight.py
cmake -S . -B build/stage001
cmake --build build/stage001
ctest --test-dir build/stage001 --output-on-failure
```

Also run frame compare against one matching pair and one intentionally modified pair. Record exact exit codes.

If practical without installing tools, also perform one clang++ configure/build as a non-blocking portability sanity check.

## 25. Code Quality Rules

Required:

- fixed-width integers for architectural values;
- explicit signedness;
- no compiler bitfields for command encoding;
- no floating point in bit-exact math;
- no UB-dependent signed overflow;
- no unchecked address wrap;
- deterministic tests;
- clear separation between serialized ISA and decoded internal objects.

## 26. Specification Integrity

Do not:

- change command layout;
- change RGB565 rounding;
- change zero-size semantics;
- reinterpret `0xAARRGGBB`;
- change endianness;
- change tests to match incorrect implementation;
- invent Clip behavior;
- silently map unsupported FILL state to COPY.

If a valid full-spec feature is not implemented in Stage 001, report current capability unsupported rather than approximating it.

## 27. Required Design-Question Reporting

Do not guess if you encounter ambiguity involving:

- surface dimensions/metadata not encoded in a command;
- strict-mode behavior;
- ignored/reserved FILL fields;
- Command ISA vs Pixel Arithmetic;
- public Golden APIs that would make later Tile support impossible without redesign.

Report `DESIGN_QUESTION` and isolate affected behavior.

## 28. Suggested Internal Commits

Formal review remains stage-level, but useful checkpoints are:

```text
build: enable golden core and ctest
feat(golden): add bit-exact arithmetic foundation
feat(golden): add physical memory and rgb565 surface
feat(golden): add fill command decode and execution
test(golden): add exhaustive and binary fill vectors
tools: add raw frame compare
docs: add stage001 implementation report
```

## 29. Stage Report

Create:

```text
docs/reports/REPORT_001_Golden_Foundation_and_FILL.md
```

It must contain:

```text
Result
START_COMMIT
END_COMMIT
Subtasks implemented
Files added/modified
Specification compliance
Arithmetic verification counts/results
Unit-test results
Directed FILL results
Binary fixture result
Frame compare matching/mismatch results
Build/CTest commands + exit codes
Compiler/version
Warnings/limitations
Blockers
Design questions
Task deviations
Suggested next stage
```

## 30. Reviewer Handoff

After the complete stage is pushed to GitHub, provide:

- branch/ref;
- `REPORT_001_Golden_Foundation_and_FILL.md`;
- START_COMMIT;
- END_COMMIT;
- CTest summary;
- blockers/design questions.

Do not begin Stage 002 before formal review.

## 31. Stage Acceptance Criteria

Stage 001 passes only if:

- [ ] Golden C++20 targets build;
- [ ] no third-party dependency added;
- [ ] exhaustive RGB565 round-trip passes;
- [ ] exhaustive DIV255 passes;
- [ ] exhaustive MUL8 passes;
- [ ] directed Q16/LERP tests pass;
- [ ] MemoryImage tests pass;
- [ ] RGB565 Surface tests pass;
- [ ] exact 64B binary command representation exists;
- [ ] FILL header/payload decoding works;
- [ ] FILL COPY executes through binary architectural path;
- [ ] zero-size FILL is a no-op;
- [ ] deterministic binary fixture exists;
- [ ] generated framebuffer matches reference exactly;
- [ ] intentional frame mismatch is detected;
- [ ] complete CTest suite passes;
- [ ] frozen specifications are unmodified;
- [ ] no BLIT/Tile/Alpha/RTL scope creep;
- [ ] Stage report is complete.

## 32. Expected Exit State

```text
Specifications       frozen baseline
Golden core          initialized
Bit-exact math       foundational helpers verified
Memory model         working
RGB565 surface       working
64B command path     working for FILL
FILL_RECT            bit-exact Golden implementation
RAW frame compare    working
BLIT                 not started
Tile                 not started
RTL                  not started
```

Expected next stage, subject to review:

> **Stage 002 — Golden BLIT & Basic Texture Path**
