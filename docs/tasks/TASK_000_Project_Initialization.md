# TASK_000 — Project Initialization & Repository Baseline

> Project: RISC-V–FPGA General-Purpose 2D GPU  
> Task ID: `TASK_000`  
> Stage: Project Initialization / Pre-Implementation  
> Priority: P0  
> Executor: Local AI Agent  
> Reviewer: Project Architect / ChatGPT  
> Status: READY AFTER SPEC PREFLIGHT  
> Expected outcome: A clean, reproducible repository skeleton prepared for Golden GPU and RTL implementation, with no functional GPU implementation yet.

---

# 1. Objective

Initialize the local project repository before any Golden GPU or RTL implementation begins.

This task must establish:

1. A stable repository directory structure.
2. Git/version-control baseline.
3. Project-level agent rules.
4. Specification indexing and status tracking.
5. Build/test placeholders without implementing GPU functionality.
6. A reproducible preflight check.
7. A standardized task/report/review workflow.
8. A clean baseline commit suitable for later staged development.

This task is **infrastructure only**.

Do **not** implement:

- Golden rendering algorithms.
- FILL_RECT.
- BLIT.
- Pixel arithmetic.
- Command decoder.
- RTL.
- FPGA board logic.
- Tile rendering.
- Performance modeling.

Those belong to later tasks.

---

# 2. Critical Rule: Specifications Are Authoritative

The implementation must follow the project specifications.

The agent MUST NOT silently change architecture, ISA, arithmetic behavior, register semantics, or internal interfaces to make implementation easier.

If a specification conflict or missing required specification is found:

> STOP the affected work and record a `BLOCKER` in the implementation report.

Do not invent a replacement rule.

---

# 3. Required Specification Preflight

Before modifying the repository, inspect `docs/` and identify the exact filenames present.

The following documents are expected to exist before implementation work begins:

1. `RISC-V_FPGA_2D_GPU_Requirements_V1.0.md`
2. `RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md`
3. `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`
4. `RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md`
5. `RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md`
6. `RISC-V_FPGA_2D_GPU_Register_Map_Memory_Map_V0.1.md`
7. `RISC-V_FPGA_2D_GPU_Verification_Plan_V0.1.md`
8. `RISC-V_FPGA_2D_GPU_Project_Development_Flow_V1.0.md`
9. `RISC-V_FPGA_2D_GPU_Golden_GPU_Software_Architecture_V0.1.md`

## 3.1 Preflight behavior

Create a table in the final report:

| Required spec | Found filename | Status |
|---|---|---|
| Requirements | ... | FOUND/MISSING |
| System Architecture | ... | FOUND/MISSING |
| Command ISA | ... | FOUND/MISSING |
| Internal Interface | ... | FOUND/MISSING |
| Pixel Arithmetic | ... | FOUND/MISSING |
| Register/Memory Map | ... | FOUND/MISSING |
| Verification Plan | ... | FOUND/MISSING |
| Project Development Flow | ... | FOUND/MISSING |
| Golden Software Architecture | ... | FOUND/MISSING |

## 3.2 Blocking rule

If either of the following is missing:

- `System Architecture V1.0`
- `Pixel Format & Arithmetic Specification V0.1`

then:

- Repository initialization may continue.
- No functional Golden/RTL implementation may begin.
- Mark the repository state as `SPEC_INCOMPLETE`.
- Record the missing files prominently in `docs/SPEC_STATUS.md`.
- Record a `BLOCKER` in the task report.

Do not create substitute technical content.

---

# 4. Existing Files: Preservation Rules

Existing specification files in `docs/` are read-only for this task.

The agent MUST NOT:

- Rewrite them.
- Rename them.
- Reformat them.
- Normalize their headings.
- Fix perceived inconsistencies.
- Change version numbers.
- Merge documents.
- Delete documents.

The only allowed operations on existing specs are:

- Read.
- Index.
- Reference by filename.
- Calculate optional file hashes.

---

# 5. Repository Root

Assume the current project root is:

```text
FPGA_2D_GPU/
```

The current repository may initially contain only:

```text
docs/
```

Do not assume Git or a build system already exists.

---

# 6. Target Repository Structure

Create the following baseline structure:

```text
FPGA_2D_GPU/
│
├── README.md
├── AGENTS.md
├── .gitignore
├── .gitattributes
├── CMakeLists.txt
│
├── docs/
│  ├── README.md
│  ├── SPEC_STATUS.md
│  ├── tasks/
│  ├── reports/
│  ├── reviews/
│  └── decisions/
│
├── spec/
│  ├── README.md
│  └── generated/
│
├── model/
│  ├── golden/
│  │  ├── CMakeLists.txt
│  │  ├── include/
│  │  ├── src/
│  │  ├── tests/
│  │  │  ├── unit/
│  │  │  ├── directed/
│  │  │  ├── random/
│  │  │  └── frames/
│  │  └── tools/
│  │
│  └── architecture/
│     ├── tile_model/
│     ├── cache_model/
│     ├── bandwidth_model/
│     ├── lane_model/
│     └── reports/
│
├── rtl/
│  ├── common/
│  ├── ctrl/
│  ├── command/
│  ├── frontend/
│  ├── texture/
│  ├── pixel/
│  ├── tile/
│  ├── memory/
│  ├── display/
│  └── perf/
│
├── sim/
│  ├── unit/
│  ├── subsystem/
│  ├── random/
│  └── fullsystem/
│
├── software/
│  ├── driver/
│  ├── graphics/
│  ├── engine/
│  ├── benchmark/
│  └── applications/
│
├── tools/
│  ├── asset_converter/
│  ├── command_generator/
│  ├── frame_compare/
│  └── result_parser/
│
├── scripts/
│
├── board/
├── assets/
├── results/
└── release/
```

Use `.gitkeep` only where needed to preserve otherwise-empty directories.

Do not create unnecessary placeholder source files.

---

# 7. TASK_000 File Placement

Copy this task document into:

```text
docs/tasks/TASK_000_Project_Initialization.md
```

The agent must preserve the task content.

The final execution report must be written to:

```text
docs/reports/REPORT_000_Project_Initialization.md
```

---

# 8. Create `AGENTS.md`

Create a root-level `AGENTS.md`.

It must contain the following project operating rules.

## 8.1 Authority hierarchy

When implementing this project, use this hierarchy:

```text
Requirements
  ↓
System Architecture
  ↓
Command ISA
  ↓
Internal Interface Specification
  ↓
Pixel Format & Arithmetic Specification
  ↓
Register/Memory Map
  ↓
Verification Plan
  ↓
Stage Task Document
  ↓
Implementation
```

When rules overlap:

- Command binary layout → Command ISA is authoritative.
- Pixel numerical behavior → Pixel Arithmetic Specification is authoritative.
- RTL module communication → Internal Interface Specification is authoritative.
- MMIO/address behavior → Register/Memory Map is authoritative.
- Verification acceptance → Verification Plan is authoritative.

## 8.2 Forbidden autonomous changes

The local agent must not autonomously alter:

- Command size.
- Opcode encoding.
- Pixel rounding.
- Alpha equations.
- RGB565 conversion.
- Q16.16 semantics.
- Tile draw ordering.
- Immediate/Tile compatibility semantics.
- Internal interface payload semantics.
- Register offsets.
- Fault semantics.
- Capability meanings.

If a change appears necessary, write:

```text
DESIGN_QUESTION:
...
```

or:

```text
BLOCKER:
...
```

and stop the affected implementation.

## 8.3 Test integrity

Include this exact rule:

> A failing test may only be changed if the authoritative specification changed. Do not modify expected results merely to match the implementation.

Also state:

- Never delete a valid failing regression to make CI pass.
- Every fixed bug should receive a regression test.
- Do not weaken assertions without review.

## 8.4 Scope discipline

The agent may make local implementation decisions only when they do not alter externally visible specified behavior.

Examples of allowed local decisions:

- Function decomposition.
- Private helper naming.
- Internal container choice.
- Local C++ class organization.
- RTL state encoding.
- FIFO implementation details when not frozen.

Examples requiring review:

- New ISA flags.
- Changed arithmetic.
- Changed Tile semantics.
- Changed register fields.
- Changed public interfaces.

## 8.5 Reporting

At the end of every task, the agent must create the requested report and include:

- Files changed.
- Tests run.
- Exact commands run.
- Test results.
- Warnings.
- Known limitations.
- Blockers.
- Design questions.
- Suggested next steps.

---

# 9. Create Root `README.md`

The root README should be concise and engineering-focused.

It must include:

## 9.1 Project title

```text
RISC-V–FPGA General-Purpose 2D GPU
```

## 9.2 Project positioning

Describe the project as:

> A command-driven, Tile-Based embedded 2D GPU using a RISC-V + FPGA heterogeneous architecture for sprite-heavy games and embedded GUI/HMI workloads.

Do not claim unimplemented features are already complete.

## 9.3 Current project phase

State:

```text
Current phase: Specification complete / implementation initialization
```

If required specs are missing, state:

```text
Current phase: Specification preflight incomplete
```

## 9.4 Major architecture concepts

Briefly mention:

- 64B Command ISA.
- RISC-V software + FPGA graphics accelerator.
- Command Ring.
- Shared Texture / Pixel pipeline.
- Immediate + Tile render paths.
- Bit-accurate PC Golden Model.
- Performance counters/X-Ray.
- Future Affine/2.5D extension.

Mark unimplemented items as planned.

## 9.5 Quick directory guide

Explain:

- `docs/`
- `model/golden/`
- `model/architecture/`
- `rtl/`
- `sim/`
- `software/`
- `tools/`
- `board/`
- `results/`

## 9.6 Development workflow

Summarize:

```text
Spec
→ Task
→ Agent Implementation
→ Automated Tests
→ Implementation Report
→ Architecture Review
→ Gate Decision
```

---

# 10. Create `docs/README.md`

Create a specification/document index.

It must list:

- Document filename.
- Version.
- Role.
- Authority area.
- Current presence status.

Suggested columns:

| Document | Version | Role | Authority | Status |
|---|---|---|---|---|

Do not paraphrase large sections of specifications.

---

# 11. Create `docs/SPEC_STATUS.md`

This file is the current specification baseline tracker.

Include:

## 11.1 Expected specs

All nine required documents from Section 3.

## 11.2 Status

For each:

```text
FOUND
MISSING
SUPERSEDED
```

For TASK_000 only use FOUND/MISSING unless there is explicit evidence otherwise.

## 11.3 Known cross-document note

Record this known architecture clarification:

> Pixel Arithmetic V0.1 defines Compatibility Tile Mode such that every logical render-target write is quantized according to `DST_FORMAT` before being stored as canonical RGBA in the Tile Buffer. This clarification should eventually be synchronized into a future System Architecture revision; do not autonomously edit System Architecture in TASK_000.

Do not attempt the revision in this task.

## 11.4 Gate A state

State either:

```text
Gate A: READY FOR IMPLEMENTATION
```

only if every required specification is present.

Otherwise:

```text
Gate A: BLOCKED — MISSING SPECIFICATION FILE(S)
```

---

# 12. Git Initialization

Check whether the project root is already a Git repository.

If not:

```bash
git init
```

Do not configure remote repositories.

Do not push anywhere.

Do not alter global Git settings.

---

# 13. `.gitignore`

Create a practical `.gitignore` covering at least:

## 13.1 OS / editor

```text
.DS_Store
Thumbs.db
.vscode/
.idea/
*.swp
```

Do not ignore all `.vscode` files if the user later chooses to commit project settings; for TASK_000 ignoring `.vscode/` is acceptable.

## 13.2 C/C++ / CMake

```text
build/
cmake-build-*/
CMakeFiles/
CMakeCache.txt
compile_commands.json
```

## 13.3 Python

```text
__pycache__/
*.pyc
.venv/
venv/
.pytest_cache/
```

## 13.4 Simulation

Ignore generated:

- waveform files.
- simulation work directories.
- logs.
- coverage databases.

But do not ignore checked-in golden/reference vectors.

## 13.5 FPGA vendor generated data

Ignore generated:

- synthesis output.
- implementation output.
- temporary tool databases.
- programming temporary files.

Do **not** add overly broad rules that could accidentally ignore source RTL or board constraint files.

Because the exact Efinix project format is not yet frozen, keep vendor-specific rules conservative.

## 13.6 Results

Ignore transient local result directories if large, but preserve the ability to commit curated benchmark reports.

Recommended:

```text
results/local/
results/tmp/
```

Do not ignore all of `results/`.

---

# 14. `.gitattributes`

Create:

```text
* text=auto
*.md text eol=lf
*.cpp text eol=lf
*.hpp text eol=lf
*.c text eol=lf
*.h text eol=lf
*.sv text eol=lf
*.svh text eol=lf
*.py text eol=lf
*.json text eol=lf
*.yaml text eol=lf
*.yml text eol=lf
```

Mark binary formats if appropriate:

```text
*.png binary
*.jpg binary
*.jpeg binary
*.bin binary
*.raw binary
```

Do not rewrite existing specification line endings during TASK_000 merely because `.gitattributes` was added.

---

# 15. Root CMake Baseline

Create a minimal top-level `CMakeLists.txt`.

Requirements:

```cmake
cmake_minimum_required(VERSION 3.20)
project(fpga_2d_gpu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_GOLDEN "Build the PC Golden GPU model" ON)

if(BUILD_GOLDEN)
    add_subdirectory(model/golden)
endif()
```

No external dependencies in TASK_000.

No FetchContent.

No SDL/OpenGL.

No testing framework dependency yet.

---

# 16. Golden CMake Placeholder

Create:

```text
model/golden/CMakeLists.txt
```

It must configure cleanly but should not build a fake Golden GPU implementation.

Acceptable:

```cmake
# Golden implementation starts in a later task.
```

or an empty INTERFACE target if useful.

Do not create FILL/BLIT implementation.

The top-level CMake configure must succeed.

---

# 17. Create `spec/README.md`

Explain:

- `spec/` is intended for machine-readable single-source constants/spec metadata.
- Future files may generate:
  - C/C++ headers.
  - SystemVerilog packages.
  - documentation tables.
- TASK_000 does not define ISA constants in YAML yet.
- Do not duplicate frozen specifications here manually.

This prevents premature divergence.

---

# 18. Create Repository Preflight Script

Create:

```text
scripts/preflight.py
```

Use only the Python standard library.

It must:

1. Locate project root relative to its own path.
2. Check required specification filenames.
3. Check required top-level directories.
4. Check required baseline files:
   - `README.md`
   - `AGENTS.md`
   - `.gitignore`
   - `.gitattributes`
   - `CMakeLists.txt`
5. Print a readable PASS/WARN/FAIL summary.
6. Return:
   - exit code `0` if repository structure is correct and all required specs are found.
   - exit code `2` if repository structure is correct but required specs are missing.
   - exit code `1` for repository-structure errors.

Do not modify any file.

Example output style:

```text
[PASS] directory: rtl
[PASS] spec: Command ISA V0.1
[MISSING] spec: Pixel Arithmetic V0.1

Repository structure: PASS
Specification preflight: BLOCKED
Exit code: 2
```

---

# 19. Create Baseline Check Script

Create:

```text
scripts/check_baseline.py
```

Standard library only.

It should:

- Run `scripts/preflight.py` or reuse its logic.
- Verify top-level CMake configure succeeds if `cmake` is available.
- If `cmake` is unavailable, print a warning instead of failing structure verification.
- Never install packages automatically.
- Never alter the environment.

Use a temporary build directory under:

```text
build/baseline-check/
```

---

# 20. Do Not Add Third-Party Dependencies

TASK_000 must remain dependency-light.

Do not install:

- SDL.
- GLFW.
- Qt.
- Catch2.
- GoogleTest.
- pytest packages.
- FPGA tools.

The agent may report missing local tools.

---

# 21. Do Not Implement Generated-Spec Infrastructure Yet

Do not create real:

```text
gpu_isa.yaml
gpu_regs.yaml
generator.py
```

in TASK_000.

Reason:

The exact machine-readable schema deserves its own reviewed task.

Only prepare:

```text
spec/
spec/generated/
```

---

# 22. Task / Report Workflow Files

Create directories:

```text
docs/tasks/
docs/reports/
docs/reviews/
docs/decisions/
```

Create small README files in these directories if useful.

Meaning:

```text
tasks/      Architect-issued implementation tasks
reports/    Agent execution reports
reviews/    Reviewer findings and gate decisions
decisions/  Approved architecture/design decisions
```

Do not create fake review approvals.

---

# 23. Project Status File

Create:

```text
docs/PROJECT_STATUS.md
```

Suggested structure:

```text
Current Stage
Current Gate
Last Completed Task
Open Blockers
Open Design Questions
Next Planned Task
```

For TASK_000 completion:

```text
Current Stage: Project Initialization
Last Completed Task: TASK_000
Next Planned Task: Golden G0/G1 bootstrap, pending review
```

If specs are missing:

```text
Current Gate: Gate A BLOCKED
```

Otherwise:

```text
Current Gate: Gate A specification baseline present; awaiting TASK_000 review
```

Do not declare Gate A formally passed; reviewer decides that.

---

# 24. Initial Toolchain Inventory

Without installing anything, record whether the following commands exist:

```text
git
python
cmake
c++
g++
clang++
```

On Windows, also note if:

```text
cl
```

is available.

Do not require every compiler to exist.

Record versions when possible.

Put results in the implementation report.

---

# 25. Initial Git Baseline

After files are created and checks are complete:

Run:

```bash
git status
```

If this repository was newly initialized and there are no user-defined Git conventions preventing it, create an initial commit:

```text
chore: initialize FPGA 2D GPU repository
```

However:

- Do not commit if Git identity is not configured.
- Do not alter Git identity automatically.
- If commit cannot be created, record that in the report.
- Never push.

If the repository already had commits, do not rewrite history.

---

# 26. Acceptance Criteria

TASK_000 is complete only when all applicable checks below pass.

## 26.1 Structure

- [ ] Required directory structure exists.
- [ ] Existing specs were not modified.
- [ ] `README.md` exists.
- [ ] `AGENTS.md` exists.
- [ ] `docs/README.md` exists.
- [ ] `docs/SPEC_STATUS.md` exists.
- [ ] `docs/PROJECT_STATUS.md` exists.
- [ ] `.gitignore` exists.
- [ ] `.gitattributes` exists.
- [ ] root `CMakeLists.txt` exists.
- [ ] `model/golden/CMakeLists.txt` exists.
- [ ] `scripts/preflight.py` exists.
- [ ] `scripts/check_baseline.py` exists.

## 26.2 Checks

- [ ] `python scripts/preflight.py` runs.
- [ ] Preflight exit code is understood and documented.
- [ ] `python scripts/check_baseline.py` runs.
- [ ] CMake configure succeeds if CMake is installed.
- [ ] No GPU functional code has been added.
- [ ] No specification was silently changed.
- [ ] Git status is documented.
- [ ] Toolchain inventory is documented.

## 26.3 Specification state

If all nine required specs are present:

```text
SPEC PREFLIGHT = PASS
```

If any are missing:

```text
SPEC PREFLIGHT = BLOCKED
```

TASK_000 may still be considered initialization-complete with a blocker, but TASK_001 functional work must not start until the required missing specs are supplied.

---

# 27. Mandatory Verification Commands

Run from repository root.

Use the platform-appropriate Python executable.

Preferred:

```bash
python scripts/preflight.py
python scripts/check_baseline.py
git status
```

If CMake exists, also run explicitly:

```bash
cmake -S . -B build/task000
cmake --build build/task000
```

The build may have no compiled targets; configuration must still succeed.

Do not hide command failures.

---

# 28. Implementation Report

Create:

```text
docs/reports/REPORT_000_Project_Initialization.md
```

Use this structure.

```markdown
# REPORT_000 — Project Initialization

## 1. Result
PASS / PASS_WITH_BLOCKERS / FAIL

## 2. Repository Before Task
Brief description.

## 3. Specification Preflight
Table of required specs and status.

## 4. Files Created
List.

## 5. Files Modified
List.
Existing spec files should show: NONE.

## 6. Directory Structure Created
Summary.

## 7. Toolchain Inventory
git:
python:
cmake:
compiler:

## 8. Commands Executed
Exact commands and exit codes.

## 9. Verification Results
preflight:
baseline:
cmake:
git:

## 10. Git Status / Commit
Repository initialized?
Commit created?
Commit hash?

## 11. Blockers
NONE or explicit blockers.

## 12. Design Questions
NONE or explicit questions.

## 13. Deviations from Task
NONE or detailed explanation.

## 14. Suggested Next Task
Do not implement it; only state suggestion.
```

---

# 29. Stop Conditions

Stop and report instead of improvising if any of the following occurs:

1. Existing specification documents appear corrupted.
2. Two required spec files have ambiguous duplicate versions.
3. Existing repository already contains implementation code that conflicts with this initialization plan.
4. Git repository contains uncommitted user work that could be overwritten.
5. A requested file already exists with materially different content.
6. The repository root cannot be confidently determined.
7. A specification appears to require changing another authoritative document.
8. A command would delete or overwrite user data.

For non-destructive issues, continue only with unaffected initialization work and mark the blocker.

---

# 30. Explicit Non-Goals

TASK_000 must NOT:

- Write Golden pixel math.
- Write a GPU command parser.
- Implement FILL.
- Implement BLIT.
- Implement a framebuffer renderer.
- Add SDL/OpenGL.
- Add unit test frameworks.
- Write SystemVerilog.
- Create Efinix projects.
- Create board constraints.
- Create a real performance model.
- Create machine-generated ISA constants.
- Modify frozen specification semantics.

---

# 31. Expected Reviewer Handoff

When complete, provide the reviewer with:

1. `REPORT_000_Project_Initialization.md`
2. `git status`
3. commit hash if created
4. `python scripts/preflight.py` output
5. `python scripts/check_baseline.py` output
6. CMake configure/build output
7. repository tree (2–3 levels deep)
8. any blockers/design questions

The reviewer will issue one of:

```text
PASS
PASS WITH ACTIONS
FAIL / REWORK
ARCHITECTURE CHANGE REQUIRED
```

Only after reviewer approval should the next functional task begin.

---

# 32. Expected Next Stage

The expected next functional stage, subject to review, is:

> **Golden GPU G0/G1 Bootstrap**

Likely scope:

- `gpu_types`
- `gpu_math`
- `MemoryImage`
- `Surface`
- exhaustive arithmetic tests
- first `FILL_RECT` Golden path

Do not start this stage during TASK_000.

---

# 33. Final Instruction to Local Agent

Execute this task conservatively.

The goal is not to maximize code volume.

The goal is to produce:

> **a clean, reviewable, reproducible project baseline from which every later Golden, RTL, software, verification, and FPGA task can proceed without repository-level ambiguity.**
