# Specification Status

Baseline tracker for required project specifications.  
Updated by infrastructure tasks only; do not invent status changes without evidence.

## 1. Expected Specs

| Required spec | Filename | Status |
|---|---|---|
| Requirements | `RISC-V_FPGA_2D_GPU_Requirements_V1.0.md` | FOUND |
| System Architecture | `RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md` | FOUND |
| Command ISA | `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md` | FOUND |
| Internal Interface | `RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md` | FOUND |
| Pixel Arithmetic | `RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md` | FOUND |
| Register/Memory Map | `RISC-V_FPGA_2D_GPU_Register_Map_Memory_Map_V0.1.md` | FOUND |
| Verification Plan | `RISC-V_FPGA_2D_GPU_Verification_Plan_V0.1.md` | FOUND |
| Project Development Flow | `RISC-V_FPGA_2D_GPU_Project_Development_Flow_V1.0.md` | FOUND |
| Golden Software Architecture | `RISC-V_FPGA_2D_GPU_Golden_GPU_Software_Architecture_V0.1.md` | FOUND |

Status values: `FOUND` / `MISSING` / `SUPERSEDED` (SUPERSEDED not used for TASK_000).

## 2. Known Cross-Document Note

> Pixel Arithmetic V0.1 defines Compatibility Tile Mode such that every logical render-target write is quantized according to `DST_FORMAT` before being stored as canonical RGBA in the Tile Buffer. This clarification should eventually be synchronized into a future System Architecture revision; do not autonomously edit System Architecture in TASK_000.

This note is recorded only. No revision is performed in TASK_000.

## 3. Gate A State

```text
Gate A: READY FOR IMPLEMENTATION
```

All nine required specification files are present in `docs/`. Formal Gate A acceptance remains with the reviewer per the project development flow.

Repository state: `SPEC_COMPLETE` (all required specs found).
