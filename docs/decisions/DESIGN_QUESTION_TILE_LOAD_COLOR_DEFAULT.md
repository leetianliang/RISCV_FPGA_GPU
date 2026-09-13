# DECISION — Tile LOAD_COLOR_DEFAULT / DONT_LOAD / CLEAR Matrix

Status: **FROZEN — REVIEW_004_V5**

## Frozen V0.1 Compatibility Rule

```text
Initialization priority:

1. TILE_CLEAR_COLOR = 1
   → initialize valid Tile pixels from CLEAR_COLOR
     through normal DST_FORMAT conversion.

2. Else TILE_DONT_LOAD_COLOR = 1:
      LOAD_COLOR_DEFAULT = 1
      → initialize valid Tile pixels from canonical 0x00000000
        through normal DST_FORMAT conversion.

      LOAD_COLOR_DEFAULT = 0
      → UNSUPPORTED_FEATURE in the V0.1 compatibility profile.

3. Else
   → load valid Tile pixels from the framebuffer.

STORE_COLOR = 1
→ store final valid Tile pixels to framebuffer.

STORE_COLOR = 0
→ do not update framebuffer on Tile retire.
```

## Default color consequences

```text
RGB565 default   → black 0x0000
ARGB8888 default → 0x00000000
XRGB8888 default → 0xFF000000
```

`LOAD_COLOR_DEFAULT=1` has no effect when a real framebuffer load occurs.

## Implementation

`model/golden/src/tile_binner.cpp` `execute_tile_frame()` implements this matrix.

Synchronize into a later controlled ISA/System Architecture revision.
