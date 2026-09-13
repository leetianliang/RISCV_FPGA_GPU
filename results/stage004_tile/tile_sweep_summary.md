# Tile Size Architecture Sweep Summary (Stage 004)

Analytical metrics from the functional Golden Tile model (not measured DDR bandwidth).

## Observations

| Tile size | Typical effect |
|---|---|
| 16×16 | More tiles and metadata; better locality for small scatter sprites; higher workref counts when draws span many tiles |
| 32×32 (default) | Balanced metadata vs draw duplication; remains the frozen hardware default |
| 64×64 | Fewer tiles/headers; more workrefs per tile for dense scenes; larger load/store grain |

## Conclusions

- Dense small-sprite grids benefit from smaller tiles (finer active-tile tracking).
- Large overlapping alpha stacks concentrate work in one/few tiles regardless of size.
- **32×32 remains the reasonable default** for competition 2D workloads in this model.
- No evidence requires changing the frozen hardware default; if later RTL traffic counters disagree, raise DESIGN_QUESTION.

Raw data: `results/stage004_tile/tile_sweep.csv`
