# REVIEW_APP2_001_PARTIAL_V1 — FACILITY-Ω Partial Implementation Review

> Project: RISC-V + FPGA 2D GPU  
> Application: Application 2 — FACILITY-Ω  
> Review type: Partial implementation / visual-effect review  
> Review date: 2026-09-14  
> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Repository HEAD reviewed: `758fda91d8f05be4b508da5b4da9db0e20494382`  
> Latest functional implementation commit reviewed: `e8be685dfeb9efba806b9b1d44c2a4056df03e50`  
> Scope reached by implementation: approximately Blocks A–H, before XP / Level-Up / final HUD evidence  
> CI note: no published GitHub commit status was present at review time; local test claims are therefore not independently reproduced here.

---

# 1. Decision

## **PARTIAL REVIEW = REWORK BEFORE CONTINUING VISUAL/GAMEPLAY EXPANSION**

This is **not** a rejection of the Application 2 architecture.

The large-world/camera/application separation and first combat-loop direction are correct.

However, there are multiple **substantive visual correctness defects in the current asset pipeline**. They directly affect what appears on screen, so they should be fixed before the Agent continues with XP, Level-Up, additional weapons, Elite/Boss, or visual polish.

The most important conclusion is:

> **Current FACILITY-Ω gameplay structure may continue to be used, but the processed runtime art must not be accepted as the visual baseline.**

---

# 2. What is already correct

The following implementation direction is approved.

### Application boundary

`software/applications/facility_omega/` exists as a separate application and uses the backend-neutral `GraphicsApi`.

The existing NEON SURVIVOR remains present.

This preserves the intended product structure:

```text
Main Menu
├── NEON SURVIVOR
└── FACILITY-Ω
```

### Large world

The application freezes:

```text
MapTile = 32×32
Map Grid = 128×128
World = 4096×4096
```

World coordinates are converted to screen coordinates by the application, and only visible map tiles are submitted.

This is the correct architecture.

### Camera

Player-follow camera plus world-edge clamp is implemented.

The game can move farther than one viewport.

### First combat loop

The current implementation contains:

```text
Drone
Crawler
Tank
automatic Pulse Shot
enemy chase
touch damage
hit flash
projectile collision
kill count
off-screen culling
```

This is a valid first Survivor-like vertical slice foundation.

### Offline asset path

The split:

```text
source/
processed/
runtime/
```

is good.

Runtime loads raw GPU-oriented resources rather than PNG decoding, which is aligned with eventual RISC-V/FPGA deployment.

---

# 3. P0 — ARGB8888 runtime byte order is wrong

Current asset compiler function:

```python
def pack_argb8888(im):
    return im.tobytes("raw", "RGBA")
```

writes bytes as:

```text
R G B A
```

But the Golden ARGB8888 decoder interprets memory bytes as little-endian `0xAARRGGBB`, i.e.:

```text
B G R A
```

and decodes:

```cpp
Rgba8888::pack(p[3], p[2], p[1], p[0])
```

The Golden backend copies texture bytes without a swizzle.

Therefore current ARGB8888 runtime textures swap red and blue.

## Required fix

The offline asset compiler must emit:

```text
B G R A
```

for `ARGB8888`.

For example:

```python
return im.tobytes("raw", "BGRA")
```

or an explicitly equivalent implementation.

## Required verification

Add a tiny directed 2×1 ARGB texture:

```text
pixel0 = red
pixel1 = blue
```

Render it through the production Golden backend and verify framebuffer colors independently.

Do not test only file hashes.

---

# 4. P0 — Enemy crop definitions collapse multiple enemy poses into one sprite

The generated enemy source sheet contains two separate visual variants/poses for Drone, Crawler, Runner, Tank and Elite.

Current crop definitions such as:

```text
drone_0   box=(67,90,386,200)
crawler_0 box=(495,90,463,200)
tank_0    box=(39,390,712,330)
elite_0   box=(811,390,603,330)
```

span both variants.

The resulting runtime sprite therefore contains two enemy drawings compressed into one small texture.

This is a substantive screen-output failure.

## Required fix

Split each enemy into independent crops:

```text
drone_0
drone_1

crawler_0
crawler_1

runner_0
runner_1

tank_0
tank_1

elite_0
elite_1
```

Each runtime sprite must contain exactly one enemy.

Then use `anim` to select frame 0/1.

---

# 5. P0 — Weapon / pickup crop coordinates target the wrong regions

The current weapon source sheet is a presentation sheet, not a fixed atlas grid.

Several frozen crop boxes do not correspond to their declared semantic asset.

Examples:

```text
pulse_shot
enemy_bullet
orbit_drone
xp_small
xp_large
repair_pickup
```

are cropped from broad regions containing labels, multiple frames, or even a different asset row.

The issue is particularly clear for:

```text
pulse_shot box=(21,20,301,100)
```

which overlaps the `PLAYER PULSE-SHOT` title and multiple projectile variants rather than one clean projectile.

Likewise the current `enemy_bullet` and `orbit_drone` boxes are not aligned to the actual enemy-bullet/orbit-drone sprite regions.

## Required fix

The source sheet must be manually/art-review cropped into one selected runtime sprite per semantic asset.

Do not use broad row-band rectangles.

For animated/evolving effects, choose either:

```text
one canonical frame
```

or explicitly extract:

```text
frame_0
frame_1
...
```

---

# 6. P0 — FX crop semantics are also incorrect

The FX source sheet contains many large effects arranged freely.

Current rectangles named:

```text
spark
glow_small
glow_large
ring
explosion
trail
```

do not reliably correspond to those effects.

For example, the current `ring` crop intersects a cross-flash-like effect rather than a clean ring; other entries similarly map to a different visual element than the name implies.

## Required fix

Re-audit every FX crop visually.

Minimum accepted rule:

> Runtime asset name must describe the actual isolated image visible in that asset.

The Agent should produce a post-crop contact sheet and perform a manual semantic check before gameplay integration.

---

# 7. P0 — Player Up/Down sprite mapping is reversed

Current direction state:

```text
dir=0 down
dir=1 up
dir=2 left
dir=3 right
```

But current `player_sprite_name()` maps:

```text
dir=0 → engineer_a*
dir=1 → engineer_b*
```

The source art clearly defines:

```text
engineer_a* = UP
engineer_b* = DOWN
```

Therefore:

```text
moving DOWN shows UP art
moving UP shows DOWN art
```

## Required fix

Map:

```text
dir=0 → engineer_b*
dir=1 → engineer_a*
dir=2 → engineer_c*
dir=3 → engineer_d*
```

Add a semantic direction test, not merely a name-prefix test.

---

# 8. P1 — Current map generation will look like a random texture collage

`sim_reset()` currently fills every MapTile with a pseudo-random value from `0..7`.

Those eight processed `floor_xx` entries are not eight equivalent base-floor variants.

They include visually strong special elements such as:

```text
hazard stripe
pipe/conduit
energy panel/core
large A-3 facility marking
vent/grate-style tiles
```

Uniformly random placement means special tiles appear everywhere.

This will make the scrolling map visually noisy and will destroy the intended impression of a coherent energy facility.

## Required map-content model

Separate tile semantics:

```text
Base Floor Layer
    plain metal
    cracked metal
    stained metal
    subtle vent

Special Floor / Decal Layer
    hazard stripe
    A-3 marking
    maintenance marking
    conduit
    reactor panel
```

Recommended first-pass distribution:

```text
70–85% base floor
10–20% subtle variation
special signage/conduit only in authored zone patterns
```

Do not uniformly randomize all art assets.

---

# 9. P1 — Asset audit does not actually close Block A

The Task required two enemy frames and UI candidates.

Current runtime/audit contains only:

```text
drone_0
crawler_0
runner_0
tank_0
elite_0
```

rather than `*_0 / *_1`.

The current asset compiler also lists the UI source sheet in `SHEET_FILES` but defines no UI crops in the current `CROPS` table.

Therefore the claim “Block A-D complete” is too strong.

The UI is still rendered with the older NEON bitmap-font HUD path.

This is acceptable for a partial vertical slice, but Block A must remain OPEN.

---

# 10. P1 — Current tests can pass while visuals are wrong

`gpu2d_test_facility` checks useful structural behavior:

```text
world size
camera clamp
movement
sprite names
enemy spawn
bullet generation
kill
determinism
asset count
floor count
```

But it does not prove:

```text
crop contains the correct object
one enemy sprite contains only one enemy
UP art is used for UP
ARGB colors are correct
weapon crop is actually a bullet
map composition is visually coherent
```

This is why the current implementation can be structurally green while the actual art is wrong.

## Required additional verification

Add:

### Asset semantic visual gate

Generate:

```text
processed/contact_sheet.png
```

with each runtime sprite enlarged nearest-neighbor and labeled.

Human review is mandatory for this stage.

### Directed pixel test

For ARGB8888 channel order.

### Player direction test

Use expected source mapping.

### Map screenshot gate

Require at least one 640×360 or 1280×720 capture that shows:

```text
coherent floor
player
Drone
Crawler
Tank
Pulse Shot
props
```

before Block H is considered visually closed.

---

# 11. P1 — Enemy movement normalization is not scalable to final Survivor density

Current enemy motion computes an approximate vector length with:

```cpp
len = 1;
while ((len + 1) * (len + 1) <= dist2) {
    ++len;
}
```

This loop runs per enemy per frame.

The same pattern is also used when aiming Pulse Shot.

At 6–40 enemies this is acceptable for a prototype.

At the planned hundreds/thousands of Survivor-like entities it is unsuitable for RISC-V.

## Required direction

Before high-density scaling, replace it with one of:

```text
integer sqrt with bounded iterations
Chebyshev / Manhattan normalization
lookup/approximation
fixed-point reciprocal-length approximation
```

Do not carry the current O(distance) loop into the final workload.

---

# 12. P2 — Current entity capacities are prototype-only

Current vectors reserve/cap approximately:

```text
enemies <= 256
bullets <= 256
```

This is acceptable for the current first slice, but it does not match the eventual visual goal:

```text
500–1500 enemies
500–3000 projectiles
```

Do not treat these values as final architecture.

This is not a blocker for the current rework.

---

# 13. Visual assessment

The **source art direction itself is good**:

```text
orange engineer helmet
blue-gray suit
cyan backpack
dark mechanical enemies
red enemy sensors
purple elite
industrial blue-gray floor
yellow-black hazards
cyan facility energy
```

The source art has substantially higher presentation quality than NEON SURVIVOR.

The problem is not the art direction.

The problem is:

> **the current automatic crop/format pipeline is degrading and misidentifying the source art before it reaches the GPU.**

Therefore no new art generation is required to solve the current blocker.

Fix the extraction first.

---

# 14. Recommended immediate rework order

```text
R1  Fix ARGB8888 BGRA byte order
↓
R2  Correct player direction mapping
↓
R3  Re-crop player/enemy/weapon/pickup/FX assets manually
↓
R4  Add second enemy frames
↓
R5  Rebuild runtime manifest + contact sheet
↓
R6  Human visual review of contact sheet
↓
R7  Replace random-all-tiles map composition with semantic layers
↓
R8  Capture one real gameplay screenshot
↓
R9  Re-review visual result
↓
R10 Only then continue XP / Level-Up / UI gameplay
```

---

# 15. Gate

## Architecture

```text
Application architecture redesign required: NO
GPU/Golden semantic redesign required: NO
Large-world/camera redesign required: NO
```

## Implementation

```text
Continue adding gameplay features immediately: NO
Asset/visual rework required first: YES
```

## Current status

> **REWORK — visual correctness blocker**

The existing combat/world code should be kept.

The rework should focus narrowly on:

```text
asset extraction
pixel format
direction mapping
map composition
visual evidence
```

Do not restart the game implementation.
