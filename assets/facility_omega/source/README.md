# FACILITY-Ω First Asset Pack V0.1

This package is the **source-art handoff** for Application 2.

## Important

The PNGs under `source_sheets/` are **not production-ready sprite atlases**. They are generated source sheets and may contain:
- labels or layout text,
- uneven spacing,
- large transparent margins,
- sprites at presentation scale rather than final runtime scale,
- anti-aliased/soft glow edges,
- multiple variants grouped together.

The local Agent is expected to process them into clean project assets.

## Agent processing expectations

1. Preserve every source image unchanged under a `source/` or equivalent directory.
2. Extract only the sprite/art regions needed by the game; never include labels in runtime atlases.
3. Normalize transparent bounds and define anchors/pivots.
4. Use nearest-neighbor when reducing pixel-art body sprites unless an explicit art review says otherwise.
5. Keep FX soft alpha where appropriate.
6. Produce metadata for every runtime sprite.
7. Convert/build runtime resources offline; do not require PNG decoding on the final RISC-V target.
8. Prefer:
   - RGB565 for opaque floor/background/props,
   - ARGB8888 for alpha FX,
   - Indexed8 + palette for bitmap font / selected UI or recolorable assets.
9. Generate deterministic asset manifests/hashes.
10. Do not modify the authoritative source sheets to make tests pass.

## Source sheets

- `player_source.png`
- `enemies_source.png`
- `weapons_pickups_source.png`
- `fx_source.png`
- `environment_source.png`
- `ui_source.png`

## Reference

- `reference/concept_art_v0.1.png`
- `docs/Application_2_Survivor_like_Game_Concept_and_Art_Direction_V0.1.md`

## Scope

This is the first-stage asset pack. It is sufficient to begin:
- player/camera,
- 4096×4096 tile-map world,
- first 3 enemy types,
- Pulse Shot,
- XP pickup,
- basic HUD,
- first playable vertical slice.

Boss/final polish assets are not required for the first implementation task.
