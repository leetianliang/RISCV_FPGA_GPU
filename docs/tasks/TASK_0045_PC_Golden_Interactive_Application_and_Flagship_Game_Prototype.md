# TASK_0045 — PC Golden Interactive Application Framework & Flagship Game Prototype

> Project: RISC-V + FPGA 2D GPU  
> Stage: 004.5 — PC Golden Interactive Application / System Integration  
> Task type: Application integration + visual demo + system-level Golden verification  
> Status: READY FOR IMPLEMENTATION  
> Architecture owner: ChatGPT / Project Owner  
> Implementation owner: Local Agent  
> Suggested repository path: `docs/tasks/TASK_0045_PC_Golden_Interactive_Application_and_Flagship_Game_Prototype.md`

---

# 0. Stage Context

Stage 004 has passed the Golden Tile Architecture Gate.

The accepted Golden baseline provides the functional authority for:

```text
FILL
BLIT
Color Key
Global Alpha
Per-Pixel Alpha
Straight Alpha
Premultiplied Alpha
Additive Blend
Color Mod
Nearest Scaling
Bilinear Scaling
Clamp / Repeat
Clip
Indexed8 + Palette
RGB565 Dither
Immediate Rendering
Software Tile Binning
TILE_FRAME
Tile Load / Render / Store
Per-pixel Overdraw Statistics
```

Stage 004 established the central proof:

> For the same legal ordered Draw2D stream and the same initial framebuffer, Immediate Mode and Tile Mode produce byte-identical final framebuffer output.

This task **does not add new GPU rendering semantics**.

This task exists to transform the accepted Golden GPU from a test-oriented functional model into a visible, interactive application platform so the project team can directly inspect the approximate final competition output before RTL implementation.

The application-layer planning authority is:

> `RISC-V_FPGA_2D_GPU_Application_Layer_Demo_Plan_V1.0.md`

This task implements only the first major application defined by that plan:

> **Application 1 — Survivor + Bullet-Hell flagship game prototype**

Other applications remain future work.

---

# 1. Baseline

Recommended START_COMMIT at task issue time:

```text
f034f01ba913c44ce749e7756bd682008f75d08a
```

If implementation starts from a later repository state, REPORT_0045 must record the actual exact 40-hex `START_COMMIT`.

Stage 004 artifacts remain authoritative and MUST NOT be weakened to simplify application development.

The implementation MUST preserve:

```text
Golden pixel arithmetic
Command ISA semantics
Immediate/Tile equality
Tile work ordering
DST_FORMAT compatibility quantization
strict validation behavior
checked fixtures
existing Stage-004 tests
```

---

# 2. Stage Goal

At Stage 004.5 completion, the repository MUST contain a PC application that can:

```text
launch a real window
accept keyboard input
run a deterministic 2D game simulation
submit all visible game rendering through a common Graphics API
execute the same game through the Golden Immediate backend
execute the same game through the Golden Tile backend
display the Golden framebuffer in the PC window
switch Immediate/Tile at runtime
display GPU/application telemetry
display Tile/Overdraw architecture visualization
run deterministic stress scenes
run in headless mode for automated testing
capture reproducible reference frames
```

The result should let the project owner launch an executable and visually inspect:

> **approximately what the final FPGA HDMI demonstration can look like.**

The PC application is a **functional visualization platform**, not a predictor of final FPGA FPS.

---

# 3. Stage Completion Contract

Stage 004.5 completion uses the same evidence discipline as Stage 004.

> **Feature implementation ≠ Stage completion.**

A mandatory Acceptance ID is complete only when it has:

```text
Implementation Evidence
+
Verification Evidence
+
Exact Test Result
```

Rules:

> **No evidence → NOT DONE.**

> **Partial evidence → NOT DONE.**

> **“Covered by a similar test” without exact mapping → NOT DONE.**

> **A visually impressive screenshot does not replace automated correctness tests.**

> **A passing test suite does not replace the required interactive demo.**

The Stage result may be:

```text
PASS
PASS WITH ACTIONS
FAIL
```

Only `PASS` or reviewer-approved `PASS WITH ACTIONS` closes Stage 004.5.

---

# 4. Primary Architectural Rule

The application must preserve:

> **Game is software; GPU is hardware.**

Application code MUST NOT know or manipulate:

```text
GoldenGPU
MemoryImage
TileHeader
WorkRef
TILE_FRAME internals
AXI
DDR
FPGA registers
```

The legal dependency is:

```text
Game / Application
        │
        ▼
Common Graphics API
        │
        ▼
Renderer Front-End / Command Recorder
        │
        ├───────────────┐
        ▼               ▼
Golden Immediate   Golden Tile
Backend             Backend
        │               │
        └───────┬───────┘
                ▼
          Golden Framebuffer
                │
                ▼
           PC Presenter
```

Later FPGA migration must replace only the renderer/backend side:

```text
Application
    ↓
same Graphics API
    ↓
RISC-V Driver
    ↓
FPGA GPU
```

---

# 5. Frozen Decisions

## FD-01 — Golden Core remains GUI-independent

`model/golden/` MUST NOT acquire SDL, windowing, input, or platform UI dependencies.

Window/input/presentation belongs to a new PC host layer.

---

## FD-02 — PC host is not a renderer

The window library may:

```text
create a window
receive input
upload/copy the completed framebuffer
present the framebuffer
measure host wall-clock time
```

It MUST NOT render game content using host drawing APIs.

Forbidden for final visible scene rendering:

```text
SDL_RenderDraw*
SDL_RenderGeometry
OpenGL draw calls
Direct2D drawing
CPU-generated game overlays directly into the presenter
```

All visible gameplay, HUD, Tile grid, debug labels, and normal X-Ray overlays intended for the FPGA demo MUST be generated through the project Graphics API / framebuffer path.

A host-only debug window title is allowed.

---

## FD-03 — Default competition render target is RGB565

Application 1 should use an RGB565 main render target by default to stay aligned with the competition target.

Textures may use:

```text
RGB565
ARGB8888
Indexed8 + Palette
```

as required by the visual scene.

---

## FD-04 — PC display format is presentation-only

The PC presenter may convert the completed RGB565 framebuffer to a host-native 32-bit display texture.

That conversion is not a GPU operation and MUST occur only after Golden rendering is complete.

It MUST NOT modify Golden render semantics.

---

## FD-05 — Application resolution profiles

Support at least:

```text
Interactive Profile:
640 × 360 internal render
presented at a larger window size if desired

Showcase Profile:
1280 × 720 internal render
```

The Stage does NOT require 1280×720 Golden rendering to run at 60 FPS.

The Showcase Profile exists to preview final visual composition and capture deterministic frames.

---

## FD-06 — PC Golden FPS is not FPGA performance evidence

Any PC HUD that shows host FPS MUST be clearly identified as:

```text
PC GOLDEN HOST FPS
```

or equivalent.

It MUST NOT be labeled as GPU hardware FPS.

Final FPGA performance metrics remain a later hardware task.

---

## FD-07 — First flagship game style

Application 1 is:

> **Survivor + Bullet-Hell, neon sci-fi / cyber arena style**

Provisional title:

> **NEON SURVIVOR**

The name is not architecturally frozen and may be changed later.

---

## FD-08 — Deterministic simulation mode

The game MUST support:

```text
fixed seed
fixed timestep
scripted/no-input execution
headless execution
```

so the same frame can be reproduced for regression testing.

Interactive mode may use real keyboard input.

---

## FD-09 — Runtime renderer selection

The same application must support runtime selection:

```text
Immediate
Tile32
```

Tile16/Tile64 are optional interactive modes but should remain available to tests/tools if easy.

The game simulation and Graphics API call sequence MUST NOT be rewritten when switching backend.

---

## FD-10 — Visible UI uses GPU rendering

HUD text should use a simple checked bitmap font atlas or equivalent GPU-rendered glyph sprite approach.

Do NOT use SDL_ttf or host-native text as the only final HUD implementation.

Host text may be used temporarily during Block A bring-up only and must not survive the final Stage implementation.

---

## FD-11 — Assets

The Stage must use repository-local, legally usable assets.

Allowed:

```text
original procedural assets
project-created pixel art
CC0/public-domain assets with attribution
```

Preferred for Stage 004.5:

> simple project-created/procedural neon assets.

Do NOT make the Stage depend on downloading assets at runtime.

---

## FD-12 — No Golden semantic changes for visual convenience

If the application exposes a Golden bug or missing semantic capability:

```text
STOP
→ report blocker
→ architecture owner decides whether Golden/spec must change
```

Do NOT silently change expected pixel behavior to make the demo easier.

---

# 6. Proposed Repository Structure

The implementation should converge toward:

```text
software/
├── graphics/
│   ├── include/
│   │   └── gpu2d/
│   │       ├── graphics_api.hpp
│   │       ├── renderer.hpp
│   │       ├── resource.hpp
│   │       ├── telemetry.hpp
│   │       └── types.hpp
│   ├── src/
│   │   ├── command_recorder.cpp
│   │   ├── resource_manager.cpp
│   │   └── ...
│   └── tests/
│
└── applications/
    └── neon_survivor/
        ├── include/
        ├── src/
        │   ├── game.cpp
        │   ├── player.cpp
        │   ├── enemy.cpp
        │   ├── projectile.cpp
        │   ├── particle.cpp
        │   ├── hud.cpp
        │   └── stress.cpp
        └── tests/

model/
└── pc_demo/
    ├── host/
    │   ├── pc_window.*
    │   ├── input.*
    │   └── presenter.*
    ├── golden_backend/
    │   ├── golden_renderer.*
    │   └── golden_telemetry.*
    └── app/
        └── main.cpp

assets/
└── neon_survivor/
    ├── sprites/
    ├── palette/
    ├── font/
    └── metadata/

results/
└── stage0045/
    ├── captures/
    ├── hashes/
    └── stress/
```

Exact file names may differ, but dependency direction MUST remain equivalent.

---

# 7. Common Graphics API Minimum Surface

The application-facing API MUST expose a backend-neutral surface equivalent to:

```cpp
gpu_begin_frame();

gpu_fill_rect(...);

gpu_draw_sprite(...);

gpu_draw_sprite_key(...);

gpu_draw_sprite_alpha(...);

gpu_draw_sprite_scaled(...);

gpu_draw_sprite_ex(...);

gpu_set_clip(...);

gpu_present();
```

Internally, the API must support at least:

```text
texture handle
destination position
source rectangle
size / scaling
blend mode
global alpha
color modulation
color key
filter mode
address mode
clip state
palette
flip
```

The application MUST use handles/descriptors rather than raw Golden physical addresses.

---

# 8. Renderer Telemetry Interface

Architecture visualization must not force Game Logic to access Golden internals.

Provide a read-only telemetry abstraction such as:

```cpp
struct RendererTelemetry {
    uint32_t command_count;
    uint32_t sprite_count;
    uint32_t workref_count;
    uint32_t tiles_total;
    uint32_t tiles_active;
    uint32_t max_workrefs_per_tile;
    uint32_t max_overdraw;

    // Optional/available only when backend supports it.
    span<const uint16_t> tile_workref_counts;
    span<const uint16_t> overdraw_map;
};
```

Exact type names may differ.

Rules:

```text
Game core does not depend on telemetry.
Demo/HUD/X-Ray layer may consume telemetry.
Missing telemetry must degrade gracefully.
```

This keeps the future FPGA backend compatible even if some debug data differs.

---

# 9. Input Map

Freeze the first prototype controls:

```text
W / A / S / D   Player movement

F1              Sprite Storm
F2              Alpha Storm
F3              Bullet Hell
F4              Scale Storm
F5              Overdraw Storm

F6              Immediate renderer
F7              Tile32 renderer

F8              Toggle technical HUD
F9              Normal view
F10             Architecture X-Ray

P               Pause/unpause simulation
R               Reset current scene using fixed seed
ESC             Return/quit
```

Optional controls may be added but these mappings should remain stable once implemented.

---

# 10. Block A — PC Window / Presenter Bring-Up

## Objective

Create the host application shell that can display a Golden framebuffer without becoming a second renderer.

## Implementation MUST

A-01. Create a PC window.

A-02. Accept keyboard input and clean shutdown.

A-03. Present an RGB565 framebuffer.

A-04. Support integer or aspect-correct scaling from internal render resolution to window resolution.

A-05. Provide headless mode that creates no window.

A-06. Keep all window/presenter dependencies outside `model/golden`.

## Tests MUST

A-T1. RGB565 conversion test with known colors:

```text
black
white
red
green
blue
representative mid-tone
```

A-T2. Presenter must correctly handle at least:

```text
640×360
1280×720
```

framebuffer dimensions.

A-T3. Headless mode must execute without requiring a display server/window.

## Forbidden shortcuts

- Rendering the game using SDL primitives.
- Adding SDL dependency to `golden_core`.
- Using a host GPU API as the actual scene renderer.
- Treating a host test pattern as a completed application.

## Exit Criteria

A block passes only when a Golden-generated or test-generated RGB565 framebuffer is visibly presented in a window and the same program can run headless.

---

# 11. Block B — Common Graphics API / Command Recorder

## Objective

Create the application-facing rendering API that can later survive the transition from PC Golden to RISC-V + FPGA.

## Implementation MUST

B-01. Define backend-neutral graphics types.

B-02. Define texture/resource handles.

B-03. Record an ordered Draw2D command stream per frame.

B-04. Support:

```text
Fill
Sprite
Color Key
Alpha
Additive
Scale
Filter select
Clip
Color Mod
Palette
```

B-05. Support `begin_frame / present`.

B-06. Keep raw Golden addresses hidden from the application.

B-07. Make draw order deterministic.

B-08. Permit the same recorded application calls to feed Immediate or Tile execution.

## Tests MUST

B-T1. One API call sequence must produce the expected count/order of underlying commands.

B-T2. Resource handles must map deterministically.

B-T3. Invalid/unloaded handles must fail deterministically; no silent fallback.

B-T4. Two runs with the same inputs must generate byte-identical command streams.

## Forbidden shortcuts

- Game code constructing `GpuCmd64` directly.
- Game code including Golden private headers.
- API methods that are secretly hardcoded to NEON SURVIVOR entity types.
- API names such as `draw_enemy`, `draw_bullet`, `draw_boss`.

## Exit Criteria

A unit test must show:

```text
application-level calls
→ deterministic generic Draw2D command bytes
```

without application access to Golden internals.

---

# 12. Block C — Golden Renderer Backend

## Objective

Connect the new Graphics API to the accepted Stage-004 Golden model.

## Implementation MUST

C-01. Implement an Immediate backend.

C-02. Implement a Tile32 backend.

C-03. Both backends must consume the same application command stream.

C-04. Tile backend must use the accepted software binner / serialized Tile path.

C-05. Expose final framebuffer to the presenter.

C-06. Expose RendererTelemetry through a backend-neutral telemetry interface.

C-07. Permit runtime switching Immediate ↔ Tile at a frame boundary.

C-08. Resource upload/registration must be hidden behind the backend/resource layer.

## Tests MUST

C-T1. Fixed application test scene:

```text
Immediate framebuffer == Tile framebuffer byte-for-byte
```

C-T2. Runtime switch sequence:

```text
Immediate
→ Tile
→ Immediate
```

must not alter deterministic simulation state or corrupt resources.

C-T3. Same application frame must have equal final framebuffer hashes in Immediate and Tile mode.

C-T4. Backend test must exercise at least:

```text
Alpha
Additive
Scale
Palette
Clip
```

in one integrated scene.

## Forbidden shortcuts

- Separate game draw code for Immediate and Tile.
- Tile backend calling a different visual asset set.
- Comparing screenshots by eye instead of bytes.
- Host-side correction of framebuffer differences.

## Exit Criteria

A checked automated integrated scene must prove Immediate/Tile exact equality through the new application-facing API.

---

# 13. Block D — Application Shell & Deterministic Simulation

## Objective

Create the reusable application loop and deterministic game simulation foundation.

## Implementation MUST

D-01. Fixed-timestep simulation.

Recommended logical tick:

```text
60 Hz
```

D-02. Fixed-seed deterministic RNG.

D-03. Input abstraction separating:

```text
interactive keyboard input
scripted/headless input
```

D-04. Entity update/render separation.

D-05. Scene reset.

D-06. Pause.

D-07. Resolution/profile configuration.

D-08. Headless `N`-frame execution.

## Tests MUST

D-T1. Same seed + same scripted inputs for N frames produce identical simulation hashes.

D-T2. Reset restores the deterministic initial state.

D-T3. Headless 300-frame execution completes without crash.

D-T4. Rendering disabled vs enabled must not change gameplay simulation state.

## Forbidden shortcuts

- Game state depending on host wall-clock timing.
- RNG seeded from current time in deterministic mode.
- Renderer state modifying gameplay logic.
- Hidden global state that prevents reset.

## Exit Criteria

The simulation can be replayed exactly from:

```text
seed
+
input script
+
frame count
```

---

# 14. Block E — NEON SURVIVOR Core Game

## Objective

Build the first playable flagship application.

## Required Game Content

### Player

E-01. WASD movement.

E-02. HP state.

E-03. Visual player sprite.

E-04. Automatic attack or deterministic attack behavior.

### Enemies

E-05. Normal Enemy.

E-06. Fast Enemy.

E-07. Heavy Enemy.

Each must differ in at least two of:

```text
speed
size
HP
visual treatment
spawn behavior
```

### Projectiles

E-08. Straight projectile.

E-09. Radial burst or equivalent spread pattern.

E-10. Spiral/bullet-hell pattern.

### Game Loop

E-11. Enemy spawning.

E-12. Basic collision/damage.

E-13. Kill/removal.

E-14. Score or kill counter.

## Visual/Architecture Rule

All game rendering MUST use the Common Graphics API.

## Tests MUST

E-T1. Scripted 600-frame game run.

E-T2. At least one deterministic enemy kill.

E-T3. Projectile/enemy counts match expected checkpoints for fixed seed.

E-T4. Immediate/Tile frame hashes match at selected checkpoints.

## Forbidden shortcuts

- Pre-rendered game video.
- Hardcoded enemy pixels in the host presenter.
- GPU model containing game logic.
- Game-specific opcodes.
- Game-specific Tile behavior.

## Exit Criteria

The executable must be genuinely playable:

```text
move
survive
shoot
spawn enemies
kill enemies
```

and render through both Golden backends.

---

# 15. Block F — Visual Effects, Assets & HUD

## Objective

Make the scene visually representative of a competition demo while deliberately exercising the GPU feature set.

## Required Effects

F-01. Explosion particles.

F-02. Additive glow.

F-03. Alpha-fade particles/trails.

F-04. Scaling effect.

F-05. At least one Bilinear-filtered visual effect.

F-06. Damage Flash using Color Mod and/or Palette variation.

F-07. At least one Indexed8 + Palette asset path.

F-08. At least one Color Key sprite path.

F-09. RGB565 Dither enabled in the default RGB565 competition profile.

F-10. Multi-layer/parallax-style background or equivalent scene depth.

## HUD MUST display at least

```text
HP
game time or score
enemy count
bullet count
particle count
renderer mode
```

Technical HUD, when enabled, must additionally display available values such as:

```text
Command Count
WorkRef Count
Active Tiles
Max Overdraw
PC Golden Host FPS
```

## Font Requirement

HUD text intended for final demo must be rendered using the GPU path, preferably a bitmap font atlas.

## Tests MUST

F-T1. Visual-effect integrated reference frame hash.

F-T2. Each required GPU feature is exercised by an automated application test or deterministic capture scene.

F-T3. Technical HUD values must match backend telemetry for a fixed test frame.

## Forbidden shortcuts

- Host-rendered final HUD text.
- Fake counters unrelated to renderer telemetry.
- “Glow” implemented as a pre-baked single opaque sprite only.
- Using only Fill rectangles while claiming Sprite/Alpha demonstration.

## Exit Criteria

One deterministic showcase frame must contain simultaneously:

```text
textured sprites
Alpha
Additive
Scaling
UI
particles
technical telemetry
```

and be capturable at 1280×720.

---

# 16. Block G — Architecture X-Ray

## Objective

Turn internal GPU architecture into visible competition content.

## X-Ray MUST support

G-01. Tile Grid Overlay.

G-02. Per-Tile WorkRef visualization.

G-03. Active Tile visualization.

G-04. Overdraw visualization when available.

G-05. Renderer mode label.

G-06. Normal ↔ X-Ray runtime toggle.

## Design Rule

X-Ray is a demo/debug layer, not Game Logic.

It may consume `RendererTelemetry`, but normal game simulation must not depend on telemetry.

## Rendering Rule

Visible X-Ray graphics should be drawn through the Graphics API whenever practical so they can later migrate to FPGA.

Telemetry data may be generated backend-side.

## Tests MUST

G-T1. Fixed scene produces known Tile-grid dimensions.

G-T2. Fixed scene must have at least:

```text
one inactive Tile
one low-work Tile
one high-work Tile
```

G-T3. X-Ray toggle must not alter simulation state.

G-T4. X-Ray ON/OFF may alter final overlay pixels, but scene render before overlay must remain deterministic.

## Forbidden shortcuts

- Replacing architecture visualization with static explanatory images.
- Hardcoding fake heatmaps.
- Reading Golden internals directly from game core.

## Exit Criteria

A reviewer watching the PC demo must be able to toggle from:

```text
normal game
```

to:

```text
visible Tile/WorkRef/Overdraw architecture view
```

without restarting.

---

# 17. Block H — Stress / Benchmark Modes

## Objective

Create deterministic scenes that visually correspond to the architectural pressure cases.

## Required Runtime Modes

### H-01 — Sprite Storm

High Sprite count.

Target visual prototype range:

```text
500+
```

No PC real-time FPS requirement.

### H-02 — Alpha Storm

Large number of semi-transparent particles/sprites.

### H-03 — Bullet Hell

Large projectile count.

### H-04 — Scale Storm

Large number of scaled sprites; must include Nearest and/or Bilinear stress.

### H-05 — Overdraw Storm

Many overlapping sprites/effects concentrated in a smaller region.

## Telemetry MUST record

```text
entity counts
draw command count
WorkRef count where available
active Tile count where available
max overdraw where available
host frame time / PC Golden Host FPS
```

## Important Labeling Rule

PC stress results MUST NOT be presented as final FPGA performance.

Output/report must label them:

> **Functional PC Golden workload statistics**

## Tests MUST

H-T1. Each stress mode can run deterministically headless.

H-T2. Each mode must exceed a defined workload threshold.

Recommended initial thresholds:

```text
Sprite Storm:   >= 500 visible Sprite draws
Bullet Hell:    >= 1000 projectile entities or draws
Alpha Storm:    >= 300 alpha-blended draws
Scale Storm:    >= 200 scaled draws
Overdraw Storm: max_overdraw >= 8 in Golden telemetry
```

Thresholds may be reduced only with explicit architecture-owner approval.

H-T3. Stress scene command stream must remain legal.

H-T4. Immediate/Tile equality must hold for at least one deterministic frame from every stress mode.

## Forbidden shortcuts

- Claiming “2000 sprites” using invisible/off-screen entities that generate no draw command.
- Counting particles that are not submitted.
- Using host FPS as FPGA performance evidence.

## Exit Criteria

F1–F5 can switch into visibly distinct pressure scenes and the same scenes run in headless automated verification.

---

# 18. Block I — Demo Launcher / User Experience

## Objective

Make the PC prototype easy to run and easy to demonstrate.

## Implementation MUST

I-01. Provide one top-level executable.

Recommended name:

```text
gpu2d_demo
```

or equivalent.

I-02. Provide at least a simple startup screen/menu with:

```text
NEON SURVIVOR
GPU PLAYGROUND / placeholder
ARCHITECTURE X-RAY entry or help
BENCHMARK / STRESS entry
```

Only NEON SURVIVOR must be fully implemented in this Stage.

I-03. Display controls/help.

I-04. Clean quit.

I-05. Provide CLI options for automation, at minimum conceptually equivalent to:

```text
--headless
--frames N
--seed N
--backend immediate|tile
--profile interactive|showcase
--scene game|sprite|alpha|bullet|scale|overdraw
--capture <path>
```

Exact option names may differ but must be documented.

## Tests MUST

I-T1. CLI invalid arguments fail clearly.

I-T2. Every required scene can be launched headless by CLI.

I-T3. `--capture` produces a deterministic framebuffer/image artifact.

## Exit Criteria

A new developer can build the repository, run one documented command, and enter the PC demo.

---

# 19. Block J — System Verification / Application Golden Gate

## Objective

Prove the application framework is not merely visually functional but remains a valid consumer of the Stage-004 Golden architecture.

## Mandatory System Tests

J-01. **Application Immediate-vs-Tile Exact Test**

For at least 100 deterministic application frames:

```text
same simulation state
same API submissions
same initial framebuffer/resources

Immediate final framebuffer
==
Tile final framebuffer
byte-for-byte
```

Include:

```text
normal gameplay
Alpha
Additive
Scaling
Clip
Palette
```

J-02. **Long Headless Stability**

Run at least:

```text
1000 simulation frames
```

without crash/resource corruption.

This may use a moderate workload profile.

J-03. **Backend Switch Stability**

Within one process:

```text
Immediate → Tile → Immediate
```

with scene continuity.

J-04. **Deterministic Capture**

For fixed:

```text
seed
scene
frame number
backend
```

two separate runs must produce identical framebuffer bytes/hash.

J-05. **Game/Golden Dependency Boundary**

Machine-check or static audit that:

```text
software/applications/neon_survivor
```

does not include/use forbidden Golden internals.

J-06. **Presenter Purity**

Audit that the presenter does not draw the game using host primitives.

J-07. **Existing Golden Regression**

All pre-existing Stage-004 tests must remain passing.

## Mutation / Integrity Rule

At least one application-level comparison test should be demonstrated to fail when a deliberate one-pixel or command mutation is introduced in the test harness.

Do not commit the mutation.

## Exit Criteria

The PC application must be accepted as a real system-level Golden workload, not an independent renderer.

---

# 20. Block K — Acceptance / Evidence / Report

## Objective

Prevent the application stage from becoming “looks good on my machine” work.

## Required Artifacts

K-01.

```text
docs/tasks/STAGE_0045_ACCEPTANCE.json
```

K-02.

```text
scripts/check_stage0045_acceptance.py
```

K-03.

```text
docs/reports/REPORT_0045_PC_Golden_Interactive_Application.md
```

K-04. Deterministic captures under:

```text
results/stage0045/captures/
```

K-05. Stress statistics under:

```text
results/stage0045/stress/
```

## Checker MUST

- own the authoritative Acceptance ID list;
- reject missing IDs;
- reject duplicates;
- reject Mandatory status other than PASS;
- verify concrete evidence paths;
- validate declared CTest names against `ctest -N`;
- reject placeholder evidence;
- require an exact 40-hex START_COMMIT and END_COMMIT in REPORT_0045;
- verify the Report contains one row per Acceptance ID.

## Report MUST

contain:

```text
Result
START_COMMIT
END_COMMIT
Build environment
Window/backend dependency version
Exact test commands
Exact test counts/results
Application controls
Implemented scenes
Known limitations
47+/all acceptance rows
Capture paths
Stress statistics paths
```

The exact number of IDs in Stage 004.5 is defined below.

---

# 21. Mandatory Acceptance IDs

The authoritative IDs for Stage 004.5 are:

```text
HOST-01 .. HOST-05      5
API-01  .. API-06       6
BACK-01 .. BACK-06      6
SIM-01  .. SIM-05       5
GAME-01 .. GAME-06      6
FX-01   .. FX-06        6
XR-01   .. XR-05        5
STR-01  .. STR-06       6
SYS-01  .. SYS-07       7
AUD-01  .. AUD-04       4
--------------------------------
TOTAL                    56
```

Every ID is mandatory.

---

# 22. Acceptance Requirements

## HOST

### HOST-01
PC window opens and displays a project-owned RGB565 framebuffer.

### HOST-02
Input and clean shutdown work.

### HOST-03
640×360 and 1280×720 paths are supported.

### HOST-04
Headless mode requires no interactive window.

### HOST-05
Golden Core remains free of window/SDL dependencies.

---

## API

### API-01
Common Graphics API exists and is application-facing.

### API-02
Game code uses resource handles, not Golden physical addresses.

### API-03
Fill/Sprite/Alpha/Additive/Scale/Clip/Palette semantics can be expressed.

### API-04
Command order is deterministic.

### API-05
API command stream is backend-neutral.

### API-06
Application code contains no game-specific GPU opcode or direct GpuCmd construction.

---

## BACK

### BACK-01
Immediate backend executes application command streams.

### BACK-02
Tile32 backend executes the same application command streams.

### BACK-03
Integrated application scene Immediate == Tile byte-for-byte.

### BACK-04
Runtime backend switching is implemented.

### BACK-05
Resource upload/registration is backend-owned.

### BACK-06
RendererTelemetry abstraction exists.

---

## SIM

### SIM-01
Fixed-timestep simulation.

### SIM-02
Fixed-seed deterministic mode.

### SIM-03
Scripted/headless input path.

### SIM-04
Reset reproduces initial state.

### SIM-05
Rendering backend does not alter gameplay simulation state.

---

## GAME

### GAME-01
Player movement and HP state.

### GAME-02
Normal/Fast/Heavy enemy classes.

### GAME-03
Straight projectile.

### GAME-04
Radial/spread projectile pattern.

### GAME-05
Spiral/bullet-hell pattern.

### GAME-06
Spawn/damage/kill/score loop is playable.

---

## FX

### FX-01
Alpha-fade particle/trail.

### FX-02
Additive glow/explosion.

### FX-03
Scaling and at least one bilinear effect.

### FX-04
Color Mod and/or Palette-based damage/variant effect.

### FX-05
Color Key + Indexed8/Palette paths are both exercised by the application.

### FX-06
GPU-rendered HUD exists with gameplay and technical telemetry.

---

## XR

### XR-01
Tile grid visualization.

### XR-02
Per-Tile WorkRef visualization.

### XR-03
Active Tile visualization.

### XR-04
Overdraw visualization when Golden telemetry is available.

### XR-05
Normal/X-Ray switching does not affect simulation state.

---

## STR

### STR-01
Sprite Storm.

### STR-02
Alpha Storm.

### STR-03
Bullet Hell.

### STR-04
Scale Storm.

### STR-05
Overdraw Storm.

### STR-06
Stress telemetry and thresholds are machine-verified and explicitly labeled as PC Golden workload statistics.

---

## SYS

### SYS-01
At least 100 deterministic application frames prove Immediate==Tile exact equality.

### SYS-02
1000-frame headless stability run.

### SYS-03
Immediate→Tile→Immediate switch stability.

### SYS-04
Deterministic frame capture/hash.

### SYS-05
Game code does not depend on Golden internals.

### SYS-06
Presenter does not host-render game content.

### SYS-07
All existing Stage-004 regressions remain passing.

---

## AUD

### AUD-01
56-ID acceptance manifest with concrete evidence.

### AUD-02
Strict acceptance checker validates files and CTest names.

### AUD-03
REPORT_0045 contains exact START/END commits and full 56-row evidence matrix.

### AUD-04
Deterministic showcase captures and stress outputs are checked/generated reproducibly.

---

# 23. Machine-Checkable Acceptance

The Agent must provide exact commands used on its platform.

The final workflow should be conceptually equivalent to:

```bash
cmake -S . -B build/stage0045
cmake --build build/stage0045 --config Release

ctest --test-dir build/stage0045 --output-on-failure

python scripts/check_stage0045_acceptance.py
```

Application-level commands should support equivalents of:

```bash
gpu2d_demo --headless --frames 300 --seed 1234 --backend immediate --scene game

gpu2d_demo --headless --frames 300 --seed 1234 --backend tile --scene game

gpu2d_demo --headless --frames 1 --seed 1234 --backend tile \
    --profile showcase --capture results/stage0045/captures/showcase_frame.raw

gpu2d_demo --headless --frames 60 --scene sprite
gpu2d_demo --headless --frames 60 --scene alpha
gpu2d_demo --headless --frames 60 --scene bullet
gpu2d_demo --headless --frames 60 --scene scale
gpu2d_demo --headless --frames 60 --scene overdraw
```

Exact executable layout may vary.

A dedicated CTest should own major automated modes rather than relying only on manual CLI invocation.

---

# 24. Visual Acceptance

Automated pixel tests are required, but this Stage also has explicit visual acceptance.

The Agent must produce deterministic captures demonstrating:

## Capture V1 — Normal Gameplay

Must visibly contain:

```text
player
multiple enemy types
projectiles
particles
HUD
background
```

## Capture V2 — Effects Showcase

Must visibly contain:

```text
Alpha
Additive Glow
Scaling
Bilinear effect
Palette/Color Mod variation
```

## Capture V3 — Architecture X-Ray

Must visibly contain:

```text
Tile grid
WorkRef visualization
Overdraw or active-Tile visualization
technical HUD
```

## Capture V4 — Stress Scene

Must visibly contain high entity density.

Recommended showcase capture resolution:

```text
1280 × 720
```

Captures may be raw + PPM/BMP for convenience.

Reference images must be generated from the actual Golden framebuffer, not composited externally.

---

# 25. Performance / Responsiveness Rule

This Stage does NOT require:

```text
1280×720 @ 60 FPS on PC Golden
```

The Golden model is functional, not performance-optimized hardware.

However:

- the interactive profile must remain usable enough for direct visual inspection;
- input/event processing must not intentionally freeze indefinitely;
- stress scenes may render slowly;
- headless deterministic verification is the authority.

Do not optimize Golden arithmetic in a way that changes Stage-004 bit-exact behavior merely to improve PC FPS.

---

# 26. Non-Goals

This Stage explicitly does NOT include:

```text
RTL
FPGA synthesis
AXI
DDR controller simulation
hardware FIFO timing
Command Ring hardware timing
VSYNC hardware
HDMI
RISC-V driver
final FPGA FPS
audio engine
networking
save system
complex RPG progression
complex physics
advanced pathfinding
3D
Mode-7
final Application 2
final Application 3
```

---

# 27. Forbidden Shortcuts — Stage-Wide

The following are Stage-failing shortcuts.

## FS-01
Using SDL/OpenGL/host drawing to render gameplay instead of the Golden framebuffer.

## FS-02
Game code directly calls GoldenGPU or manipulates Tile structures.

## FS-03
Separate game rendering code paths for Immediate and Tile.

## FS-04
Visual comparison only; no byte-exact application-level Immediate/Tile test.

## FS-05
Claiming PC host FPS as FPGA performance.

## FS-06
Fake counters or hardcoded Tile heatmaps.

## FS-07
Stress entity counts include entities that produce no actual draw submission.

## FS-08
Changing Stage-004 tests/expected pixels only to match new application code.

## FS-09
Downloading runtime assets from the network.

## FS-10
Adding game-specific rendering behavior into Golden Core.

---

# 28. Stop Conditions / DESIGN_QUESTION Triggers

The Local Agent MUST stop and report instead of making a unilateral architecture decision if:

## STOP-01
SDL/window dependency cannot be made isolated from Golden Core.

## STOP-02
The Common Graphics API requires a new GPU semantic not represented by existing Command ISA / Golden.

## STOP-03
Immediate and Tile differ for an application frame.

## STOP-04
A required visual effect exposes a Golden bug.

## STOP-05
A resource-handle design requires changing the frozen 32-bit physical/wire semantics.

## STOP-06
A desired gameplay feature would require game-specific RTL/Golden behavior.

## STOP-07
The only way to achieve acceptable interactive responsiveness appears to require changing bit-exact Golden arithmetic.

## STOP-08
An external dependency/license issue makes chosen assets unsuitable.

The report must include:

```text
DESIGN_QUESTION
Observed problem
Minimal reproduction
Options
Recommended option
Architecture impact
```

---

# 29. Mandatory Evidence Matrix

For every Acceptance ID the final manifest/report must provide:

| Field | Requirement |
|---|---|
| ID | Exact authoritative ID |
| Requirement | Short requirement text |
| Status | PASS only for completed mandatory items |
| Implementation Evidence | Real repo file/function path |
| Verification Evidence | Real test/script/capture path |
| Test Name | Real CTest or exact command |
| Result | Exact observed PASS/result |
| Notes | Optional |

Generic evidence such as:

```text
application implementation
ctest stage0045
covered by game test
```

does not count.

---

# 30. Final REPORT_0045 Template

The final report must follow this structure.

```text
# REPORT_0045 — PC Golden Interactive Application

## 1. Result

## 2. START_COMMIT
<exact 40-hex>

## 3. END_COMMIT
<exact 40-hex>

## 4. Environment
OS
compiler
CMake
window library/version
build type

## 5. Implemented Architecture

## 6. Implemented Game Content

## 7. Graphics API / Backend Separation Evidence

## 8. Immediate-vs-Tile Application Equality

## 9. Automated Tests
exact commands
exact counts
exact results

## 10. Interactive Demo Controls

## 11. Stress Modes
exact thresholds/results

## 12. Visual Captures
paths
resolution
scene/seed/frame

## 13. Known Limitations

## 14. Technical Debt / Follow-up

## 15. 56-row Acceptance Evidence Matrix

## 16. Git Status
must explain any uncommitted files
```

---

# 31. Self-Audit Rule

Before writing REPORT_0045, the Agent MUST re-read this entire task.

For every MUST/REQUIRED statement:

```text
identify concrete implementation path
identify exact test
identify exact observed result
```

If any mandatory item lacks all three:

> **Stage result MUST NOT be PASS.**

The Agent must continue implementation instead of writing a misleading completion report.

---

# 32. Expected Stage Outcome

A successful Stage 004.5 should leave the project with:

```text
a real PC executable
a reusable Graphics API
a backend-neutral application architecture
a Golden Immediate backend
a Golden Tile backend
a playable Survivor/Bullet-Hell prototype
visual effects using actual GPU features
technical HUD
Architecture X-Ray
stress scenes
headless deterministic verification
competition-style 1280×720 captures
```

At that point the project owner should be able to directly inspect:

> **the approximate visual character of the final FPGA demonstration.**

---

# 33. Stage Boundary After Completion

After Stage 004.5 passes, development can split into two parallel tracks:

```text
Track A — Application refinement
    art
    gameplay polish
    additional applications
    GUI/HMI
    demo choreography

Track B — Stage 005 RTL
    command infrastructure
    memory service
    pixel pipeline
    Tile renderer
    Golden co-verification
```

The application developed in Stage 004.5 then becomes a system-level workload and future FPGA demo target.

---

# 34. Final Instruction to Local Agent

Implement the Stage in Blocks.

Do not report after each small subtask.

The workflow is:

```text
Block implementation
→ Block tests
→ Block exit criteria
→ next Block
...
→ full Stage verification
→ acceptance checker
→ REPORT_0045
→ one formal review
```

Do not start unrelated applications or RTL during this task.

The priority is:

> **make the existing Golden GPU visible, interactive, reusable, and demonstrably application-capable without compromising its functional authority.**
