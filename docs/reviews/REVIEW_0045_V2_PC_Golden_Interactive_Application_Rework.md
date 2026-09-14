# REVIEW_0045_V2 — PC Golden Interactive Application Rework Review

> Repository: `leetianliang/RISCV_FPGA_GPU`  
> Branch: `master`  
> Stage baseline / START_COMMIT: `f034f01ba913c44ce749e7756bd682008f75d08a`  
> Previous reviewed HEAD: `f357afbdfeaa017de016ea2aa2ea2631dc7b983c`  
> Rework implementation END_COMMIT: `fe99e88972f20e625cde20e0339edff097f65346`  
> Current report/HEAD commit: `cd5383ad15fec7fafc79afb50022c6aa8736f2ab`  
> Task authority: `TASK_0045_PC_Golden_Interactive_Application_and_Flagship_Game_Prototype.md`  
> Previous review: `REVIEW_0045_V1_PC_Golden_Interactive_Application.md`  
> Decision: **FAIL / CONTINUE STAGE 004.5 REWORK**  
> Architecture redesign required: **NO**  
> Application/game framework direction: **ACCEPTED**

---

# 1. Executive Decision

The V1 rework is substantial and closes most of the previous review.

The following V1 blockers are now materially addressed:

```text
B1  clipped FILL semantics                       CLOSED
B2  Bilinear application effect                  CLOSED
B3  real Indexed8 + Palette draw path            CLOSED
B4  RGB565 Dither application use                CLOSED
B5  canonical Color Key                          CLOSED
B6  weak stress generators / AlphaStorm bug      MOSTLY CLOSED
B7  per-stress Immediate == Tile                 CLOSED
B8  X-Ray tests                                  PARTIAL
B9  launcher                                     CLOSED
B10 1000 rendered-frame stability                CLOSED
B11 100-frame feature coverage                   CLOSED
B12 deterministic capture regeneration           CLOSED
B13 mutation sensitivity                         CLOSED
B14 CLI scene/capture test                       CLOSED
B15 unconditional acceptance PASS generation     PARTIAL
B16 deterministic checkpoint                     CLOSED
```

The latest code is much closer to a valid system-level application Golden.

However, Stage 004.5 still does **not** meet its own mandatory acceptance contract.

The remaining blockers are no longer broad missing features. They are concentrated in:

```text
Architecture X-Ray correctness
strict proof/acceptance machinery
stress metric proof
one mandatory visual-feature proof
report consistency
```

Most importantly, the current live X-Ray measures its **own overlay**, so after X-Ray is enabled the WorkRef / Active Tile / Overdraw data becomes contaminated by the previous X-Ray grid and heatmap draws. That directly undermines one of the most important competition-facing features of this Stage.

Therefore:

# **REVIEW_0045_V2 = FAIL / CONTINUE STAGE 004.5 REWORK**

This should be a short closure rework, not another architecture rewrite.

---

# 2. V1 Rework Improvements Accepted

## A1 — FILL clipping is now implemented

The PC backend now performs deterministic rectangle intersection before FILL submission.

Both Immediate and Tile paths use the same helper.

`gpu2d_test_features` includes:

```text
partial clip
fully clipped fill
unclipped fill
Immediate == Tile clipped-fill equality
```

**Status: ACCEPTED**

One documentation issue remains: the test comment says `xmax/ymax inclusive`, while the actual Golden rectangle convention is half-open. See PWA-03.

---

## A2 — canonical Color Key is fixed

RGB565 magenta font background remains encoded as `0xF81F`, but the application now submits the canonical decoded RGB key:

```text
0x00FF00FF
```

A dedicated glyph test checks that magenta key pixels do not appear.

**Status: ACCEPTED**

---

## A3 — Bilinear is now a real application feature

The application now uses Bilinear for:

```text
Heavy enemy scaling
periodic shockwave
large glow particles
```

`gpu2d_test_features` explicitly checks for a recorded Bilinear Sprite and Immediate/Tile equality.

**Status: ACCEPTED**

---

## A4 — Dither is now exercised

Large glow/shockwave draws set:

```text
filter = Bilinear
dither = true
```

and counters/tests assert Dither presence.

**Status: ACCEPTED**

---

## A5 — Indexed8 + Palette is now genuinely submitted

A new `draw_text_pal()` path uses:

```text
INDEX8 font texture
palette lookup
palette flag
```

The normal HUD now uses this path.

**Status: ACCEPTED AS FEATURE PATH**

A visual-alpha quality issue remains; see PWA-01.

---

## A6 — stress generators are significantly stronger

The new deterministic 80-frame stress set reports:

```text
Sprite Storm    872 Sprite draws
Alpha Storm     432 Alpha draws
Bullet Hell     1016 live bullets
Scale Storm     480 scaled draws
Overdraw Storm  max_overdraw 284
```

These reported values exceed the frozen Stage thresholds.

The implementation also removed the old `Particle{}` zero-life Alpha-Storm mistake.

**Status: FUNCTIONAL GENERATORS ACCEPTED**

The Bullet machine assertion still uses the wrong metric expression; see R2-04.

---

## A7 — per-stress Immediate == Tile exists

Each of the five stress modes is executed through:

```text
Immediate
Tile32
```

and the selected deterministic final frame is compared byte-for-byte.

This closes the core H-T4 requirement.

**Status: ACCEPTED**

---

## A8 — Launcher is now implemented

Interactive startup now has a GPU-rendered launcher with:

```text
NEON SURVIVOR
GPU PLAYGROUND placeholder
ARCHITECTURE X-RAY / HELP
BENCHMARK / STRESS
```

**Status: ACCEPTED**

---

## A9 — 1000-frame system stability now includes rendering

`gpu2d_test_system` now performs, for 1000 frames:

```text
sim_step
→ render_frame
→ execute_frame(Tile32)
→ framebuffer access
```

This is the correct system-level form of the stability test.

**Status: ACCEPTED**

---

## A10 — deterministic capture verification exists

`verify_stage0045_captures.py` regenerates V1–V4 captures in temporary storage and compares SHA256 against checked references.

It does not overwrite reference files during normal verification.

**Status: ACCEPTED**

---

## A11 — mutation sensitivity now exists

`gpu2d_test_mutation` proves that:

```text
an additional 1×1 framebuffer write
```

and:

```text
a changed command color
```

break equality.

**Status: ACCEPTED**

---

## A12 — fixed simulation checkpoint exists

The test now freezes seed/frame values including:

```text
enemy count
bullet count
kills
score
hash
```

for seed 1234 / frame 60.

**Status: ACCEPTED**

---

# 3. R2-01 — CRITICAL: Live Architecture X-Ray Measures Its Own Overlay

Severity:

> **CRITICAL — XR-02 / XR-03 / XR-04 / competition demo correctness**

The interactive render loop intentionally uses:

```text
previous-frame RendererTelemetry
```

to draw the current X-Ray overlay, then executes the **entire scene + X-Ray overlay** as one command stream.

That means the telemetry produced at the end of an X-Ray frame contains:

```text
normal game draws
+
X-Ray Tile grid draws
+
WorkRef heat blocks
+
Overdraw markers
+
X-Ray text
```

The next X-Ray frame consumes that contaminated telemetry.

This is not a small one-frame lag problem.

The X-Ray grid draws:

```text
horizontal line across the full render target for every Tile row
vertical line across the full render target for every Tile column
```

Therefore after one X-Ray frame, the overlay itself touches essentially every Tile.

Consequences:

```text
tiles_active tends toward tiles_total
WorkRef counts are inflated by X-Ray commands
Overdraw includes debug-overlay overdraw
heatmap feeds on its own previous heatmap/grid
```

So after the first X-Ray frame, the display no longer cleanly visualizes the underlying game workload.

This is particularly serious because X-Ray is supposed to be the primary answer to:

> “How does the Tile-Based GPU architecture behave on this application?”

## Required correction

Keep X-Ray rendering through the Graphics API, but separate **measurement** from **overlay**.

Recommended frame flow:

```text
1. Record BASE scene only
2. Execute BASE scene
3. Snapshot BASE RendererTelemetry
4. Record X-Ray/HUD overlay-only command stream
5. Execute overlay over existing framebuffer
6. Present framebuffer
7. Preserve BASE telemetry as the authoritative X-Ray source
```

Important:

```text
Do NOT re-render the entire game a second time.
```

The second execution should contain only debug/HUD overlay commands.

Alternative solutions are acceptable if they guarantee:

```text
X-Ray overlay does not contribute to the telemetry it visualizes.
```

Add a regression test:

```text
base telemetry
→ enable X-Ray for multiple consecutive frames
→ authoritative base tile/workref/overdraw metrics remain independent of overlay command count
```

---

# 4. R2-02 — HIGH: G-T2 Is Still Weakened and Does Not Prove the Mandatory Matrix

TASK_0045 explicitly requires one fixed X-Ray test scene containing all three:

```text
one inactive Tile
one low-work Tile
one high-work Tile
```

The current test computes:

```text
zero
low
high
```

but asserts only:

```text
zero >= 1 OR low >= 1
low + high >= 1
```

This can pass with:

```text
zero = 0
low  = 15
high = 0
```

which violates G-T2.

There is an additional structural reason this is important:

the normal game always begins with a full-screen background FILL. Therefore raw WorkRef telemetry for normal gameplay naturally gives every Tile at least one draw.

A normal-game frame is therefore a poor test fixture for:

```text
inactive Tile
```

## Required correction

Create a dedicated sparse X-Ray fixture scene for verification, e.g.:

```text
clear / initialized framebuffer
+
small draw in one Tile
+
moderate overlap in one Tile
+
heavy overlap in one Tile
+
untouched regions
```

Then require exactly:

```cpp
CHECK(zero >= 1);
CHECK(low  >= 1);
CHECK(high >= 1);
```

Do not weaken the requirement.

For the competition game, Active Tile visualization may legitimately show all active when the background covers the full screen. The dedicated sparse architecture scene is what proves the feature.

---

# 5. R2-03 — HIGH: G-T4 Still Does Not Prove “Base Scene Before Overlay” Determinism

G-T4 requires:

> X-Ray ON/OFF may alter overlay pixels, but the scene render **before overlay** must remain deterministic.

The current test:

```text
hash existing framebuffer
build X-Ray render command stream
execute it
verify final hash changed
```

does not isolate a base-scene framebuffer from an X-Ray-overlay framebuffer.

`render_frame(... xray=true ...)` contains both:

```text
base scene
+
overlay
```

so the test cannot prove that the pre-overlay base scene is identical to X-Ray-off rendering.

## Required correction

After implementing the base/overlay split from R2-01, test:

```text
base frame with X-Ray OFF
==
base frame before overlay with X-Ray ON
```

byte-for-byte.

Then separately verify:

```text
overlay-on final framebuffer != base framebuffer
```

This closes G-T4 cleanly.

---

# 6. R2-04 — HIGH: Bullet-Hell Threshold Assertion Uses an Invalid Metric

The frozen threshold is:

```text
>= 1000 projectile entities OR projectile draws
```

The current machine test computes Bullet-Hell threshold metric as:

```text
live_bl + dc.sprites
```

This is invalid because `dc.sprites` already contains:

```text
bullet Sprite draws
other Sprite draws
HUD glyph Sprite draws
effects
```

It can double-count bullets and count unrelated Sprite draws.

The checked stress report states:

```text
Live Bullets = 1016
```

so the current implementation apparently satisfies the intended threshold.

But the machine gate does not enforce that fact directly.

## Required correction

Use exactly one valid metric:

```text
live_bl >= 1000
```

or introduce:

```text
bullet_draws
```

into `DrawCounts` and assert:

```text
bullet_draws >= 1000
```

Do not combine total Sprite count with live bullet count.

This is a proof defect, not a request to increase the current workload.

---

# 7. R2-05 — HIGH: Acceptance Checker Still Does Not Satisfy Its Mandatory Contract

The Stage task requires the checker to:

```text
own authoritative 56 IDs
reject missing IDs
reject duplicates
reject Mandatory != PASS
verify concrete evidence paths
validate CTest names
reject placeholder evidence
require exact START/END commit
verify one report row per Acceptance ID
```

The current checker has improved ID/status/path/Test-name checks, but its REPORT validation is still:

```text
for each ID:
    check "ID" occurs somewhere in report text
```

It does not parse the report table.

Therefore it does not verify:

```text
exactly 56 report rows
one row per ID
no duplicate rows
row status matches manifest
row implementation evidence matches manifest
row verification evidence matches manifest
row test name matches manifest
```

It also explicitly allows generic evidence strings beginning with:

```text
ctest ...
```

and therefore does not actually enforce the task's “reject placeholder evidence” rule.

Example:

```text
"ctest stage0045 includes golden_*"
```

is accepted as verification evidence without proving the full Stage-004 regression set.

## Required correction

Parse the Markdown matrix.

Machine-check:

```text
header
56 exact data rows
exact ordered ID set
unique IDs
PASS result
non-empty concrete implementation evidence
non-empty concrete verification evidence
declared CTest exists
```

Compare report rows against `STAGE_0045_ACCEPTANCE.json`.

Reject evidence patterns such as:

```text
ctest stage0045
covered by tests
application implementation
see report
similar test
```

unless paired with concrete test/path evidence.

---

# 8. R2-06 — HIGH: AUD-04 Stress Statistics Are Not Reproducibly Generated or Verified

Capture reproducibility is now good.

Stress-stat reproducibility is not yet equivalent.

`results/stage0045/stress/stress_stats.md` is a checked textual artifact containing specific numbers.

But no registered test:

```text
regenerates those statistics
and
compares the regenerated values to the checked artifact
```

`gpu2d_test_system` prints stress values, but the Markdown file remains manually synchronized.

This means AUD-04:

> captures **and stress outputs** checked/generated reproducibly

is only half closed.

## Required correction

Preferred approach:

```text
gpu2d_stress_stats --json <path>
```

or a Python wrapper that executes a deterministic stats-producing tool.

Freeze machine-readable output:

```text
results/stage0045/stress/stress_stats.json
```

with:

```text
scene
seed
frame
sprite_draws
alpha_draws
bullet_draws/live_bullets
scaled_draws
bilinear_draws
workrefs
tiles_active
max_overdraw
```

Then generate `stress_stats.md` from the JSON.

CTest should regenerate temporary JSON and compare semantic values.

---

# 9. R2-07 — MEDIUM/HIGH: FX-04 “Damage Flash / Color Mod” Evidence Is Still Weak

TASK F-06 requires:

```text
Damage Flash using Color Mod and/or Palette variation
```

The enemy hit path currently does:

```text
if (e.flash):
    color_mod = true
    mod = white
```

Multiplication by white is an identity operation.

So the enemy “flash” flag does not visibly modify its RGB output.

The player hit path uses a non-white tint and is visually meaningful, but the current feature tests do not explicitly prove this requirement.

The Report maps FX-04 to `gpu2d_test_features`, yet that test does not contain a dedicated visible Damage-Flash/Color-Mod assertion.

## Required correction

Either:

### Option A
Make enemy hit feedback visibly change pixels:

```text
red/cyan tint
palette variant
additive flash overlay
```

and test it.

or:

### Option B
Use the existing player damage tint as the authoritative F-06 path and add a deterministic test proving:

```text
damage flash command has color_mod enabled
mod != identity white
rendered framebuffer differs from non-flash baseline
Immediate == Tile
```

---

# 10. R2-08 — MEDIUM: F-T3 Technical HUD Telemetry Test Is Still Missing

TASK F-T3 requires:

> Technical HUD values must match backend telemetry for a fixed test frame.

The HUD implementation formats values directly from `RendererTelemetry`, which is good design.

But no current automated test freezes a frame and proves the displayed values correspond to:

```text
command_count
sprite_count
workref_count
tiles_active
max_workrefs_per_tile
max_overdraw
```

The existing system test proves telemetry exists, not that the HUD display is correct.

## Required correction

A practical test does not need OCR.

Expose a pure formatting helper such as:

```cpp
TechHudStrings make_tech_hud_strings(const RendererTelemetry&);
```

Test exact strings.

Then the renderer draws those strings through the GPU font path.

---

# 11. R2-09 — MEDIUM: BackendKind::Tile32 Does Not Always Mean 32×32

`GoldenBackend::set_backend()` currently does:

```text
Tile16 → tile_size = 16
Tile64 → tile_size = 64
Tile32 → keep current tile_size
Immediate → keep current tile_size
```

Therefore:

```text
Tile16
→ Immediate
→ Tile32
```

can still execute with 16×16 tiles.

Likewise a backend initialized with a non-32 profile can be switched to `BackendKind::Tile32` without forcing 32.

This is semantically inconsistent with the enum name and CLI option.

Stage 004.5 primarily requires Tile32, so current default paths are usually unaffected, but this should be fixed before treating runtime backend selection as authoritative.

## Required correction

Use:

```text
Tile16 → 16
Tile32 → 32
Tile64 → 64
Immediate → leave tile size unchanged or set only telemetry default
```

Add:

```text
Tile16 → Tile32
Tile64 → Tile32
```

tests.

---

# 12. R2-10 — MEDIUM: REPORT_0045 Contains Stale Statements

The latest Report still says:

```text
Stress unit-test thresholds are lower than demo targets for CI speed
```

and Technical Debt still says:

```text
Stronger stress thresholds in CTest when runtime budget allows
```

Those statements describe the pre-rework implementation.

The new `gpu2d_test_system` is intended to check the frozen Stage thresholds directly.

The report should not simultaneously claim:

```text
frozen thresholds PASS
```

and:

```text
unit thresholds are lower
```

## Required correction

Update the report to reflect the current implementation exactly.

Also state test evidence precisely:

```text
84/84 PASS — Agent-local Windows run
```

There are no published GitHub commit status checks on the reviewed implementation commit.

---

# 13. PWA-01 — Indexed8 Font Alpha Is Not Used

`make_font_index8()` defines:

```text
palette[0] alpha = 0
palette[1] alpha = 255
```

but `draw_text_pal()` uses `BlendMode::Copy` when caller alpha is 255.

COPY ignores source alpha for RGB565 output, so index-0 background pixels become black rather than transparent.

Because the HUD panels are black, this may be visually hidden.

This does not invalidate the Palette lookup feature itself, but it wastes the intended palette-alpha behavior.

Recommended:

```text
use StraightAlpha for the Indexed8 font even at global alpha 255
```

or use a real palette Color Key.

---

# 14. PWA-02 — Interactive X-Ray/HUD Telemetry Is One Frame Late

A one-frame telemetry delay is acceptable for a debug overlay if documented.

After R2-01 removes self-contamination, the one-frame lag can remain if necessary for PC Golden responsiveness.

It should be labeled as:

```text
debug telemetry: previous completed base frame
```

rather than implied to be current-frame hardware telemetry.

---

# 15. PWA-03 — Clip Comment Has the Wrong Boundary Terminology

The new feature test comments:

```text
xmax/ymax inclusive
```

but Golden clipping uses:

```text
[xmin, xmax)
[ymin, ymax)
```

half-open semantics.

The implementation behaves half-open.

Correct the comment/documentation before it propagates into FPGA-side code.

---

# 16. PWA-04 — PPM Capture RGB565 Expansion Differs from the Presenter

The Win32 presenter expands RGB565 by bit replication.

The PPM writer currently uses simple left shifts.

This creates a small display-vs-capture color difference.

Recommended:

reuse the same RGB565 expansion helper for:

```text
Win32 presentation
PPM capture
```

The RAW RGB565 references remain authoritative.

---

# 17. Evidence Assessment

The latest REPORT records:

```text
84 tests
71 Golden + 13 gpu2d
PASS
```

on the Agent-local Windows environment.

GitHub has no published commit-status checks for the reviewed implementation commit.

Therefore this review treats:

```text
84/84 PASS
```

as **Agent-reported local execution evidence**.

It was not independently executed by this reviewer.

Static review confirms that the new tests/files exist and materially improve the V1 implementation, but several tests are weaker than the exact TASK contract as described above.

---

# 18. Stage Gate

## Architecture / framework

```text
Common Graphics API                  ACCEPTED
Golden Immediate backend             ACCEPTED
Golden Tile backend                  ACCEPTED
Win32 presenter                      ACCEPTED
Headless mode                        ACCEPTED
NEON SURVIVOR game core              ACCEPTED
Launcher                             ACCEPTED
Bilinear / Dither / Palette paths    ACCEPTED
stress generators                    ACCEPTED
capture fixture mechanism            ACCEPTED
mutation test                        ACCEPTED
1000-frame rendered stability        ACCEPTED
```

## Remaining mandatory closure

```text
X-Ray telemetry isolation             FAIL
G-T2 exact inactive/low/high proof    FAIL
G-T4 pre-overlay determinism proof    FAIL
Bullet threshold direct assertion     FAIL
strict acceptance checker contract    FAIL
reproducible stress stats artifact    FAIL
F-06/FX-04 exact proof                INCOMPLETE
F-T3 HUD telemetry proof              INCOMPLETE
```

---

# 19. Final Decision

# **REVIEW_0045_V2 = FAIL / CONTINUE STAGE 004.5 REWORK**

Architecture redesign required:

> **NO**

Do not start over.

The project is now at a narrow closure point.

The next rework should only address the findings in this review.

---

# 20. Required Rework Order

1. Split base rendering from X-Ray overlay rendering so debug overlays never contaminate authoritative telemetry.
2. Add a sparse fixed X-Ray fixture and enforce `inactive >=1`, `low >=1`, `high >=1`.
3. Close G-T4 with exact base-frame OFF/ON pre-overlay equality.
4. Change Bullet-Hell threshold assertion to direct `live_bullets` or `bullet_draws`.
5. Make Stage0045 acceptance checker parse and compare the exact 56-row report matrix.
6. Reject generic placeholder evidence in the checker.
7. Generate/verify machine-readable stress statistics reproducibly.
8. Add a real F-06 Damage-Flash/Color-Mod proof.
9. Add pure technical-HUD string formatting and fixed telemetry test.
10. Fix `Tile32` backend selection to force 32×32.
11. Remove stale stress-threshold statements from REPORT_0045.
12. Apply the PWA cleanup items if low-risk.
13. Re-run full Golden + application suite.
14. Update exact END_COMMIT and submit REVIEW_0045_V3.

Only after these closure items pass should Stage 004.5 be promoted to final PASS.
