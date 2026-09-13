# Tile Size Architecture Sweep Summary (Stage 004)

Source: `results/stage004_tile/tile_sweep.csv` produced by
`model/architecture/tile_model/run_tile_sweep.py` invoking `golden_tile_sweep`
(W1–W6 × tile 16/32/64, 18 rows, functional Golden model).

Workloads (distinct generators in `model/golden/tools/tile_sweep_tool.cpp`):

| ID | Name | Behavior |
|---|---|---|
| W1 | W1_sprite_grid | 6×6 BLIT grid |
| W2 | W2_high_overdraw | dense stack of fills |
| W3 | W3_alpha_storm | overlapping straight-alpha fills |
| W4 | W4_scaled_sprites | BLIT_EXT 4→16 nearest |
| W5 | W5_edge_scatter | edge + scatter 1–3px fills |
| W6 | W6_mixed_scene | fill + blit + scale + alpha |

## Observations (from CSV)

- Smaller tiles increase active-tile count for grids/scatter.
- High-overdraw stacks concentrate workrefs in few tiles regardless of size.
- Scaled sprites produce workrefs spanning multiple tiles at 16×16.
- Every (workload, tile) row has `tiles_total` equal to the exact grid cover
  (64×64 surface: 16 / 4 / 1 tiles) and `tiles_active > 0`.

## Conclusion

32×32 remains a reasonable default for competition 2D; no frozen hardware
default change is required from this functional model.
