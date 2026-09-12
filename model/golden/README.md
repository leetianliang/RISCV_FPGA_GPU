# model/golden

Bit-accurate PC Golden GPU model (C++20).

## Stage status

Stage 002: **Golden 2D Core** — FILL, BLIT, RGB565/ARGB8888/XRGB8888 source, Color Key, Global/Per-Pixel Alpha, Straight Alpha, Additive, RGB565 destination, multi-command streams.

Not started: BLIT_EXT, scaling, bilinear, palette, Tile, RTL.

## Layout

```text
model/golden/
├── CMakeLists.txt
├── include/golden/   Public headers
├── src/              Implementation
├── tests/
│   ├── unit/
│   ├── directed/
│   ├── random/
│   └── frames/       Immutable binary fixtures (opt-in regenerate)
└── tools/golden_cli.cpp
```

## Build / test

From repository root:

```bash
cmake -S . -B build/stage002
cmake --build build/stage002
ctest --test-dir build/stage002 --output-on-failure
```

## Fixture policy

- Checked-in files under `tests/frames/` are **reference oracles**.
- Normal `cmake --build` / `ctest` **must not** rewrite them.
- Opt-in regeneration:

```bash
cmake --build build/stage002 --target golden_regenerate_fixtures
```

- Current-run outputs go under the build tree only.

## CLI

```text
golden_cli run-stream --cmd C.bin --initial-fb I.raw ... --out O.raw
golden_cli generate-fill-basic <dir>
golden_cli generate-stage002-fixtures <frames_root>
```

Command binaries are explicit little-endian (`serialize_cmd_le` / `deserialize_cmd_le`).

## Dependencies

Standard library only for Golden build/test. No SDL/OpenGL/third-party test frameworks.
