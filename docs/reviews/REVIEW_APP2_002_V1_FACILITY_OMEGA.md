# REVIEW_APP2_002_V1 — FACILITY-Ω Gameplay Core Expansion Formal Review

> Project: RISC-V + FPGA 2D GPU  
> Application: FACILITY-Ω  
> Review date: 2026-09-17  
> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed HEAD: `9c355d4efadcd08160262902bc7793933d863b32`  
> Baseline: `57c5eb35d0c64aa7c3940f3d6f11c11f018f8376`  
> Task: `TASK_APP2_002_FACILITY_OMEGA_Gameplay_Core_Expansion.md`  
> Agent report: `REPORT_APP2_002_FACILITY_OMEGA_GAMEPLAY_CORE_EXPANSION.md`

---

# 1. Formal Decision

## Architecture / implementation direction

# **PASS WITH ACTIONS**

The APP2_002 implementation has the correct overall architecture:

- deterministic four-phase Director;
- Runner / Elite integration;
- Pulse / Orbit / Nova / Field;
- Level-Up build choices;
- CPU-side GameplaySpatialGrid;
- entity-capacity growth;
- Immediate / Tile32 exactness corpus;
- dense correctness fixture;
- preserved MAP R3V2 environment;
- no GPU ISA / Golden semantic expansion.

There is no reason to redesign the Application 2 architecture.

## Formal Stage APP2_002

# **FAIL — NARROW CLOSURE REQUIRED**

The Stage cannot yet be closed because:

1. there is a real multi-level-up gameplay correctness bug;
2. the signed RNG helper is incorrect and is already used by XP spawning;
3. all mandatory VIS acceptance IDs remain pending;
4. V3 / V4 / showcase visual artifacts are byte-identical aliases rather than independent evidence;
5. the report does not contain an exact `END_COMMIT` value.

These are narrow closure items. Do **not** restart APP2_002.

---

# 2. What is accepted

The following parts are accepted as the new gameplay foundation.

## 2.1 Game Director

The four phases are centralized and deterministic:

```text
Early    0 s
Build  120 s
Swarm  300 s
Late   480 s
```

The phase data includes:

```text
spawn interval
active cap
group size
enemy weights
elite interval
```

The phase model is suitable for later tuning.

---

## 2.2 Enemy roster

Five enemy roles are now represented:

```text
Drone
Crawler
Tank
Runner
Elite
```

Runner and Elite are not merely renamed common enemies: they have distinct speed / HP / damage / XP characteristics.

Elite also has distinct visual treatment through scale + additive aura.

Accepted.

---

# 3. Weapon architecture

All four required weapons exist simultaneously.

## Pulse Shot

Uses spatial nearest-target lookup and local projectile broad-phase.

## Orbit Drone

Uses integer LUT / fixed-point-style phase progression rather than runtime trigonometry.

## Plasma Nova

Uses the intended GPU feature combination:

```text
Scaling
Bilinear
Additive
Alpha fade
```

## Energy Field

Uses:

```text
Straight Alpha
Scaling
squared-distance periodic damage
```

The multi-weapon GPU narrative is therefore now real rather than merely planned.

Accepted.

---

# 4. GameplaySpatialGrid

The application-side broad-phase is structurally sound for this stage.

Key positives:

```text
128 px grid cells
32×32 world grid
fixed storage
stable slot IDs
sorted rectangle-query results
deterministic nearest tie-break
```

The brute-force oracle test is a meaningful independent check.

This successfully removes the most dangerous:

```text
projectile × all-enemies
```

collision pattern.

Accepted.

---

# 5. Capacity / dense correctness evidence

The current evidence reports:

```text
enemy capacity      1024
projectile capacity 2048
pickup capacity     2048
```

and a 60-frame dense corpus with at least:

```text
300 enemies
700+ combined live projectile/effect/pickup objects
```

Immediate and Tile32 are compared byte-for-byte per frame.

This is valuable correctness evidence.

It is **not** FPGA performance evidence, and the report correctly avoids making that claim.

Accepted.

---

# 6. Regression evidence

Agent-local full regression is reported as:

```text
101 / 101 PASS
0 FAIL
```

with the complete Golden / gpu2d suite included.

There is currently no published GitHub commit-status / CI result for HEAD, so classify this as:

> **Agent-local verified, not independently reproduced CI.**

This distinction does not invalidate the implementation evidence.

---

# 7. BLOCKER B1 — Multiple level gains collapse into one upgrade event

This is the most important gameplay correctness issue found in review.

Current XP collection logic can execute:

```cpp
while (player.xp >= player.xp_need) {
    subtract threshold;
    ++player.level;
    compute next threshold;
    leveled = true;
}
```

After processing all collected pickups, it only does:

```cpp
if (leveled) {
    level_up_pending = true;
    generate_choices();
}
```

There is only a boolean pending state.

Therefore if one simulation tick crosses two or more level thresholds:

```text
Player level increases multiple times
but
only one Level-Up choice event is awarded
```

This breaks the expected Survivor-like rule:

> **one level gained → one upgrade selection**

A naturally possible example is:

```text
Level 1
XP = 9 / 10

same tick collects:
Elite XP = 12
Crawler XP = 2
Drone XP = 1

total added = 15

Level 1 → Level 2
Level 2 → Level 3

but only one 3-choice panel is queued.
```

## Required fix

Introduce an explicit queue/count, e.g.:

```text
pending_levelups
```

For every threshold crossed:

```text
++pending_levelups
```

When the user applies one upgrade:

```text
--pending_levelups

if pending_levelups > 0:
    generate next 3 choices
    remain paused
else:
    resume game
```

## Required test

Directed case must cross at least two level thresholds in one tick and verify:

```text
level increments by 2
two separate choice events are required
simulation remains paused between them
```

This is a **Stage blocker**.

---

# 8. BLOCKER B2 — `Rng::irange()` is incorrect for negative bounds

Current helper effectively does:

```cpp
range(
    static_cast<u32>(lo),
    static_cast<u32>(hi)
)
```

For:

```cpp
irange(-6, 6)
```

the negative `lo` becomes a very large unsigned number.

Then:

```text
hi <= lo
```

is true inside `range()`, so the function returns `lo`.

After cast back to signed:

```text
irange(-6, 6) == -6
```

every time.

APP2_002 currently uses this in `spawn_xp()`:

```text
x + irange(-6, 6)
y + irange(-6, 6)
```

Therefore the intended XP scatter is not random:

```text
every XP offset is effectively (-6, -6)
```

## Required fix

Implement signed-range arithmetic without unsigned wrap.

Example semantic contract:

```text
irange(lo, hi)
returns [lo, hi)
for signed i32 values
```

## Required tests

At minimum:

```text
irange(-6, 6) can produce negative, zero-region and positive values
all outputs satisfy -6 <= x < 6
fixed seed reproduces sequence
```

Also add one directed XP-spawn test showing offsets are not permanently `(-6,-6)`.

This is a real implementation bug and must be closed.

---

# 9. VISUAL / EVIDENCE BLOCKER B3 — mandatory VIS IDs remain pending

The Agent report correctly leaves:

```text
VIS-01 .. VIS-06
```

as:

```text
PENDING OWNER REVIEW
```

Therefore the mandatory 64-ID acceptance contract is not complete.

The previous MAP M13 approval cannot automatically approve:

```text
Orbit
Nova
Field
Elite
Level-Up build UI
300-enemy density
technical HUD
```

APP2_002 requires a new visual review.

Formal Stage PASS is impossible until that happens.

---

# 10. VISUAL EVIDENCE ISSUE B4 — V3, V4 and showcase are exact duplicates

Repository artifact hashes show:

```text
V3_fx.png
V4_elite.png
showcase_app2_002.png
```

all use the same PNG blob.

Their `.raw` and `.ppm` artifacts are also identical.

This matches the fixture implementation: the names:

```text
fx
elite
showcase
```

currently resolve to essentially the same mixed scene.

One image is allowed to support multiple observations, but the TASK explicitly requested:

```text
V3 — FX Build
V4 — Elite Encounter
additional competition showcase candidate
```

The current artifact set does not provide three independent compositions.

## Required closure

Preferred:

```text
V3:
clearly frame Nova + Field + Orbit behavior

V4:
make Elite the obvious focal threat

showcase:
compose a deliberate competition screenshot
```

They can share the same engine and assets, but they should not be byte-identical aliases.

At minimum, the report must not present duplicated files as independent visual evidence.

---

# 11. Documentation blocker B5 — no exact END_COMMIT

The APP2_002 report contains the exact `START_COMMIT`, but `END_COMMIT` is described indirectly:

```text
query git log to obtain it
```

The task explicitly requires an exact:

```text
END_COMMIT
```

For formal closure, the final report must name the closure commit SHA directly.

Recommended workflow:

```text
implementation commit
→ report finalization / closure commit
→ report records the intended reviewed implementation SHA
  and exact report/closure SHA where applicable
```

Do not leave this implicit.

---

# 12. Nonblocking Action A1 — interactive P / R controls are still NEON-centric

The launcher help advertises:

```text
P pause
R reset
```

but the current interactive event handling toggles/resets the NEON simulation state.

FACILITY simulation continues through its own path.

This means the advertised pause/reset behavior is not cleanly implemented for FACILITY-Ω.

This predates much of APP2_002 and is not a core architectural blocker, but should be fixed before calling the game presentation-ready.

Recommended:

```text
P:
Facility pause flag gates sim_step

R:
sim_reset(fo, seed)
set viewport
camera_follow
```

---

# 13. Nonblocking Action A2 — free-slot insertion is linear scan

Entity insertion currently reuses slots via:

```text
for every slot:
    if dead:
        reuse
```

This is deterministic and works at current capacities, but its worst-case insertion cost rises with:

```text
1024 enemies
2048 bullets
2048 pickups
```

Before actual RISC-V high-density optimization, consider:

```text
free-index stack / free list
```

while preserving stable slot IDs.

Not required for this closure.

---

# 14. Nonblocking Action A3 — `nearest()` still scans all 1024 grid cells

The spatial grid improves candidate correctness and eliminates full enemy scans inside projectile collision.

However, nearest-target lookup still loops across all:

```text
32 × 32 = 1024 cells
```

and prunes using cell-distance bounds.

At current Pulse cadence this is acceptable.

For later RISC-V optimization, use expanding local rings or bounded nearby-cell traversal.

Not required for APP2_002 closure.

---

# 15. Known Nova slot-reuse semantic

The Agent report explicitly documents that Nova records one-hit state by stable enemy slot.

If a dead slot is reused during the same Nova wave, the new entity inherits the slot's “already hit” status.

Given current spawn rules:

```text
new enemies spawn outside the camera
Nova radius remains local to the player
Nova duration is short
```

this has little practical effect.

Because it is explicitly documented, it does not block APP2_002.

A future entity-generation ID would be cleaner if spawn rules change.

---

# 16. Acceptance summary

## Accepted

```text
PRE   4/4 technical
DIR   8/8 technical
ENM   8/8 technical
WPN  16/16 architecture/features
SPC   6/6 technical
SYS   8/8 Agent-local evidence
```

## Requires narrow closure

```text
UPG gameplay semantics:
multi-level queue bug

RNG:
signed irange bug

VIS:
6/6 owner review still pending
visual evidence aliases need separation

REPORT:
exact END_COMMIT missing
```

The 101/101 existing test result is real evidence, but the suite does not cover B1/B2.

Therefore:

> **Green regression does not override the newly identified directed correctness gaps.**

---

# 17. Required APP2_002R closure order

Do only this narrow rework:

```text
R1  Fix signed Rng::irange
R2  Add signed-range + XP-scatter directed tests
R3  Add pending_levelups queue semantics
R4  Add multi-level-in-one-tick directed test
R5  Re-run facility tests
R6  Re-run full Golden/gpu2d suite
R7  Generate distinct V3 / V4 / showcase captures
R8  Upload / provide V1–V7 + showcase for direct visual review
R9  Update report with exact END_COMMIT and corrected evidence
```

Do **not** add:

```text
Boss
Chain Arc
Meteor
new map systems
new GPU semantics
RTL
```

until APP2_002R closes.

---

# 18. Final Verdict

# **REVIEW_APP2_002_V1 = FAIL — NARROW CLOSURE**

This is a strong implementation and the architecture should be kept.

The failure is narrow, not structural:

```text
two gameplay correctness defects
+
unfinished mandatory visual review
+
evidence/report closure issues
```

After these are corrected, APP2_002 is likely close to PASS / PASS WITH ACTIONS.
