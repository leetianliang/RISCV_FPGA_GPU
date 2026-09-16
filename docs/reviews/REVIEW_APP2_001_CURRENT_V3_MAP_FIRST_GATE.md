# REVIEW_APP2_001_CURRENT_V3 — FACILITY-Ω Current Implementation / Map-First Gate

> Project: RISC-V + FPGA 2D GPU  
> Application: Application 2 — FACILITY-Ω  
> Review date: 2026-09-15  
> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Current repository HEAD reviewed: `5091521c68da2e1897e7dbd431ecabf4105a9646`  
> Report END_COMMIT: `95c93501b557494cce3be565e99b7375d924e8a7`  
> Review focus: Current implementation after visual rework, with the latest project decision that gameplay expansion is paused and the map must be fixed first.  
> Decision: **HOLD / MAP REWORK REQUIRED**  

---

# 1. Decision

The current implementation should **not** be accepted as a finished Application 2 vertical slice for visual purposes.

Functional work has progressed substantially and many previous correctness problems are fixed. However, the user's latest visual review has changed the active gate:

> **Gameplay expansion is paused. The map/environment must be brought to an acceptable visual and gameplay-semantic level first.**

Against that active gate, the current implementation is still not ready.

The correct status is:

```text
GPU/Application architecture: KEEP
Combat/XP/UI code already implemented: KEEP, freeze for now
Current map visual design: REWORK
Current map collision semantics: NOT IMPLEMENTED
Current 56-ID PASS claim: NOT ACCEPTED AS REVIEW AUTHORITY
```

---

# 2. What is worth keeping

Do not restart Application 2.

Keep:

- 4096×4096 world-space model;
- 32×32 MapTile indexing;
- player-follow camera and visible-region culling;
- Common Graphics API boundary;
- Immediate / Tile32 rendering path;
- processed texture atlas pipeline;
- corrected BGRA / alpha handling;
- corrected player facing;
- corrected enemy crop/animation assets;
- bounded integer distance-root / Q8 movement accumulation;
- existing combat, XP, Level-Up and HUD code, but freeze feature expansion until map gate passes.

These are not the source of the present visual problem.

---

# 3. P0 — The floor is still visually built as 32×32 repeated tiles

The current renderer still chooses one floor texture for every logical 32×32 MapTile.

For map value 0 it alternates four `floor_base_*` textures using `tx/ty` parity.

That means the visual surface is still fundamentally:

```text
32×32 image
32×32 image
32×32 image
...
```

even though the textures have been cleaned and darkened.

Therefore the user's observation that the image still has a strong “贴图质感” is expected from the implementation itself.

## Required change

`MapTile = 32×32` may remain the logical map/culling unit.

It must stop being the primary visible art unit.

Use a hierarchy such as:

```text
logical MapTile 32×32
        ↓
MacroChunk 256×256
        ↓
Base floor material
+ long structure strips
+ decals
+ props
+ collision
+ local lighting/FX
```

The player should not be able to visually count the 32×32 grid in normal play.

---

# 4. P0 — The world still repeats periodic procedural motifs

`sim_reset()` currently introduces map patterns with `% 32` rules.

That makes large-scale layout repeat every 32 MapTiles:

```text
32 × 32 × 32px = 1024px
```

The environment renderer also repeats authored service bays on a regular 768-pixel grid.

This creates two visible repetition frequencies:

```text
~1024 px floor-layout repetition
~768 px service-bay repetition
```

Even if an individual screen looks better than the first prototype, travelling through the map will reveal a mechanical repeating pattern rather than a believable facility.

## Required change

First build one authored 1024×1024 **Hero Area**.

Do not generate the whole world from one repeating service-bay template.

Recommended first Hero Area:

```text
Central Power Test Hall
├── wide open combat floor
├── small Maintenance edge
├── small Storage edge
└── small Power Lab edge
```

Only after that area passes visual review should it be generalized into multiple MacroChunk templates.

---

# 5. P0 — Solid-looking walls and large props have no collision

The current rendering code explicitly describes the service-bay structures as visual landmarks rather than collision walls.

The application state also has no collision/obstacle layer, and `player_move()` only clamps against the 4096×4096 outer world boundary.

Therefore, visually solid objects such as:

```text
service bay walls
barriers
stacked crates
large cabinets
access-door structures
large equipment groups
```

do not currently define gameplay obstruction.

This directly conflicts with the current map-design decision:

> **If an object visually reads as a wall or major obstruction, the player should not walk through it.**

## Required change

Introduce application-layer collision semantics.

Do not change the GPU.

Minimum first version:

```text
Collision type:
AABB / tile-mask

Player:
slide against obstacles

Enemy:
same collision space, simple slide/axis resolution

Bullets:
may ignore map collision in V1 unless explicitly desired

Decorative decals / small floor props:
non-collidable
```

Collision geometry must be simpler than sprite art and authored explicitly.

---

# 6. P0 — Current obstacle density is too high once collision becomes real

Each repeated service area draws long north/south structures and many prop groups:

```text
servers
pipe elbow
console
power panel
multiple canisters
warning lamps
barriers
stacked crates
crate
barrel
cable spool
access door
power cabinet
machine wreck
```

At present this does not fully hurt movement because most of it is visual-only.

If correct collision were simply added to the existing arrangement, the map would become too restrictive for a Survivor-like game.

Therefore:

> **Do not “add collision to everything that exists now”.**
>
> First simplify the environment, then add collision to the small subset of major structures that remain.

Recommended target for one normal screen:

```text
70–80% open combat floor
10–15% large structural boundary
5–10% prop groups
remainder decals / lights / non-collision detail
```

Typical screen:

```text
3–6 collidable obstacle groups maximum
```

---

# 7. P1 — Current map data model is too weak for the desired scene

`AppState` currently stores the map primarily as:

```text
std::vector<u8> map
```

This is adequate for a one-layer tile map but not for the new visual target.

The environment now needs explicit semantic layers.

Recommended application data:

```text
BaseFloorLayer
DecalLayer
StructureLayer
CollisionLayer
PropInstances
EnvironmentFxInstances
Zone / MacroChunk metadata
```

These layers do not need new GPU primitives.

They compile down to the same existing:

```text
fill_rect
draw_sprite
alpha/additive
clip
```

commands.

---

# 8. P1 — Current scene composition still overuses individual prop assets

The latest renderer is an improvement over random scattering: props are grouped around service bays.

However, every group still tries to show too many asset types at once.

The resulting visual language risks becoming:

```text
“look at all the assets we have”
```

instead of:

```text
“this is one coherent industrial space”
```

For the first Hero Area use fewer prop families per sub-zone.

Example:

```text
Central Hall:
almost empty, markings + 1 major test structure

Storage edge:
crates + barrel only

Maintenance edge:
pipe + cabinet + console

Power Lab edge:
canister + power panel + cyan glow
```

This makes each area readable and reduces clutter.

---

# 9. P1 — The formal report itself acknowledges the remaining visual problem

The current final report lists:

> `Floor panel tiling still visible on large empty areas`

as a known limitation.

Under the old functional TASK this might be accepted as polish debt.

Under the user's current instruction — **“把地图部分先做好”** — this is no longer a minor limitation.

It is now a Stage blocker.

---

# 10. P1 — The 56-ID PASS mechanism is not evidence-driven enough

The acceptance generator currently does:

```python
REVIEWED_PASS = set(AUTHORITATIVE)
```

which marks all 56 IDs PASS by construction.

The checker then verifies:

- IDs exist;
- rows say PASS;
- evidence paths exist;
- test names appear in `ctest -N`.

It does **not** independently execute each mapped verification and promote the ID only after a passing result.

The report generator also writes:

```text
**PASS** (pending REVIEW_APP2_001)
```

unconditionally from the manifest.

Therefore:

> The current `56/56 PASS` is a bookkeeping assertion, not independent review authority.

It cannot override a substantive visual failure found by the owner.

This does not mean the underlying functional tests are invalid; it means the Stage completion claim must remain pending formal review.

---

# 11. Test evidence status

Useful tests exist for:

- BGRA/alpha correctness;
- Immediate == Tile;
- camera/world behavior;
- combat and deterministic simulation;
- 600-frame stability.

However the current report gives only:

```text
gpu2d_test_facility* 3/3 PASS
golden+gpu2d suite (Agent-local)
```

rather than an exact full-suite `XX/XX PASS` with captured output.

No published commit-status/check result is present for current HEAD.

Therefore functional regression quality looks promising, but it is still **Agent-local evidence**, not independently reproduced review evidence.

---

# 12. Current implementation versus new map target

| Area | Current | Required |
|---|---|---|
| World size | 4096×4096 | Keep |
| Camera | Follow + clamp | Keep |
| Logical tile | 32×32 | Keep |
| Visual floor | 32×32 repeated images | Replace as dominant visual unit |
| Macro structure | repeated 768px service bays | authored Hero Area / chunk templates |
| Floor pattern | periodic rules | coherent authored zones |
| Open combat space | present but framed by dense repeated structures | enlarge further |
| Props | many types per repeated bay | fewer grouped props |
| Wall/large prop collision | none | required |
| Decorative collision | none | keep none |
| Zone identity | weak/repeated | Central Hall / Maintenance / Storage / Power Lab |
| Visual gate | screenshots exist | owner-visible Hero Area approval required |

---

# 13. Recommended map-only implementation order

Freeze all new gameplay features.

Proceed only with:

```text
M1  Define semantic environment layers
M2  Define simple collision representation
M3  Design 1024×1024 Hero Area layout
M4  Reduce prop density
M5  Create broad continuous base-floor treatment
M6  Add long structure strips / boundaries
M7  Add decals separately from base floor
M8  Add 3–6 collidable structure groups per normal screen
M9  Implement player collision + slide
M10 Implement simple enemy obstacle handling
M11 Add restrained lighting / steam / glow
M12 Capture 3 map views + one scrolling sequence
M13 Owner visual review
```

Only after M13 passes should the project resume:

```text
XP/upgrade polish
more weapons
Elite/Boss
late-game density
```

The already implemented XP/upgrade code can remain in the branch but should not drive the next development work.

---

# 14. New Map Visual Acceptance Gate

The next map revision should not pass merely because “8 floor textures exist”.

Mandatory visual criteria:

1. At normal gameplay zoom, the 32×32 floor grid is not immediately obvious.
2. Three adjacent screens do not look like copies of one service-bay template.
3. At least 70% of the principal combat space is comfortably traversable.
4. Visual walls / large machinery that read as solid have collision.
5. Small decals and floor details do not collide.
6. Player can move continuously around all major obstacle groups without trap pockets.
7. Enemies do not walk directly through collidable structures.
8. Background local contrast stays below player/enemy/projectile contrast.
9. Props are grouped by environmental function rather than uniformly distributed.
10. One 1024×1024 Hero Area looks like a coherent facility even with player/enemies hidden.
11. The scene remains achievable entirely with the existing 2D GPU API.
12. Owner visually approves the map before gameplay development resumes.

---

# 15. Final Decision

## Current full Application 2 task

**NOT ACCEPTED AS FINAL PASS**

Reason is not a GPU or gameplay architecture failure.

Reason is the currently active product gate:

> the map is still visibly tile-driven, repeated, non-collidable, and too asset-dense for the desired Survivor-like battlefield.

## Current code disposition

```text
KEEP:
application architecture
camera/world
asset pipeline
combat
XP/upgrade code
tests

REWORK NOW:
map visual composition
environment layers
collision semantics
obstacle density
Hero Area

FREEZE:
new gameplay feature expansion
```

# **FORMAL STATUS: HOLD — MAP VISUAL/SEMANTIC REWORK REQUIRED**
