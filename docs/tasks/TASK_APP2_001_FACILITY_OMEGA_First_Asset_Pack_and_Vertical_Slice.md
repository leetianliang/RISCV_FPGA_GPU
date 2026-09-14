# TASK_APP2_001 — FACILITY-Ω First Asset Pack Integration & Survivor-like Vertical Slice

> Project: RISC-V + FPGA 2D GPU  
> Application: Application 2 — FACILITY-Ω  
> Task type: Asset pipeline + large-world/camera + first playable Survivor-like vertical slice  
> Status: READY FOR LOCAL AGENT  
> Architecture owner: Project Owner / ChatGPT  
> Implementation owner: Local Agent  
> Design authority: `Application_2_Survivor_like_Game_Concept_and_Art_Direction_V0.1.md`  
> Asset source authority: `FACILITY_OMEGA_First_Asset_Pack_V0.1.zip`

---

# 0. Task Intent

Stage 004.5 already established a reusable PC Golden application framework and Common Graphics API.

This task does **not** modify GPU semantics and does **not** start RTL.

Its purpose is to begin Application 2 as a separate real game while preserving Application 1.

The desired menu-level product direction is:

```text
RISC-V + FPGA 2D GPU
│
├── Game 1 — NEON SURVIVOR
│            technical/stress demo
│
├── Game 2 — FACILITY-Ω
│            full Survivor-like game
│
├── GPU Playground
├── Architecture X-Ray
└── Benchmark
```

This task delivers the first FACILITY-Ω playable vertical slice.

---

# 1. Critical Asset Rule

The PNGs in `FACILITY_OMEGA_First_Asset_Pack_V0.1.zip` are:

> **SOURCE ART SHEETS / ART-DIRECTION MATERIAL**

They are **not** production-ready runtime atlases.

The Agent is expected to process them.

The Agent MUST NOT simply load an entire source sheet and use the embedded labels/layout as gameplay texture content.

Expected asset workflow:

```text
generated source sheet
      ↓
non-destructive extraction
      ↓
crop / alpha cleanup / normalization
      ↓
runtime-sized sprite
      ↓
atlas + metadata
      ↓
offline format conversion
      ↓
Common Graphics API resource
      ↓
Golden backend
```

Source sheets are immutable evidence/reference.

---

# 2. Source Package Contents

Expected files:

```text
source_sheets/
├── player_source.png
├── enemies_source.png
├── weapons_pickups_source.png
├── fx_source.png
├── environment_source.png
└── ui_source.png

reference/
└── concept_art_v0.1.png

docs/
└── Application_2_Survivor_like_Game_Concept_and_Art_Direction_V0.1.md

README.md
MANIFEST.json
```

The source package includes SHA256 values.

The Agent MUST verify source hashes before processing.

---

# 3. Application Boundary

FACILITY-Ω MUST be a separate application.

Recommended repo location:

```text
software/applications/facility_omega/
```

Do not replace:

```text
software/applications/neon_survivor/
```

Both applications must continue to build.

FACILITY-Ω rendering must use:

```text
software/graphics Common Graphics API
```

FACILITY-Ω game code MUST NOT directly depend on:

```text
GoldenGPU
TileHeader
WorkRef
TILE_FRAME
MemoryImage
AXI
DDR
FPGA registers
```

---

# 4. World / Camera Architecture Freeze

FACILITY-Ω V0.1 uses a world larger than one screen.

Freeze:

```text
World size:
4096 × 4096 pixels

Map Tile:
32 × 32 pixels

Map grid:
128 × 128 MapTiles

Initial major zone:
Power Test Area

Camera:
player-follow camera
clamped to world bounds

Rendering:
visible world only
```

Important terminology:

```text
MapTile    = application/world tile
RenderTile = GPU Tile-Based rendering tile
```

Never use ambiguous variable/class names such as plain `Tile` where the meaning is not obvious.

---

# 5. Coordinate Model

Game entities use WORLD coordinates.

Example:

```text
player_world_x
player_world_y
enemy_world_x
enemy_world_y
```

CPU/application converts world to screen:

```text
screen_x = world_x - camera_x
screen_y = world_y - camera_y
```

The GPU receives screen-space draw requests only.

The GPU does not know the world is 4096×4096.

Use deterministic integer or fixed-point world coordinates.

Do not make simulation correctness depend on host floating-point wall-clock behavior.

---

# 6. Runtime Asset Philosophy

Use authored/generated images where they improve visual quality.

Keep procedural generation where it is technically useful.

Recommended split:

```text
AUTHORED / PROCESSED IMAGE ASSETS
    engineer
    enemies
    map tiles
    props
    weapon icons
    UI panels

PROCEDURAL OR PARAMETRIC
    simple glow
    tiny particles
    debug/X-Ray
    bars
    simple geometric overlays
```

Do not remove the project’s existing procedural asset capability.

---

# 7. Runtime Texture Formats

Preferred first-pass formats:

```text
Opaque floor/background/props:
RGB565

Player / enemy sprites:
ARGB8888 or RGB565 + ColorKey
choose based on cleaned alpha quality

FX:
ARGB8888

Bitmap font / selected UI:
Indexed8 + Palette where practical
```

The Agent must document every runtime texture format in an asset manifest.

---

# 8. Offline Asset Build Rule

Final RISC-V software must not depend on PNG decoding.

Therefore PNG processing belongs to a host-side offline tool.

Preferred:

```text
tools/facility_omega_assetc.py
```

A host-only Pillow dependency is allowed for asset preparation if:

1. it is documented;
2. normal runtime does not require Pillow;
3. processed runtime assets can be checked/generated deterministically;
4. the final C++ application loads project runtime formats rather than decoding PNG.

Alternative deterministic host tooling is allowed.

---

# 9. Source Preservation

The Agent MUST place the supplied source package under a clear immutable directory, e.g.:

```text
assets/facility_omega/source/
```

Source files MUST remain byte-identical to supplied files.

Runtime outputs must go elsewhere:

```text
assets/facility_omega/runtime/
assets/facility_omega/processed/
```

Never rewrite source sheets during normal tests.

---

# 10. Block A — Asset Intake & Audit

## Implementation MUST

A-01. Verify every source-package SHA256.

A-02. Preserve source sheets unchanged.

A-03. Produce an asset audit manifest listing candidate extracted assets.

A-04. Explicitly mark source-sheet labels/text as non-runtime content unless intentionally used in UI.

A-05. Record source image size, alpha presence, and crop source for each extracted sprite.

A-06. Produce a contact-sheet preview of processed runtime candidates.

## Minimum asset audit

Player:
```text
engineer_up_0
engineer_up_1
engineer_down_0
engineer_down_1
engineer_left_0
engineer_left_1
engineer_right_0
engineer_right_1
engineer_idle
engineer_hurt
```

Enemies:
```text
drone_0 / drone_1
crawler_0 / crawler_1
runner_0 / runner_1
tank_0 / tank_1
elite_0 / elite_1
```

Weapons/pickups:
```text
pulse_shot
enemy_bullet
orbit_drone
plasma_orb
energy_field_core
chain_arc_node
meteor_marker
xp_small
xp_large
repair_pickup
```

Environment:
```text
at least 8 floor tiles
at least 6 props
```

UI:
```text
HP frame
XP frame
time panel
kills panel
skill frame
level-up panel/card
main menu panel/button
```

## Exit Criteria

A machine-readable audit exists and a human can inspect one generated contact sheet to see all extracted candidates without source labels being mixed into sprites.

---

# 11. Block B — Asset Extraction / Normalization

## Implementation MUST

B-01. Extract runtime sprites non-destructively from source sheets.

B-02. Remove surrounding label graphics from runtime sprite crops.

B-03. Trim excessive transparent margins.

B-04. Preserve soft alpha for FX.

B-05. Define anchor/pivot for every moving sprite.

B-06. Normalize animation frames so the anchor does not visibly jump between frames.

B-07. Do not accidentally turn transparency into black backgrounds.

B-08. Generate processed PNG preview assets plus runtime raw output.

## Scaling Rule

For body sprites, prefer nearest-neighbor only if it preserves readability.

Because generated source art includes antialiased detail, the Agent MAY use alpha-aware area/Lanczos downsampling for the first reduction step if nearest-neighbor produces unacceptable aliasing.

But after a runtime sprite size is frozen, gameplay scaling should use the project GPU semantics, not host-side arbitrary resampling.

## Target first-pass sizes

Guidance, not absolute law:

```text
Engineer:
32×32 preferred
48×48 allowed if 32×32 loses identity

Drone:
16~24 px

Crawler:
20~32 px

Runner:
20~32 px

Tank:
40~64 px

Elite:
40~64 px

Pulse shot:
6~12 px

XP small:
8~12 px
```

If a target size must differ significantly, document why.

---

# 12. Block C — Deterministic Runtime Asset Pipeline

## Implementation MUST

C-01. Build runtime texture binaries deterministically.

C-02. Generate one machine-readable runtime asset manifest.

C-03. Runtime manifest records at least:

```text
name
source
format
width
height
stride
anchor_x
anchor_y
atlas/source rect
runtime file
sha256
```

C-04. Asset output is reproducible from immutable source sheets.

C-05. Normal verification mode never overwrites golden/reference outputs.

C-06. Provide explicit opt-in update mode for regenerated checked assets if checked outputs are used.

## Suggested runtime structure

```text
assets/facility_omega/runtime/
├── player/
├── enemies/
├── weapons/
├── fx/
├── map/
├── ui/
└── facility_omega_assets.json
```

---

# 13. Block D — Application Skeleton & Menu Integration

## Implementation MUST

D-01. Add separate `facility_omega` application.

D-02. Existing NEON SURVIVOR remains intact.

D-03. Top-level demo launcher exposes both games.

Minimum menu:

```text
[1] NEON SURVIVOR
[2] FACILITY-Ω
[3] GPU PLAYGROUND
[4] ARCHITECTURE / BENCHMARK
```

Only games 1 and 2 must launch real applications in this task.

D-04. Returning to the launcher must be deterministic and not terminate the whole process unless the actual Quit action is selected.

---

# 14. Block E — 4096×4096 Map / Camera

## Implementation MUST

E-01. 128×128 MapTile logical map.

E-02. 32×32 MapTile size.

E-03. World bounds = 4096×4096.

E-04. Player-follow camera.

E-05. Camera clamp at world edges.

E-06. Only visible/near-visible map region is submitted for drawing.

E-07. At least 8 distinct processed floor tiles are used.

E-08. At least 6 props are placed in the world.

E-09. World can scroll continuously for more than one screen in every valid direction.

## Recommended culling

For screen size `W×H`:

```text
visible tiles
+
1 tile guard band
```

Do not submit all 16,384 map tiles every frame.

## Tests MUST

- camera center behavior;
- left/top clamp;
- right/bottom clamp;
- world→screen transform;
- visible MapTile range;
- moving > one screen changes visible world tiles.

---

# 15. Block F — Player

## Implementation MUST

F-01. Engineer uses processed image assets, not the old NEON procedural player.

F-02. WASD movement.

F-03. 4 directions.

F-04. 2-frame movement animation where processed frames are available.

F-05. Idle state.

F-06. Hurt tint/flash may use Color Mod rather than a separate final texture.

F-07. Player collision with world boundary.

F-08. Camera follows player.

## Test

A deterministic scripted route must move the player through more than one viewport width/height while preserving valid camera and world coordinates.

---

# 16. Block G — First Enemy Set

First playable vertical slice requires:

```text
Flying Drone
Crawler
Tank Unit
```

Runner and Elite assets should be processed now but may be activated in the next gameplay task.

## Implementation MUST

G-01. Spawn enemies in world space.

G-02. Prefer spawning outside the current camera viewport.

G-03. Enemies chase/move toward player using simple deterministic logic.

G-04. Drone / Crawler / Tank differ visibly and statistically.

G-05. Enemies outside the render/cull margin are not submitted to GPU.

G-06. Enemy simulation may exist off-screen when appropriate.

## Suggested differentiation

```text
Drone:
small / medium speed / low HP

Crawler:
small-medium / melee / swarm

Tank:
large / slow / high HP
```

---

# 17. Block H — Pulse Shot Combat

## Implementation MUST

H-01. Auto-target nearest legal enemy.

H-02. Fire Pulse Shot.

H-03. Projectile exists in world space.

H-04. Projectile uses processed Pulse Shot art.

H-05. Projectile/enemy collision.

H-06. Damage / kill.

H-07. Enemy hit flash uses GPU Color Mod or equivalent existing semantic.

H-08. Kill counter.

H-09. Basic explosion/spark uses FX assets/procedural GPU effects.

This block must create the first real combat loop.

---

# 18. Block I — XP / Level-Up Vertical Slice

## Implementation MUST

I-01. Killed enemies can drop XP crystals.

I-02. XP crystal uses processed image asset.

I-03. Player collects nearby XP.

I-04. XP bar.

I-05. Level threshold.

I-06. On level-up, pause simulation and show a 3-choice Level-Up panel.

For this first vertical slice the 3 choices can be simple:

```text
Pulse Damage +1
Fire Rate +1
Projectile Count +1
```

I-07. Choice affects gameplay deterministically.

I-08. Resume after selection.

This block is required because it is the key mechanic that distinguishes FACILITY-Ω from a simple stress shooter.

---

# 19. Block J — HUD / Presentation

## Required HUD

```text
HP
XP
Level
Time
Kills
current weapon icon
```

The new FACILITY-Ω UI art should be used where practical.

Text may reuse the existing project bitmap-font system for this first task.

Do not add OS/system-font dependencies to final runtime.

## Technical HUD

Optional toggle can display:

```text
Sprites
Commands
WorkRefs
Active Tiles
Max Overdraw
Renderer Mode
```

Reuse existing telemetry abstraction.

---

# 20. Block K — Immediate / Tile Compatibility

FACILITY-Ω must remain backend-neutral.

## Implementation MUST

K-01. Immediate mode.

K-02. Tile32 mode.

K-03. Same application code.

K-04. At least 60 deterministic vertical-slice frames:

```text
Immediate final framebuffer
==
Tile32 final framebuffer
byte-for-byte
```

K-05. Equality corpus must include:

```text
map rendering
player
enemy
pulse shot
hit
kill
XP crystal
HUD
at least one level-up UI frame
```

---

# 21. Block L — Headless / Deterministic Verification

## Implementation MUST

CLI or equivalent:

```text
--app facility_omega
--headless
--frames N
--seed N
--backend immediate|tile
--capture path
```

## Tests MUST

L-01. Fixed seed + scripted input reproduces simulation hash.

L-02. Fixed seed + frame reproduces framebuffer hash.

L-03. 600-frame headless gameplay smoke.

L-04. 60-frame Immediate==Tile exact.

L-05. Asset manifest hashes stable.

L-06. Source assets unchanged.

L-07. Existing NEON SURVIVOR tests continue passing.

L-08. Existing Golden tests continue passing.

---

# 22. Block M — Visual Evidence

Generate checked or review artifacts:

```text
results/facility_omega/v0_1/
```

Required:

### V1 — Asset Contact Sheet
Processed player/enemy/weapon/map/UI samples.

### V2 — Large Map Screenshot
Engineer visibly away from initial spawn, proving scrolling world.

### V3 — Combat Screenshot
Player + Drone + Crawler + Tank + Pulse Shot + FX.

### V4 — Level-Up Screenshot
3-choice upgrade screen.

### V5 — Tile/Technical Screenshot
FACILITY-Ω with technical HUD or X-Ray enabled.

Recommended resolution:

```text
1280×720 showcase
```

These are visual review evidence, not replacements for deterministic tests.

---

# 23. Non-Goals

Do NOT implement in this task:

```text
Overseer Core Boss
full 6-weapon system
Orbit Drone gameplay
Plasma Nova gameplay
Energy Field gameplay
Chain Arc gameplay
Meteor Strike gameplay
final story
save system
audio
networking
3D
new GPU opcodes
new Golden pixel semantics
RTL
AXI / DDR timing
```

Assets for some of these may already exist in the source pack; processing them is allowed, gameplay implementation is deferred.

---

# 24. Forbidden Shortcuts

## FS-01
Do not use the full source sheet as one runtime sprite.

## FS-02
Do not include source labels such as “UP 1”, “FLYING DRONE”, etc. in gameplay textures.

## FS-03
Do not destructively edit authoritative source sheets.

## FS-04
Do not render gameplay with Win32/SDL/OpenGL primitives.

## FS-05
Do not put FACILITY-Ω game logic inside Golden Core.

## FS-06
Do not hardcode a single screen arena; world must exceed viewport.

## FS-07
Do not submit the entire 4096×4096 tile map each frame.

## FS-08
Do not silently replace generated art with unrelated web assets.

## FS-09
Do not add unlicensed external assets.

## FS-10
Do not alter Stage-004/004.5 Golden expected pixels merely to fit Application 2.

---

# 25. Stop Conditions

Stop and report `DESIGN_QUESTION` if:

```text
source art cannot be separated cleanly from labels/background;
player art becomes unreadable at practical runtime size;
asset alpha cleanup changes intended visual identity severely;
existing Graphics API cannot express a required first-slice draw;
Immediate and Tile differ;
large-map rendering requires a new GPU opcode to be viable;
runtime would require PNG decoder on RISC-V;
asset licensing/source provenance becomes unclear.
```

Do not invent a new GPU semantic to solve an Application 2 problem.

---

# 26. Acceptance IDs

Mandatory:

```text
AST-01 .. AST-08    8   Asset intake/pipeline
MAP-01 .. MAP-08    8   World/camera/map
PLY-01 .. PLY-05    5   Player
ENM-01 .. ENM-05    5   Enemies
CMB-01 .. CMB-06    6   Combat
XP-01  .. XP-06     6   XP/level-up
UI-01  .. UI-04     4   HUD/menu
EQ-01  .. EQ-04     4   Backend exactness
SYS-01 .. SYS-05    5   Determinism/regression
VIS-01 .. VIS-05    5   Visual evidence
--------------------------------
TOTAL                56
```

Every ID is mandatory.

---

# 27. Completion Contract

For every mandatory ID:

```text
Requirement
→ Implementation Evidence
→ Verification Evidence
→ Exact Result
→ PASS
```

Rules:

> No evidence = NOT DONE.

> Similar test without exact mapping = NOT DONE.

> Screenshot alone = not correctness proof.

> Green equality between two backends does not prove a shared application bug is correct; feature-directed tests are required where applicable.

---

# 28. Report

Required:

```text
docs/reports/REPORT_APP2_001_FACILITY_OMEGA_VERTICAL_SLICE.md
```

Must include:

```text
START_COMMIT
END_COMMIT
source asset pack SHA256
asset-processing toolchain
processed asset list
runtime texture formats
world/camera architecture
implemented gameplay
exact tests
exact counts/results
visual evidence paths
known art issues
known gameplay issues
56-row acceptance matrix
```

---

# 29. Expected Result

At completion the owner should be able to launch the demo and do this:

```text
Main Menu
→ FACILITY-Ω
→ control Engineer
→ walk through a scrolling 4096×4096 facility
→ camera follows
→ fight Drone/Crawler/Tank
→ automatic Pulse Shot
→ kill enemies
→ pick up XP
→ level up
→ choose one of 3 upgrades
→ continue playing
→ switch Immediate/Tile32
```

The screen must visibly use the processed FACILITY-Ω art rather than the old NEON procedural player/enemy set.

---

# 30. Final Instruction to Local Agent

This is a first playable vertical slice.

Do not attempt to finish the full game in one task.

Priority order:

```text
asset integrity
→ asset pipeline
→ map/camera
→ player
→ 3 enemies
→ Pulse Shot combat
→ XP/level-up
→ HUD
→ deterministic/backend verification
→ visual captures
```

The purpose of this task is to establish a stable foundation that later tasks can expand with:

```text
Orbit Drone
Plasma Nova
Energy Field
Elite
Boss
additional facility zones
visual polish
```
