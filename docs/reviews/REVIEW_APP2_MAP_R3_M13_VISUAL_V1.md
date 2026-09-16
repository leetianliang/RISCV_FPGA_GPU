# REVIEW_APP2_MAP_R3_M13_VISUAL_V1

## Review target

- `hero_overview.png` — 1024×1024 Hero Area overview
- `gameplay_180.png` — 640×360 in-game capture
- Scope: M13 visual quality gate only
- Technical MAP R3 status remains `PASS WITH ACTIONS`

# Formal decision

## M13 VISUAL GATE: **FAIL / HOLD — VISUAL POLISH REQUIRED**

The screenshots prove that the R3 architectural rework is functioning, but the rendered scene is not yet at the intended competition-demo visual quality.

This is **not** a request to redesign the map system or collision system again.

Keep:

- current Hero Area geometry foundation;
- current collision layer;
- current open-space ratio;
- current player/enemy movement;
- current GPU API and renderer semantics.

Only perform a focused **R3V2 visual-composition pass**.

---

# What is already good

1. The old visible 32×32 floor-tile mosaic is gone.
2. The scene reads as one coherent industrial facility rather than a repeated tile test.
3. Player, enemy and machinery sprites are visually much stronger than the former placeholder-map level.
4. The dark blue-gray + cyan + yellow/orange palette is coherent.
5. Combat space remains open and readable.
6. HUD is compact and readable at 640×360.
7. Large props are sparse enough not to turn the game into a maze.

These parts should be preserved.

---

# Main visual problems

## 1. The scene is too empty

The overview has very large uninterrupted dark floor areas.

In the gameplay screenshot, most of the 640×360 viewport is essentially:

`dark floor + a few seams + one wall marking`.

The result still reads closer to an engineering test arena than a finished game environment.

The fix is **not** to scatter many collision props.

Add non-collidable environmental information:

- cable runs;
- recessed floor panels;
- oil / wear marks;
- drains;
- broken floor plates;
- warning arrows;
- service numbers;
- small debris clusters;
- floor lights;
- inactive conduit traces;
- subtle stains / scorch marks.

Most of these should be decals and therefore not affect movement.

---

## 2. Macro rectangular grid is still visually obvious

The 32×32 tiling problem is solved, but the overview now exposes another pattern:

- long perfectly straight horizontal seams;
- long perfectly straight vertical seams;
- large rectangular floor fields.

They create a new large-scale grid feeling.

Keep some industrial expansion joints, but break the regularity.

Recommended:

- terminate some seams before the full room width;
- offset adjacent seams;
- use T-junctions rather than full cross-map lines;
- use 2–3 widths / brightness levels;
- let props, door frames or floor panels interrupt them.

The scene should look authored, not partitioned by a coordinate grid.

---

## 3. Equipment groups look like isolated display platforms

Several equipment clusters are placed on dark rectangular plinths with a bright trim.

They look somewhat like “asset showcase cards” placed on the floor.

Integrate each equipment group into the environment using:

- cable/pipe connection to walls or floor;
- small shadow / base plate;
- nearby warning decal;
- service light;
- one or two associated small props;
- a stronger relationship to the corresponding zone.

The equipment should look installed in the facility, not pasted onto a rectangular island.

---

## 4. Wall language is still too primitive

The perimeter walls are readable, but visually they are still mostly:

`large dark rectangle + border`.

For final competition presentation, add a small wall-decoration vocabulary:

- wall edge sprite;
- outer corner;
- inner corner;
- doorway frame;
- support column;
- pipe strip;
- warning stripe cap;
- wall-mounted light.

Collision should remain the existing simple AABB.

Do not derive collision from the decorative wall pixels.

---

## 5. Zone identity is too weak

The current four named areas exist structurally, but their visual identities are not strong enough.

A player should recognize the zone from the screen without reading a debug label.

Recommended visual grammar:

### Central Power Test Hall
- broad open floor;
- A-3 / test markings;
- cyan power lines;
- large empty combat area.

### Maintenance
- pipes;
- service markings;
- yellow warning accents;
- tool / repair props.

### Storage
- crates;
- pallet / floor loading marks;
- warmer amber accents;
- sparse debris.

### Power Lab
- cyan emissive equipment;
- energy canisters;
- brighter light pools;
- more technical floor decals.

No new gameplay semantics are needed.

---

## 6. Gameplay frame lacks visual activity

`gameplay_180.png` is technically clear, but as a showcase frame it is visually quiet:

- one player;
- one enemy;
- tiny projectiles;
- almost no effects;
- very little environmental contrast.

This is acceptable as a correctness capture, but not as the final hero screenshot.

For the competition capture, add a controlled scene with:

- 10–30 enemies;
- multiple bullets;
- one alpha field / glow;
- one additive effect;
- XP drops;
- one larger enemy;
- richer surrounding environment.

The goal is not “maximum stress” here. It is a visually convincing middle-density gameplay shot.

---

# R3V2 recommended scope

Do not add new game mechanics.

Only implement:

1. 2–4 quiet floor-material variants;
2. non-colliding large floor decals;
3. broken / offset macro seams;
4. wall edge / corner / doorway decoration;
5. environment integration around the four equipment groups;
6. stronger visual identity for the four zones;
7. one deliberately staged gameplay capture with medium combat density;
8. updated `hero_overview.png`, `map_view_0/1/2.png`, `gameplay_180.png`.

Optional:
- soft local light pools;
- a small number of steam/spark effects;
- very subtle floor wear variation.

---

# Do not change

- collision geometry unless a visual change exposes an actual mismatch;
- player collision radius;
- enemy collision approach;
- map logical size;
- GPU semantics;
- renderer architecture;
- Tile32 / Immediate behavior;
- game progression systems;
- weapons / bosses / upgrade logic.

---

# Acceptance criteria for the next visual pass

M13 can pass when the following are visible in the actual screenshots:

- no obvious 32×32 floor tiling;
- no obvious large regular grid dominating the room;
- center combat space still remains open;
- no viewport looks mostly like an empty flat rectangle;
- each major zone has a recognizable visual identity;
- equipment reads as part of the facility rather than isolated showcase objects;
- walls have enough decorative language to look game-like;
- gameplay capture shows the map, character and GPU effects together;
- overall scene still remains readable at 640×360.

---

# Final status

**MAP R3 technical foundation: PASS WITH ACTIONS**

**M13 visual quality: FAIL / HOLD**

**Required next stage: R3V2 VISUAL POLISH ONLY**

The current screenshots are a solid technical map prototype, but they are not yet the final competition-quality environment.
