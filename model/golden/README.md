# model/golden

Bit-accurate PC Golden GPU model (C++). Implementation begins in a later task (Golden G0/G1 bootstrap).

## Layout

```text
model/golden/
├── CMakeLists.txt
├── include/     Public headers
├── src/         Implementation
├── tests/
│   ├── unit/
│   ├── directed/
│   ├── random/
│   └── frames/
└── tools/       Golden-side helper tools
```

## Build

Configured from the repository root:

```bash
cmake -S . -B build/task000
```

The `BUILD_GOLDEN` option (default ON) adds this subdirectory. No compiled targets exist until Golden implementation lands.
