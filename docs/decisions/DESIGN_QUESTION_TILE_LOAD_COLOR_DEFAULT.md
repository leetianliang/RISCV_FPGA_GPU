# DESIGN_QUESTION — Tile LOAD_COLOR_DEFAULT / DONT_LOAD / CLEAR Matrix

Status: **OPEN — awaiting reviewer freeze**

## Context

`RT_STATE.LOAD_COLOR_DEFAULT` is defined in Command ISA V0.1 but its default color value and interaction with `TILE_DONT_LOAD_COLOR` / `TILE_CLEAR_COLOR` are not fully specified in the frozen text.

## Stage-004 working rule (provisional, not architecturally frozen)

```text
CLEAR_COLOR=1           → fill tile with CLEAR_COLOR
CLEAR_COLOR=0 & DONT_LOAD=1
  LOAD_COLOR_DEFAULT=1  → zero-fill (RGBA 0)
  LOAD_COLOR_DEFAULT=0  → FAULT_UNSUPPORTED_FEATURE
CLEAR_COLOR=0 & DONT_LOAD=0 → load from framebuffer
```

Precedence assumed: CLEAR > DONT_LOAD > LOAD_COLOR_DEFAULT.

## Required reviewer action

Freeze the full matrix and the actual default color value (zero vs CLEAR_COLOR vs other) before Stage 004 final Gate, or authorize this provisional rule as Golden/RTL authority.
