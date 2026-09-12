# RISC-V–FPGA General-Purpose 2D GPU

A command-driven, Tile-Based embedded 2D GPU using a RISC-V + FPGA heterogeneous architecture for sprite-heavy games and embedded GUI/HMI workloads.

**Current phase: Specification complete / implementation initialization**

## Major Architecture Concepts

Planned / designed (not yet implemented unless noted):

- **64B Command ISA** — fixed-width command words for fill, blit, and related 2D operations.
- **RISC-V software + FPGA graphics accelerator** — host CPU issues commands; FPGA executes the pixel pipeline.
- **Command Ring** — producer/consumer ring between software and hardware.
- **Shared Texture / Pixel pipeline** — unified path for sampling and blending.
- **Immediate + Tile render paths** — low-latency immediate mode and tile-based deferred path.
- **Bit-accurate PC Golden Model** — software reference for RTL co-simulation and frame compare.
- **Performance counters / X-Ray** — on-chip instrumentation for bandwidth and stall analysis.
- **Future Affine / 2.5D extension** — planned beyond the baseline 2D feature set.

No functional GPU implementation is present yet; this repository currently holds specifications and a project baseline skeleton.

## Directory Guide

| Path | Purpose |
|---|---|
| `docs/` | Specifications, tasks, reports, reviews, decisions |
| `model/golden/` | Bit-accurate PC Golden GPU model (C++) |
| `model/architecture/` | Tile / cache / bandwidth / lane architectural models |
| `rtl/` | SystemVerilog RTL sources by subsystem |
| `sim/` | Unit, subsystem, random, and full-system simulation |
| `software/` | Driver, graphics libraries, engine, benchmarks, apps |
| `tools/` | Asset converter, command generator, frame compare, parsers |
| `board/` | FPGA board projects and constraints |
| `results/` | Curated results and local run artifacts |
| `spec/` | Machine-readable single-source constants (future) |
| `scripts/` | Preflight and baseline check helpers |

## Development Workflow

```text
Spec
→ Task
→ Agent Implementation
→ Automated Tests
→ Implementation Report
→ Architecture Review
→ Gate Decision
```

Authoritative specifications live under `docs/`. See `docs/README.md` for the document index and `docs/SPEC_STATUS.md` for Gate A status. Operating rules for agents are in `AGENTS.md`.

## Getting Started (Baseline)

```bash
python scripts/preflight.py
python scripts/check_baseline.py
cmake -S . -B build/task000
cmake --build build/task000
```

These commands verify repository structure and CMake configuration only. They do not build a GPU.
