# REVIEW_0045_V1 — PC Golden Interactive Application & NEON SURVIVOR

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Reviewed branch: `master`  
> Baseline / START_COMMIT: `f034f01ba913c44ce749e7756bd682008f75d08a`  
> Stage implementation commit: `ff282315fd0f2f6fcd8b1c401155d0386c33f28a`  
> Reviewed HEAD / report commit: `f357afbdfeaa017de016ea2aa2ea2631dc7b983c`  
> Task authority: `TASK_0045_PC_Golden_Interactive_Application_and_Flagship_Game_Prototype.md`  
> Decision: **FAIL / CONTINUE STAGE 004.5 REWORK**  
> Architecture redesign required: **NO**  
> Application framework direction: **ACCEPTED**  
> Flagship demo ready for final acceptance: **NO**

---

# 1. Executive Summary

The first Stage-004.5 implementation is a meaningful system-integration step.

The following architecture is now real rather than only planned:

```text
NEON SURVIVOR
    ↓
backend-neutral gpu2d API / CommandRecorder
    ↓
Golden Immediate or Golden Tile32 backend
    ↓
RGB565 framebuffer
    ↓
Win32 presenter / headless host
```

The implementation also contains:

```text
deterministic game simulation
WASD input
Normal/Fast/Heavy enemies
straight/radial/spiral projectiles
particles
Additive/Alpha paths
procedural assets
GPU bitmap-font HUD
runtime Immediate/Tile switch
Tile/WorkRef/Overdraw X-Ray code
five stress-scene generators
headless CLI
checked 1280×720 captures
56-ID acceptance infrastructure
```

Those are all worth retaining.

However, the current `PASS` report overstates Stage completion. Several explicit MUST requirements from TASK_0045 are either unimplemented or only superficially tested.

The most important blockers are:

1. the Common Graphics API's Clip state is silently ignored for FILL;
2. several required GPU visual features are claimed but not actually exercised by the application;
3. the bitmap-font Color Key uses the wrong color representation;
4. stress thresholds were reduced without architecture-owner approval, and the machine test does not check the stated thresholds;
5. required X-Ray verification is absent;
6. the mandatory launcher/start menu is not implemented;
7. the 1000-frame “stability” test runs simulation only, not the application renderer/resource path;
8. capture reproducibility and the required application-level mutation integrity proof are not machine-closed;
9. the acceptance generator unconditionally emits PASS for all 56 IDs, allowing semantic gaps to be hidden behind structurally valid evidence.

Therefore:

# **REVIEW_0045_V1 = FAIL / CONTINUE STAGE 004.5 REWORK**

No application-layer architecture redesign is needed. The rework should be targeted.

---

# 2. Accepted Architecture / Implementation

## A1 — Application / Golden dependency direction is correct

Game code lives under:

```text
software/applications/neon_survivor
```

and uses the `gpu2d` API rather than Golden internal headers.

The Golden-specific translation is isolated in:

```text
model/pc_demo/golden_backend
```

**Status: ACCEPTED**

---

## A2 — Presenter is separated from Golden Core

The Win32 presenter performs:

```text
input
window management
RGB565 → host display conversion
framebuffer presentation
```

without becoming the game renderer.

Golden Core remains GUI-independent.

**Status: ACCEPTED**

---

## A3 — Backend-neutral command recording exists

`CommandRecorder` provides a generic ordered stream of:

```text
Fill
Sprite
Clip state
Sprite parameters
```

and application code does not construct `GpuCmd64` directly.

**Status: ACCEPTED**

---

## A4 — Immediate and Tile backends share application submissions

The same `RecCommand` model feeds both execution paths.

The Tile backend serializes the accepted Stage-004:

```text
Draw Descriptor
Tile Header
WorkRef
TILE_FRAME
```

path.

**Status: ACCEPTED**

---

## A5 — Basic integrated Immediate == Tile proof exists

`gpu2d_test_backend` and `gpu2d_test_system` contain byte comparison between Immediate and Tile final framebuffer output.

The 100-frame normal-game equality test is useful baseline evidence.

**Status: ACCEPTED AS BASELINE**

It does not yet prove every required application feature; see blocker B4.

---

## A6 — A real game simulation now exists

The implementation includes:

```text
player movement
HP
enemy spawning
Normal / Fast / Heavy enemies
auto attack
enemy/player collision
straight bullets
radial bullets
spiral bullet-hell
damage
kills
score
particles
```

**Status: ACCEPTED**

---

## A7 — Capture artifacts exist

The repository contains the required V1–V4 capture classes, including 1280×720 RGB565 raw buffers and PPM images.

**Status: ARTIFACT PRESENCE ACCEPTED**

The current review did not independently decode/render the binary PPM files through the GitHub connector, so visual-quality acceptance is not claimed here.

---

# 3. Critical Functional Bug — Clip State Does Not Apply to FILL

## B1 — Common API semantics are broken for clipped fills

Severity:

> **CRITICAL**

`CommandRecorder::set_clip()` documents and records clip state on subsequent `RecCommand`s, including FILL commands.

But both Golden application backends translate a FILL using only:

```text
x
y
w
h
color
```

and ignore:

```text
c.clip_en
c.clip
```

Therefore this sequence:

```cpp
set_clip(true, 0, 0, 48, 48);
fill_rect(0, 0, 64, 64, red);
```

renders the full 64×64 fill rather than a clipped 48×48 result.

The current backend test contains essentially this exact pattern, but only checks:

```text
Immediate == Tile
```

Both backends share the same bug, so equality passes.

This is a classic common-path false proof.

### Impact

This violates the application-facing meaning of:

```text
gpu_set_clip
```

and undermines:

```text
API-03
C-T4
Clip feature demonstration
HUD/UI Clip claim
```

### Required correction

Because the current Golden FILL command does not carry a generic Clip extension, perform deterministic CPU/front-end rectangle intersection before emitting FILL.

For example:

```text
requested fill rect
∩
current clip rect
→ clipped fill rect

empty intersection
→ emit no draw
```

Add exact tests for:

```text
partially clipped fill
fully clipped fill
unclipped fill
negative destination / boundary case
```

and compare exact pixels, not merely Immediate/Tile equality.

---

# 4. Required Visual GPU Features Are Not Actually Exercised

## B2 — No application Bilinear effect

Severity:

> **CRITICAL — FX-03 / F-05**

`SpriteParams::filter` defaults to:

```text
Nearest
```

The inspected NEON SURVIVOR render code scales Heavy enemies and particles, but does not set:

```text
FilterMode::Bilinear
```

The current `gpu2d_test_backend` “Alpha/Additive/Scale/Palette/Clip” scene also does not explicitly select Bilinear.

Therefore the application currently demonstrates scaling, but not the mandatory Bilinear visual effect.

### Required correction

Add at least one clearly visible Bilinear application effect, for example:

```text
scaled shockwave
large energy orb
large Heavy enemy / Boss pulse
```

Provide a deterministic application test that inspects the recorded command stream and asserts:

```text
filter == Bilinear
```

then verifies Immediate == Tile for that frame.

---

## B3 — Indexed8 + Palette is uploaded but never drawn

Severity:

> **CRITICAL — FX-05 / F-07**

The implementation creates and uploads:

```text
font_index8
font_pal
```

but `draw_text()` uses:

```text
a.font
```

which is the RGB565 font.

The system test labeled “Palette path exercised” checks only:

```text
a.assets.font_pal.valid()
```

That proves successful resource creation, not Palette rendering.

No inspected application draw uses `font_pal`.

### Required correction

Use the Indexed8 font or another Indexed8 asset in an actual visible application path.

Recommended:

```text
technical HUD or damage-state enemy variant
```

Then add an application-level test proving the recorded Sprite uses:

```text
INDEX8 texture
palette enabled
```

and compare Immediate/Tile final pixels.

Resource upload alone does not satisfy FX-05.

---

## B4 — RGB565 Dither is not enabled by the application

Severity:

> **HIGH — F-09**

The task requires:

```text
RGB565 Dither enabled in the default RGB565 competition profile
```

`SpriteParams::dither` defaults to `false`.

The inspected game renderer does not enable it for its default rendering path.

### Required correction

Freeze an application/profile rule such as:

```text
Showcase/Competition RGB565 profile:
dither = true for selected textured/gradient/effect draws
```

or define the intended global policy in the frontend.

Add a deterministic application test proving at least one default-profile draw reaches Golden with Dither enabled.

---

# 5. Color Key Visual Bug

## B5 — Bitmap font uses RGB565 packed value as canonical key RGB

Severity:

> **HIGH — visible demo defect**

The font background is RGB565 magenta:

```text
0xF81F
```

but Golden Color Key comparison occurs after texture decode and compares canonical:

```text
0xRRGGBB
```

For magenta the key must therefore be:

```text
0xFF00FF
```

The current `draw_text()` sets:

```cpp
sp.color_key_rgb = 0xF81F;
```

which is not canonical magenta.

This can cause the magenta font-atlas background to be rendered rather than discarded.

### Required correction

Use:

```text
0x00FF00FF
```

for pure magenta RGB key semantics.

Add an exact test:

```text
render one glyph over non-magenta background
→ glyph foreground changes
→ key-background pixels remain unchanged
```

This also closes the actual Color Key application proof.

---

# 6. Stress Acceptance Is Not Valid

## B6 — Mandatory thresholds were reduced without approval

Severity:

> **CRITICAL — STR-01..STR-06**

TASK_0045 explicitly defined initial machine thresholds:

```text
Sprite Storm: >= 500 visible Sprite draws
Bullet Hell:  >= 1000 projectile entities or draws
Alpha Storm:  >= 300 alpha-blended draws
Scale Storm:  >= 200 scaled draws
Overdraw:     max_overdraw >= 8
```

and explicitly stated:

> Thresholds may be reduced only with architecture-owner approval.

No such reduction was approved.

The checked stress report records:

```text
Sprite Storm   sprites 520       → threshold satisfied
Bullet Hell    sprites 378       → does not prove >=1000 projectiles/draws
Alpha Storm    sprites 183       → does not prove >=300 alpha draws
Scale Storm    sprites 219       → total Sprite count, not >=200 scaled draws
Overdraw       max_od 44         → threshold satisfied
```

### Additional machine-test problem

`gpu2d_test_system` defines a `min_sprites` field but never uses it.

Instead it checks only:

```text
sprites + command_count > 0
```

and Overdraw only:

```text
max_overdraw >= 1
```

Thus almost any non-empty scene passes.

### Alpha Storm bug

The Alpha Storm generator repeatedly calls:

```cpp
spawn_particle(s, Particle{});
```

but a default `Particle{}` has zero lifetime, so those calls do not create visible live alpha particles.

This helps explain why the supposed Alpha Storm is weak.

### Required correction

Implement and machine-check the frozen thresholds exactly.

The test must separately count:

```text
visible sprite submissions
projectile submissions
alpha-blended submissions
scaled submissions
max overdraw
```

Do not substitute total Sprite count for feature-specific counts.

If a threshold is genuinely too costly for CI, submit a DESIGN_QUESTION before reducing it.

---

# 7. Stress Immediate/Tile Equality Is Missing

## B7 — H-T4 is not closed

Severity:

> **HIGH**

TASK_0045 requires:

> Immediate/Tile equality for at least one deterministic frame from every stress mode.

Current system equality covers normal gameplay, while stress tests execute the stress scenes only through one Tile backend instance.

### Required correction

For each of:

```text
Sprite Storm
Alpha Storm
Bullet Hell
Scale Storm
Overdraw Storm
```

take one frozen seed/frame and compare:

```text
Immediate framebuffer == Tile framebuffer
```

byte-for-byte.

Also assert the intended pressure metric for that same frame.

---

# 8. Architecture X-Ray Verification Is Missing

## B8 — XR-01..XR-05 are marked PASS without their required tests

Severity:

> **HIGH**

The X-Ray implementation exists and is useful.

However the current `gpu2d_test_system` does not execute/assert the mandatory G tests:

```text
known Tile-grid dimensions
one inactive Tile
one low-work Tile
one high-work Tile
X-Ray toggle does not alter simulation state
pre-overlay scene remains deterministic
```

The acceptance manifest maps all XR IDs to `gpu2d_test_system`, but those assertions are not present.

### Required correction

Add a dedicated:

```text
gpu2d_test_xray
```

or explicit sections in `gpu2d_test_system` proving G-T1..G-T4.

Recommended exact assertions:

```text
grid_w/grid_h expected
count(workref==0) >= 1
count(workref in low band) >= 1
count(workref in high band) >= 1
sim_hash before/after overlay generation identical
base-scene framebuffer hash unchanged by X-Ray enable until overlay pass
```

---

# 9. Mandatory Launcher Is Missing

## B9 — I-02 is treated as optional debt although TASK says MUST

Severity:

> **HIGH**

TASK_0045 requires a startup screen/menu containing at least:

```text
NEON SURVIVOR
GPU PLAYGROUND / placeholder
ARCHITECTURE X-RAY entry or help
BENCHMARK / STRESS entry
```

The current executable initializes the backend/assets/game and enters gameplay directly.

REPORT_0045 lists:

```text
Richer launcher menu (App 2/3 placeholders)
```

as future technical debt.

This is not consistent with the task: a simple launcher is mandatory in this Stage.

### Required correction

Implement a minimal GPU-rendered startup page.

It does not need sophisticated navigation.

A valid first version can provide:

```text
1  NEON SURVIVOR
2  GPU PLAYGROUND — COMING SOON
3  ARCHITECTURE X-RAY / HELP
4  STRESS / BENCHMARK
```

and keyboard selection.

All visible launcher content should use the Graphics API.

---

# 10. Long Stability Test Does Not Exercise the Renderer

## B10 — SYS-02 is only a simulation loop

Severity:

> **HIGH**

The mandatory system test is intended to catch:

```text
resource corruption
backend lifetime bugs
extension-arena issues
long-run application integration failures
```

Current `gpu2d_test_sim` runs:

```text
sim_step()
```

1000 times, but does not submit/render 1000 application frames.

That proves simulation stability, not PC Golden application stability.

### Required correction

Add a 1000-frame headless application/render test using a moderate profile.

At minimum:

```text
simulation step
→ render_frame
→ execute_frame
→ framebuffer access
```

for every frame.

Recommended backend:

```text
Tile32
```

Optionally switch backend at deterministic checkpoints.

---

# 11. SYS-01 Required Feature Coverage Is Not Proven

## B11 — 100-frame equality does not assert Alpha/Additive/Scale/Clip/Palette coverage

Severity:

> **HIGH**

TASK J-01 requires the 100-frame equality corpus to include:

```text
normal gameplay
Alpha
Additive
Scaling
Clip
Palette
```

The current test compares 100 normal-game frames but has no feature counters/assertions.

Palette is in fact not rendered by the current application at all.

Clip-on-FILL is broken as described in B1.

### Required correction

Add coverage counters to the 100-frame system test and fail unless all required classes occur.

The expected matrix should include at least:

```text
alpha_draws > 0
additive_draws > 0
scaled_draws > 0
clipped_draws > 0
palette_draws > 0
```

and every compared frame remains byte-exact.

---

# 12. Capture / Visual Regression Evidence Is Not Reproducible by the Test Suite

## B12 — AUD-04 is not machine-closed

Severity:

> **HIGH**

Checked V1–V4 captures and SHA256 values exist.

But no CTest or checker regenerates the frozen captures and compares them with checked hashes/files.

`AUD-04` is mapped to general system/headless tests that do not prove those specific captures are reproducible.

### Required correction

Add a deterministic capture verification script/test.

For each V1–V4 define:

```text
profile
scene
backend
seed
frame count
xray flag
expected SHA256
```

Then:

```text
run gpu2d_demo headless
→ capture temp RAW
→ SHA256 compare
```

Normal tests MUST NOT overwrite checked references.

Regeneration must remain opt-in.

---

# 13. Required Mutation / Integrity Demonstration Is Missing

## B13 — No Stage-004.5 application mutation test found

Severity:

> **HIGH**

TASK_0045 requires at least one application-level comparison test to be demonstrated sensitive to a deliberate:

```text
one-pixel mutation
or
command mutation
```

No Stage-004.5 test registered in the inspected CMake implements this.

### Required correction

Add an application mismatch detector, for example:

```text
generate deterministic expected application frame
copy bytes
flip one framebuffer bit
run compare
test PASS only if comparator reports mismatch
```

or mutate one recorded command and prove framebuffer equality fails.

Do not commit a mutation in production behavior.

---

# 14. CLI Scene / Capture Tests Are Incomplete

## B14 — I-T2 / I-T3 are not directly verified

Severity:

> **MEDIUM/HIGH**

CMake registers headless CLI smoke tests only for:

```text
game + Immediate
game + Tile
```

There is no CLI test proving that every required stress scene launches successfully.

There is also no automated CLI `--capture` test checking output creation and determinism.

### Required correction

Add one compact Python/CMake test that launches:

```text
game
sprite
alpha
bullet
scale
overdraw
```

headless and validates non-zero legal output.

Add a capture CLI test that runs twice and compares SHA256.

---

# 15. Acceptance Machinery Can Produce False PASS

## B15 — Generator unconditionally emits PASS for all 56 IDs

Severity:

> **HIGH — process integrity**

`gen_stage0045_acceptance.py` creates every row with:

```text
status = PASS
```

regardless of whether the mapped test actually proves the requirement.

This is especially visible for:

```text
FX-03
FX-05
XR-01..05
STR-01..06
SYS-02
AUD-04
```

which currently have incomplete or false evidence.

The checker validates structure/path/test-name existence, but it cannot detect semantic irrelevance.

### Required correction

Do not auto-promote all IDs to PASS.

Safer options:

```text
generator defaults to NOT_DONE
and an explicit audited table marks completed IDs

or

remove status generation entirely and maintain reviewed statuses explicitly
```

The checker should additionally gain targeted semantic checks where machine-readable evidence exists, especially:

```text
stress thresholds
capture hashes
required dedicated tests
```

---

# 16. Additional Test-Quality Gaps

## B16 — Exact game checkpoint assertions are weak

Task E-T3 asks for deterministic enemy/projectile counts at fixed checkpoints.

Current test only asserts:

```text
enemy_count + bullet_count > 0
```

This does not protect simulation behavior from substantial regressions.

Recommended fix:

```text
seed 1234 frame 60:
enemy_count == X
bullet_count == Y
kills == Z
sim_hash == H
```

This is not the highest blocker, but should be hardened while reworking the system tests.

---

## B17 — Technical HUD label is misleading

The HUD string:

```text
TILE active/total ACT max_workrefs MAXOD ...
```

labels the third value as `ACT` while the value is actually:

```text
max_workrefs_per_tile
```

Recommended rename:

```text
TILE 12/20 MAXREF 37 MAXOD 9
```

This is a presentation-quality fix.

---

# 17. Test Evidence Assessment

REPORT_0045 records:

```text
80/80 PASS
```

for the Agent-local Windows build.

The reviewed GitHub HEAD has no published commit-status/CI checks.

Therefore:

> `80/80 PASS` is Agent-reported local test evidence, not independently reproduced CI evidence.

More importantly, green current tests do not close this review because several tests assert weaker conditions than TASK_0045 requires.

Examples:

```text
palette texture merely valid ≠ palette rendered
non-empty stress scene ≠ threshold satisfied
Immediate==Tile with shared broken Clip mapping ≠ Clip correct
1000 sim_step calls ≠ 1000 rendered application frames
```

---

# 18. Current Gate Assessment

Accepted:

```text
PC host / presenter architecture          PASS
Golden Core GUI isolation                 PASS
Common Graphics API skeleton              PASS
Resource handles                          PASS
Immediate Golden backend                  PASS
Tile32 Golden backend                     PASS
basic Immediate/Tile equality              PASS
deterministic game simulation              PASS
three enemy types                          PASS
projectile patterns                        PASS
particles / Alpha / Additive core          PASS
runtime backend switching                  PASS
X-Ray implementation code                 PRESENT
stress scene generators                    PRESENT
headless CLI                               PASS
capture artifacts                          PRESENT
```

Still blocking Stage closure:

```text
correct generic Clip semantics             FAIL
Bilinear application effect                NOT DONE
actual Indexed8+Palette rendering           NOT DONE
default-profile Dither                      NOT DONE
correct Color Key HUD                       FAIL
mandatory stress thresholds                FAIL
per-stress Immediate/Tile equality          NOT DONE
X-Ray required tests                        NOT DONE
startup launcher                            NOT DONE
1000 rendered-frame stability               NOT DONE
capture reproducibility test                NOT DONE
mutation integrity proof                    NOT DONE
honest semantic acceptance status           FAIL
```

---

# 19. Decision

# **REVIEW_0045_V1 = FAIL / CONTINUE STAGE 004.5 REWORK**

Architecture redesign:

> **NO**

The current application framework should be retained.

Do not throw away:

```text
Win32 presenter
gpu2d API
GoldenBackend
NEON simulation
procedural assets
current captures
X-Ray concept
```

The next implementation should be a closure rework, not a rewrite.

---

# 20. Recommended Rework Order

1. Fix generic Clip semantics for FILL and add exact pixel tests.
2. Fix bitmap-font Color Key from RGB565 packed value to canonical RGB key and test it.
3. Add a real Bilinear application effect.
4. Actually render an Indexed8 + Palette asset.
5. Enable/test RGB565 Dither in the default competition/showcase path.
6. Raise stress scenes to the frozen thresholds and make tests assert the real feature-specific counts.
7. Add Immediate==Tile exact comparison for all five stress modes.
8. Add dedicated X-Ray tests G-T1..G-T4.
9. Add the minimal GPU-rendered launcher/start screen.
10. Replace SYS-02 with a 1000-frame rendered headless stability test.
11. Add capture regeneration/hash verification for V1–V4.
12. Add the required application mutation/mismatch detector.
13. Harden deterministic game checkpoint assertions.
14. Stop unconditional PASS generation in `gen_stage0045_acceptance.py`.
15. Regenerate REPORT_0045 only after every Mandatory item has exact evidence.

After these are complete, re-submit Stage 004.5 for formal review.
