# DECISION — Golden Immediate Raster Bounds (V0.1)

Status: **Recorded for Stage 003R closure**  
Source authority: Command ISA V0.1 Clip / BLIT parameter rules; TASK_003R R11.

## Rule (Immediate Golden; must match future Tile)

1. **Explicit Clip (`CLIP_EN=1`)**  
   Effective raster = destination rect ∩ clip rect (half-open) ∩ registered render-target bounds.

2. **No Clip (`CLIP_EN=0`)**  
   Effective raster = destination rect ∩ registered render-target bounds.  
   Pixels outside the target allocation are **not** written; the command still succeeds if the intersection is non-empty. Fully outside → no-op success.

3. **`H_EXT_VALID` alone does not enable Clip.** Clip is solely `CLIP_EN`.

4. **UV mapping origin is always the full destination rectangle origin** (`DST_X/Y`), not the clipped raster origin.

5. Mid-command memory failure after partial writes: **no rollback** (synchronous Golden). Pre-raster validation failures must not modify the framebuffer.

## Rationale

ISA defines Clip via extension and half-open rectangles; it does not define a separate “hard fault on any OOB pixel” Immediate rule. Intersecting with target bounds keeps Golden/RTL comparable without inventing a new fault for ordinary scissored placement.

Future Tile mode must use the same effective-raster definition (also intersect tile rect).
