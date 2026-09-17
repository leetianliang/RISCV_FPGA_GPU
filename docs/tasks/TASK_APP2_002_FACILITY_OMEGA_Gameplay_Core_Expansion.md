# TASK_APP2_002 — FACILITY-Ω Gameplay Core Expansion & Multi-Weapon Survivor Loop

> Project: RISC-V + FPGA 2D GPU  
> Application: Application 2 — FACILITY-Ω  
> Task type: Gameplay core expansion / deterministic director / multi-weapon system / scalable entity broad-phase  
> Status: **READY FOR LOCAL AGENT**  
> Architecture owner: Project Owner / ChatGPT  
> Implementation owner: Local Agent  
> Baseline commit: `57c5eb35d0c64aa7c3940f3d6f11c11f018f8376`  
> Baseline visual gate: **MAP R3V2 + M13 VISUAL PASS**  
> Design authority: `Application_2_Survivor_like_Game_Concept_and_Art_Direction_V0.1.md`  
> Previous implementation task: `TASK_APP2_001_FACILITY_OMEGA_First_Asset_Pack_and_Vertical_Slice.md`

---

# 0. Baseline State

Application 2 already has a valid technical and visual foundation.

The following must be treated as **existing baseline**, not reimplemented from scratch:

```text
4096×4096 world
1024×1024 authored Hero Area
player-follow camera
environment collision
open Survivor-like combat space
processed FACILITY-Ω asset pipeline
Engineer player
Drone / Crawler / Tank
automatic Pulse Shot
damage / kill / XP drop / XP pickup
Level-Up pause
3-choice upgrade
HUD
Immediate backend
Tile32 backend
deterministic/headless path
MAP R3V2 visual baseline
```

MAP R3V2 passed direct visual review.

Therefore:

> **This task MUST NOT reopen the map architecture or start another map-only visual redesign.**

Minor environment polish is allowed only when required to support new gameplay feedback.

---

# 1. Task Intent

TASK_APP2_001 proved that FACILITY-Ω can function as a playable vertical slice.

TASK_APP2_002 turns that slice into the first genuine **Survivor-like gameplay core**.

The goal is to establish:

```text
time-based enemy escalation
+
five distinct enemy roles
+
four simultaneous weapon families
+
weapon unlock / upgrade choices
+
deterministic high-density combat
+
scalable CPU-side entity lookup
+
GPU-visible workload progression
```

The desired gameplay loop after this task is:

```text
Start
↓
Move Engineer
↓
Pulse Shot auto-fire
↓
Enemy waves escalate with time
↓
Kill → XP → Level Up
↓
Choose new weapon or upgrade
↓
Orbit Drone / Plasma Nova / Energy Field join build
↓
Runner / Elite appear
↓
Mid/late game becomes visibly denser
↓
Multiple GPU features operate together
↓
Survive until late-game phase
```

This task does **not** add the final Boss.

---

# 2. Product Principle

The application remains a real game running on the same general-purpose 2D GPU API.

Never implement game-specific GPU semantics such as:

```text
DRAW_ENEMY
DRAW_NOVA
DRAW_ORBIT
DRAW_XP
DRAW_FACILITY_WALL
```

FACILITY-Ω must continue to compile down to generic:

```text
fill_rect
draw_sprite
clip
palette
color mod
alpha
additive
nearest / bilinear scaling
```

Game is software.

GPU is hardware.

---

# 3. Frozen Map / Environment Rules

The R3V2 map is now baseline.

Freeze:

```text
Hero Area geometry
Environment collision footprints
Open-space ratio
Camera model
World coordinate model
Player collision semantics
Simple enemy obstacle handling
```

Do not change these merely to simplify new gameplay.

Allowed:

```text
small non-colliding FX
temporary combat decals
hit marks
pickup effects
weapon FX
```

Not allowed:

```text
new maze walls
major Hero Area reshaping
new map GPU primitives
destructive map redesign
```

---

# 4. Core Architecture for APP2_002

Recommended gameplay stack:

```text
Facility App
│
├── Simulation Clock
├── Game Director
├── Spawn Service
├── Player / Build State
├── Enemy System
├── Weapon Systems
│   ├── Pulse Shot
│   ├── Orbit Drone
│   ├── Plasma Nova
│   └── Energy Field
├── Projectile / Area Effect System
├── XP / Pickup System
├── Gameplay Spatial Grid
├── HUD / Level-Up UI
└── Graphics API Submission
```

The Graphics API remains the only rendering boundary.

---

# 5. Determinism Freeze

Simulation must remain deterministic.

Use:

```text
fixed simulation timestep
integer / fixed-point positions
seeded PRNG
stable update order
stable spawn order
stable level-up option generation
```

Do not make gameplay correctness depend on:

```text
host wall clock
host FPS
unordered iteration order
platform-specific floating-point behavior
```

If a vector is iterated to resolve collisions or targets, its ordering must be deterministic.

---

# 6. Game Director

Add a deterministic time-based `GameDirector`.

Normal-game progression:

```text
Phase 1: 0–120 s
Phase 2: 120–300 s
Phase 3: 300–480 s
Late:    >=480 s
```

The phase thresholds are game-time seconds, not host time.

## Phase 1 — Establish

Main enemies:

```text
Drone
Crawler
```

Target feeling:

```text
readable
low-to-medium density
Pulse Shot dominates
```

## Phase 2 — Pressure

Add:

```text
Runner
Tank
```

Target feeling:

```text
more movement pressure
larger bodies
first multi-weapon build
```

## Phase 3 — Swarm

Add:

```text
Elite
higher spawn density
larger mixed groups
```

Target feeling:

```text
Alpha
Additive
Scaling
higher overdraw
```

## Late Phase

Continue increasing density within configured caps.

No Boss in this task.

---

# 7. Director Configuration

Director tuning values must live in explicit data/config structures.

Do not scatter magic numbers across update code.

Recommended structure:

```cpp
struct DirectorPhase {
    u32 start_tick;
    u32 spawn_interval;
    u32 max_active_enemies;
    u32 group_min;
    u32 group_max;
    EnemyWeight weights[...];
    u32 elite_interval;
};
```

Equivalent design is allowed.

Tests must be able to inspect phase configuration.

---

# 8. Test-Time Phase Access

Tests must not wait eight real minutes to validate Phase 3/Late behavior.

Provide one deterministic test path such as:

```text
fixture helper to set simulation tick
```

or headless-only CLI:

```text
--facility-start-seconds N
```

This must not change normal interactive semantics.

If a CLI hook is added, clearly label it:

```text
verification / demo control
```

not normal gameplay progression.

---

# 9. Spawn Service

Enemy spawn rules:

```text
world-space
outside visible camera rectangle
inside valid world / Hero Area policy
not inside collision geometry
deterministic candidate order
```

Recommended spawn band:

```text
camera bounds
+
32–160 px outer annulus
```

Do not spawn directly on-screen unless an explicit authored event requires it.

Spawn selection must avoid:

```text
walls
equipment collision
player overlap
invalid world edge
```

---

# 10. Enemy Roster

Existing:

```text
Drone
Crawler
Tank
```

Activate and integrate:

```text
Runner
Elite
```

All five types must differ in both visual identity and gameplay role.

---

# 11. Drone

Role:

```text
common pressure unit
```

Suggested behavior:

```text
medium speed
low HP
direct chase
```

No major redesign required.

---

# 12. Crawler

Role:

```text
swarm melee
```

Suggested behavior:

```text
lower profile
slightly slower or more numerous
low-medium HP
```

No major redesign required.

---

# 13. Runner

Mandatory in APP2_002.

Role:

```text
fast flanker
```

Requirements:

```text
visibly faster than Drone/Crawler
lower HP than Tank
spawn weight increases after Phase 2
distinct runtime sprite
respects environment collision
```

A mild lateral/tangent bias is allowed.

Do not implement expensive pathfinding.

---

# 14. Tank

Role:

```text
slow heavy body
```

Requirements:

```text
large collision radius
high HP
slow movement
larger visual sprite
higher XP reward than common enemies
```

Keep existing basic behavior unless needed for correctness.

---

# 15. Elite

Mandatory in APP2_002.

Role:

```text
rare high-pressure enemy
```

Visual identity:

```text
purple / magenta energy identity
Additive aura / glow
larger or brighter core
```

Gameplay:

```text
significantly more HP than common enemies
higher contact damage
larger XP reward
phase-gated spawn
```

Elite does not need a unique complex AI.

The point is:

```text
rare threat
+
strong visual signal
+
GPU alpha/additive workload
```

---

# 16. Enemy Population Capacity

Remove prototype assumptions that cap the final system at 256 active enemies.

APP2_002 must support configured capacity of at least:

```text
Enemies:     1024
Projectiles: 2048
Pickups:     2048
```

This is a **logical capacity requirement**, not an FPGA performance claim.

A normal phase does not need to fill all capacities.

No crash, invalid index or nondeterministic allocation behavior is allowed near the configured cap.

---

# 17. Gameplay Spatial Grid — Mandatory

Before increasing entity density, introduce a deterministic CPU-side spatial broad-phase.

Purpose:

```text
avoid O(projectiles × enemies)
avoid scanning every pickup
prepare RISC-V implementation
```

Recommended:

```text
cell size = 64 / 128 / 256 pixels
compile-time or explicit config
```

Suggested structure:

```text
GameplaySpatialGrid
├── enemy refs
├── pickup refs
└── optional projectile refs
```

The grid is application-side only.

It is unrelated to GPU RenderTile.

Use explicit terminology:

```text
GameplayGridCell
RenderTile
MapTile
MacroChunk
```

Never call all of them simply `Tile`.

---

# 18. Broad-Phase Requirements

At minimum use the grid for:

```text
projectile → enemy collision candidates
nearest-enemy search
XP magnet / collection candidate lookup
```

Environment AABB lookup may remain separate in this task, but MacroChunk collision indexing is recommended if convenient.

The broad-phase must be deterministic.

---

# 19. Broad-Phase Verification

Add a brute-force oracle for tests.

For deterministic random fixtures:

```text
SpatialGrid result
==
BruteForce result
```

Verify at least:

```text
nearest enemy
projectile collision candidate set
pickup neighborhood query
```

Do not validate the spatial grid only with performance counters.

---

# 20. Weapon System Architecture

Weapons must use a common update contract.

Recommended conceptual model:

```text
WeaponState
├── weapon_id
├── level
├── cooldown
└── weapon-specific runtime state
```

Player build can hold:

```text
up to 4 active weapon families
```

for APP2_002.

Mandatory families:

```text
Pulse Shot
Orbit Drone
Plasma Nova
Energy Field
```

---

# 21. Pulse Shot

Existing weapon remains baseline.

APP2_002 requirements:

```text
nearest legal target
fire cooldown
damage
projectile count
projectile lifetime/range
processed pulse art
hit FX
```

Upgrade dimensions must include at least three of:

```text
damage
fire rate
projectile count
projectile speed
```

Keep behavior deterministic.

---

# 22. Orbit Drone

Mandatory new weapon.

Concept:

```text
small drones orbit Engineer
damage enemies on contact / proximity
```

Implementation rules:

```text
CPU computes orbit positions
GPU draws generic sprites
no arbitrary sprite rotation required
```

Use:

```text
orbit_drone asset
optional trail / glow
```

Suggested upgrade dimensions:

```text
drone count
orbit radius
damage
orbit speed
```

Orbit position may use:

```text
integer LUT
fixed-point phase
```

Avoid host `sin()`/`cos()` dependence in production simulation.

---

# 23. Plasma Nova

Mandatory new weapon.

Concept:

```text
periodic expanding energy ring
```

GPU mapping:

```text
one ring texture
+
Scaling
+
Bilinear
+
Global Alpha
+
Additive
```

This weapon is strategically important because it directly demonstrates multiple GPU features together.

Simulation:

```text
trigger cooldown
radius grows
alpha decreases
damage occurs according to explicit rule
effect expires
```

Do not implement the Nova as a 20-frame pre-baked animation.

Suggested upgrades:

```text
cooldown
max radius
damage
ring count / repeat pulse
```

---

# 24. Energy Field

Mandatory new weapon.

Concept:

```text
persistent / periodic zone around player
```

GPU mapping:

```text
Alpha
Scaling
Overdraw
optional Additive edge
```

Gameplay:

```text
enemies within radius receive periodic damage
```

Use integer squared-distance tests.

Do not perform square root per enemy.

Suggested upgrades:

```text
radius
damage
tick rate
alpha/visual level
```

---

# 25. Multi-Weapon Coexistence

By the end of this task the player must be able to simultaneously run:

```text
Pulse Shot
+
Orbit Drone
+
Plasma Nova
+
Energy Field
```

without special-case rendering conflicts.

The game should visibly evolve from:

```text
single BLIT weapon
```

to:

```text
many sprites
+ scaling
+ bilinear
+ alpha
+ additive
+ overdraw
```

This is a key project narrative.

---

# 26. Upgrade / Build System

Current 3-choice Level-Up UI stays.

Expand its semantics from only stat boosts into a build system.

Choices may be:

```text
unlock new weapon
upgrade existing weapon
player stat upgrade
```

For APP2_002, prioritize weapon unlock/upgrade choices.

---

# 27. Upgrade Choice Rules

Each Level-Up presents exactly:

```text
3 valid choices
```

Rules:

```text
no duplicate choice in one panel
do not offer max-level upgrade
do not offer unlock for already-owned weapon
stable ordering under fixed seed
```

If the candidate pool has fewer than three normal choices, fill using valid fallback player-stat upgrades.

---

# 28. Weapon Levels

Each weapon must support at least:

```text
Level 1–5
```

Exact numeric balance is not frozen, but every level must produce an observable gameplay change.

Do not create levels that only change a UI number without changing simulation.

---

# 29. Build State

The HUD or pause/level-up UI must expose current weapon build.

Minimum:

```text
weapon icon
weapon level
```

The main gameplay HUD must remain compact.

Do not cover a large portion of the 640×360 screen.

---

# 30. Repair Pickup

Use the existing processed repair pickup.

Minimum behavior:

```text
small chance from kill / Elite
restores HP
does not exceed max HP
```

Exact drop rate is tuning data.

This is a small feature and should not become a large inventory system.

---

# 31. XP Economy

Keep deterministic XP thresholds.

APP2_002 must ensure:

```text
Phase 1 can produce several upgrades
Phase 2 allows a multi-weapon build
Phase 3 can reach at least 3 active weapons in normal tuning
```

Do not hardcode the test to grant weapons without exercising the upgrade system, except in explicit showcase fixtures.

---

# 32. Combat Feedback

Required lightweight feedback:

```text
enemy hit flash
kill spark/explosion
XP pickup feedback
Elite glow
Nova ring
Energy Field visual
Orbit trail/glow
```

Use existing assets and existing GPU features.

No screen-space shader effects.

---

# 33. Player Readability Rule

Even in late-game test scenes:

```text
Engineer must remain visually discoverable in < 1 second
```

Practical rules:

```text
player cyan/orange identity stays unique
avoid permanent bright FX exactly over player body
Energy Field center should not hide player
Orbit Drones should not form an opaque ring
```

This is a visual acceptance item.

---

# 34. Enemy Readability Rule

Common enemies may become dense, but:

```text
Elite
Tank
Runner
```

must remain recognizable by silhouette/color/motion.

Do not use color alone as the only differentiator.

---

# 35. Difficulty / Density Targets

These are gameplay/workload targets, not hardware-performance claims.

APP2_002 accelerated deterministic scenes should demonstrate approximately:

```text
Early:
  20–60 active enemies

Mid:
  80–180 active enemies

Swarm test:
  >=300 active enemies

Projectile / FX combined:
  >=500 visible or live draw/effect entities in dedicated density test
```

The system must remain logically stable at these counts.

Do not label PC Golden host FPS as FPGA performance.

---

# 36. Late-Phase Capacity Stress

Add a deterministic density fixture that can instantiate at least:

```text
300 enemies
700 projectile/effect/pickup entities combined
```

for:

```text
60 rendered frames
```

Requirements:

```text
no crash
no invalid memory
deterministic hash
Immediate == Tile32 final framebuffer
```

This is a correctness/workload test, not a 60 FPS performance requirement.

---

# 37. GPU Workload Telemetry

Expose or reuse telemetry for APP2_002 scenes:

```text
draw calls / sprites
alpha draws
additive draws
scaled draws
bilinear draws
active enemies
live projectiles
live pickups
max overdraw
Tile active count
WorkRefs
```

The telemetry is for technical demonstration.

Do not claim it as FPGA silicon performance until RTL counters exist.

---

# 38. Normal Game vs Showcase

Maintain clear separation:

## Normal Game

Generated by actual simulation progression.

## Showcase Fixture

Authored deterministic snapshot for visual/technical demonstration.

Every capture/report must state which it is.

Never present authored showcase entity counts as natural gameplay progression evidence.

---

# 39. Backend Neutrality

All new weapon/game code must remain backend-neutral.

Same application command stream must run on:

```text
Immediate
Tile32
```

No weapon may call Golden Core directly.

No application code may inspect Tile internals.

---

# 40. Required Exactness Corpus

Add deterministic equality sequences containing:

```text
Runner
Elite
Orbit Drone
Plasma Nova
Energy Field
multi-weapon build
Level-Up
Repair pickup
dense enemy scene
```

Minimum:

```text
120 consecutive rendered frames
Immediate == Tile32 byte-for-byte
```

Additionally test one high-density 60-frame fixture.

---

# 41. Simulation Hash

Define a stable simulation hash for key gameplay state.

Include enough state to detect real divergence, such as:

```text
tick
player position / HP / XP / level
weapon IDs / levels / cooldowns
enemy count + ordered core state
projectile state
pickup state
director phase / PRNG state
```

Do not hash raw pointer addresses or allocator capacity.

---

# 42. Headless Verification

Required CLI path remains:

```text
--app facility
--headless
--frames N
--seed N
--backend immediate|tile
--capture path
```

Optional new verification flags may include:

```text
--facility-start-seconds
--facility-showcase
--facility-density-fixture
```

If added, document them as verification/demo controls.

---

# 43. Required Visual Evidence

Generate:

```text
results/facility_omega/app2_002/
```

Required captures:

### V1 — Early Game

```text
Pulse Shot
Drone/Crawler
low density
clear map
```

### V2 — Mid Build

```text
Pulse Shot
Orbit Drone
Runner/Tank
XP
```

### V3 — FX Build

```text
Plasma Nova
Energy Field
Alpha + Additive + Scaling
```

### V4 — Elite Encounter

```text
Elite visible
Elite aura
mixed enemy group
```

### V5 — Level-Up Build Screen

```text
3 valid weapon/build choices
icons + levels
```

### V6 — Late Density

```text
>=300 enemies
multi-weapon effects
player still readable
```

### V7 — Technical HUD

Show:

```text
active enemies
sprites
commands
scaled / alpha / additive
tiles / WorkRefs
```

---

# 44. Main Competition Capture Candidate

Create one additional:

```text
showcase_app2_002.png
```

Target composition:

```text
Engineer near center
15–40 visible enemies
one Tank
one Elite
Pulse Shot trail
Orbit Drones
Nova in progress
Energy Field visible
XP crystals
map zone detail
HUD
```

This should be aesthetically staged but still use the real rendering path.

Label it:

```text
STAGED SHOWCASE
```

in metadata/report, not necessarily as visible text in the final screenshot.

---

# 45. Performance-Friendly Rules for RISC-V

Avoid introducing CPU-heavy algorithms that scale badly.

Forbidden in per-entity hot loops:

```text
unbounded square root loops
dynamic string lookup
heap allocation per frame
std::sin / std::cos dependence
O(E×B) full projectile-enemy scan
full pickup scan for each player query
```

Prefer:

```text
fixed-point
LUT
squared distance
spatial grid
preallocated/reused vectors
stable compact state
```

---

# 46. Data-Oriented Guidance

Do not require a full ECS rewrite.

But entity hot data should be reasonably compact.

At minimum avoid deeply nested polymorphic objects for thousands of entities.

Simple structures/vectors are preferred.

This task is gameplay expansion, not an engine rewrite.

---

# 47. Non-Goals

Do NOT implement in APP2_002:

```text
Overseer Core Boss
Boss phases
Chain Arc
Meteor Strike
save/load
audio system
networking
multiplayer
procedural infinite world
full 4096×4096 art expansion
A* pathfinding
3D
new GPU opcode
new Golden pixel semantics
RTL
AXI / DDR timing
final competition balancing
```

Chain Arc / Meteor / Boss belong to later tasks.

---

# 48. Forbidden Shortcuts

## FS-01
Do not hardcode “Level 2 means Orbit” without a real 3-choice build system.

## FS-02
Do not draw Nova using host graphics.

## FS-03
Do not fake Energy Field as a HUD overlay.

## FS-04
Do not bypass Graphics API for any new weapon.

## FS-05
Do not disable collision to make dense scenes pass.

## FS-06
Do not use the staged showcase as the only proof of gameplay behavior.

## FS-07
Do not reduce the map visual baseline to improve entity-count tests.

## FS-08
Do not silently change Golden expected pixels to fit new app output.

## FS-09
Do not interpret PC Golden FPS as FPGA FPS.

## FS-10
Do not reopen map architecture unless a verified functional defect requires it.

---

# 49. Stop Conditions

Stop and report `DESIGN_QUESTION` if:

```text
existing Graphics API cannot express a required weapon effect;
Immediate and Tile32 diverge;
new weapon requires a new GPU semantic;
spatial-grid result disagrees with brute-force oracle;
entity density causes nondeterministic simulation;
environment collision becomes incompatible with high-density gameplay;
source assets are insufficient for a required visual;
normal gameplay cannot remain readable with four weapons active.
```

Do not invent architecture changes silently.

---

# 50. Acceptance IDs

Total mandatory IDs: **64**

## PRE — Baseline / Architecture (4)

```text
PRE-01  Baseline HEAD recorded
PRE-02  MAP R3V2 baseline preserved
PRE-03  Graphics API boundary preserved
PRE-04  Deterministic fixed-step preserved
```

## DIR — Director / Spawn (8)

```text
DIR-01  Four time phases implemented
DIR-02  Director config centralized
DIR-03  Fixed seed gives fixed phase/spawn sequence
DIR-04  Phase test hook exists
DIR-05  Spawn outside camera
DIR-06  Spawn avoids environment collision
DIR-07  Phase-specific enemy weights
DIR-08  Density caps enforced
```

## ENM — Enemy System (8)

```text
ENM-01  Drone integrated
ENM-02  Crawler integrated
ENM-03  Tank integrated
ENM-04  Runner implemented and distinct
ENM-05  Elite implemented and distinct
ENM-06  Elite aura uses real GPU blend path
ENM-07  All enemy types respect environment collision
ENM-08  Capacity >=1024 enemies
```

## WPN — Weapons (16)

```text
WPN-01  Pulse auto-target
WPN-02  Pulse upgrades observable
WPN-03  Pulse multi-projectile works
WPN-04  Pulse uses spatial-grid candidate lookup

WPN-05  Orbit Drone implemented
WPN-06  Orbit uses fixed-point/LUT phase
WPN-07  Orbit damage works
WPN-08  Orbit upgrades observable

WPN-09  Plasma Nova implemented
WPN-10  Nova uses Scale+Bilinear
WPN-11  Nova uses Alpha/Additive
WPN-12  Nova damage/radius lifecycle deterministic

WPN-13  Energy Field implemented
WPN-14  Field uses Alpha/Overdraw path
WPN-15  Field damage uses squared-distance rule
WPN-16  All four weapons coexist
```

## UPG — Build / Level-Up (8)

```text
UPG-01  3 valid choices
UPG-02  No duplicate choice
UPG-03  No max-level choice
UPG-04  Unlock vs upgrade semantics correct
UPG-05  Four weapon families unlockable
UPG-06  Each weapon supports Level 1–5
UPG-07  Fixed seed gives fixed options
UPG-08  HUD shows weapon build/levels
```

## SPC — Spatial / Scaling (6)

```text
SPC-01  GameplaySpatialGrid implemented
SPC-02  Nearest-enemy query uses grid
SPC-03  Projectile broad-phase uses grid
SPC-04  Pickup neighborhood uses grid
SPC-05  Grid matches brute-force oracle
SPC-06  Capacity fixture stable near configured limits
```

## SYS — System / Exactness (8)

```text
SYS-01  120-frame Immediate==Tile32
SYS-02  60-frame dense Immediate==Tile32
SYS-03  Simulation hash deterministic
SYS-04  >=300-enemy density fixture
SYS-05  >=700 combined projectile/effect/pickup fixture
SYS-06  600-frame normal headless smoke
SYS-07  Existing APP2 map tests remain PASS
SYS-08  Full Golden + gpu2d regression remains PASS
```

## VIS — Visual / Evidence (6)

```text
VIS-01  Early-game capture
VIS-02  Mid-build capture
VIS-03  Nova/Field FX capture
VIS-04  Elite encounter capture
VIS-05  Late-density capture remains readable
VIS-06  Staged APP2_002 showcase + technical HUD evidence
```

---

# 51. Completion Contract

Every mandatory ID requires:

```text
Requirement
→ Implementation Evidence
→ Verification Evidence
→ Exact Result
→ PASS
```

Rules:

> **No evidence = NOT DONE**

> **File exists ≠ behavior verified**

> **A checker that only reads PASS labels does not grant PASS**

> **Screenshot ≠ deterministic correctness proof**

> **Immediate==Tile does not prove both backends are semantically correct if the application generates wrong commands**

Feature-directed tests remain mandatory.

---

# 52. Required Tests

At minimum add/update tests equivalent to:

```text
gpu2d_test_facility_director
gpu2d_test_facility_enemies
gpu2d_test_facility_weapons
gpu2d_test_facility_upgrades
gpu2d_test_facility_spatial
gpu2d_test_facility_app2_002_visual
gpu2d_test_facility_app2_002_density
gpu2d_test_facility_app2_002_determinism
```

Naming can vary, but evidence mapping must be exact.

---

# 53. Spatial Oracle Corpus

Minimum random/directed validation:

```text
>=100 deterministic fixtures
```

covering:

```text
empty cells
cell boundaries
world edges
dense clusters
enemy exactly on grid boundary
projectile crossing cell boundary
nearest-target ties
pickup magnet across adjacent cells
```

Tie-breaking must be deterministic.

---

# 54. Dense Fixture

Required deterministic fixture:

```text
seed fixed
>=300 enemies
>=700 combined bullet/effect/pickup entities
four weapon families active
60 rendered frames
```

Verify:

```text
simulation stable
frame hash stable
Immediate == Tile32
no invalid texture / bounds fault
player remains alive by fixture design
```

This is not a balancing test.

---

# 55. Normal Gameplay Smoke

Required:

```text
600 frames
fixed seed
normal director
no fixture injection
```

Verify:

```text
enemy spawn
combat
XP
at least one Level-Up
no crash
no invalid state
deterministic hash
```

---

# 56. Report

Required report:

```text
docs/reports/REPORT_APP2_002_FACILITY_OMEGA_GAMEPLAY_CORE_EXPANSION.md
```

Must include:

```text
START_COMMIT
END_COMMIT
MAP R3V2 baseline commit
architecture changes
director phase table
enemy table
weapon table
upgrade table
spatial-grid design
capacity limits
normal-game vs showcase distinction
test commands
exact full CTest XX/XX result
Immediate/Tile equality results
simulation hashes
density fixture counts
visual evidence paths
64-row acceptance matrix
known limitations
```

---

# 57. APP2_001 Report Synchronization

Because the prior APP2_001 report predates MAP R3/R3V2 closure, APP2_002 implementation must also update or append closure information so repository documentation no longer claims:

```text
MAP VISUAL/SEMANTIC REWORK REQUIRED
```

The updated APP2_001 record should state:

```text
MAP R3 technical PASS
R3V2 visual polish complete
M13 VISUAL PASS
map-first HOLD lifted
```

Do not rewrite historical review results; add a clear closure/update section.

---

# 58. Visual Review Gate

APP2_002 is not complete until the actual new gameplay captures are visually reviewed.

The Agent must not self-grant final visual PASS based only on:

```text
PNG exists
hash exists
capture command succeeded
```

Visual review is owner/reviewer authority.

---

# 59. Recommended Implementation Order

```text
1. Sync APP2_001 closure docs
2. Introduce Director config
3. Add GameplaySpatialGrid
4. Validate grid against brute-force
5. Activate Runner
6. Activate Elite
7. Refactor weapon state/common weapon update contract
8. Implement Orbit Drone
9. Implement Plasma Nova
10. Implement Energy Field
11. Expand Level-Up choices/build state
12. Add repair pickup
13. Add phase escalation
14. Add density fixture
15. Add telemetry
16. Immediate/Tile equality
17. 600-frame normal smoke
18. Full regression
19. Generate V1–V7 captures
20. Generate staged APP2_002 showcase
21. Write report + exact acceptance matrix
```

Do not start with visual effects before the gameplay/state architecture is stable.

---

# 60. Expected Result

At the end of APP2_002, a normal play session should visibly progress like:

```text
0:00
Engineer + Pulse Shot
few Drone/Crawler

↓ Level Up

Orbit Drone unlocked

↓ time / kills

Runner + Tank pressure
XP field
weapon upgrades

↓ later

Plasma Nova
Energy Field
Elite appears

↓ swarm phase

hundreds of enemies
multi-weapon effects
player remains readable
HUD shows build
map remains navigable
```

The result should finally feel like:

> **a real survivor-like game running on the project’s general-purpose RISC-V + FPGA 2D GPU software stack**

rather than a technical combat prototype.

---

# 61. Final Instruction to Local Agent

Do not optimize for “maximum feature count”.

Optimize for:

```text
coherent survivor loop
determinism
scalable entity handling
GPU-feature-visible weapons
backend exactness
readable high-density combat
```

The four mandatory weapons are enough for this task.

Boss, Chain Arc and Meteor are intentionally deferred.

When the 64 mandatory acceptance IDs have exact evidence, stop and submit the report for formal review.
