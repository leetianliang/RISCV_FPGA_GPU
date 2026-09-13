# DESIGN_QUESTION_TILE_ALIGNMENT — TILE_FRAME array base alignment

Status: **IMPLEMENTED (local interpretation for Golden; RTL must match)**  
Date: Stage 004 V8 rework  
Authority: Command ISA V0.1 §2.2 + §38–§41

## Frozen by ISA V0.1

```text
Draw Descriptor Array Base: 64B aligned
Draw Descriptor entry: 64B
```

`DRAW_DESC_BASE % 64 != 0` → `FAULT_BAD_ALIGNMENT` before any descriptor access.

## Local Golden interpretation (not 64B-frozen by ISA)

Command ISA freezes Tile Header size (16B) and WorkRef size (4B) but does **not**
explicitly freeze 64B alignment for `TILE_HEADER_BASE` / `WORK_LIST_BASE`.

Golden Stage 004 therefore enforces natural structure alignment:

```text
TILE_HEADER_BASE % 16 == 0
WORK_LIST_BASE   % 4  == 0
```

Misalignment returns the same exact fault: `FAULT_BAD_ALIGNMENT`.

## RTL requirement

Stage 005 RTL must use the same three checks and the same fault code. Changing
these alignments requires a new DESIGN_QUESTION and ISA amendment — do not
silently relax or tighten them.
