# Documentation Index

Specifications under this directory are authoritative for their areas. Do not modify them without an approved architecture decision and an authorized task.

| Document | Version | Role | Authority | Status |
|---|---|---|---|---|
| `RISC-V_FPGA_2D_GPU_Requirements_V1.0.md` | V1.0 | Project requirements and top-level goals | Highest-level product/system intent | FOUND |
| `RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md` | V1.0 | System architecture and major blocks | Overall architecture and block partitioning | FOUND |
| `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md` | V0.1 | 64B Command ISA | Command binary layout, opcodes, encodings | FOUND |
| `RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md` | V0.1 | Internal module interfaces | RTL module communication payloads | FOUND |
| `RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md` | V0.1 | Pixel formats and arithmetic | Pixel numerical behavior, rounding, blending, Q16.16 | FOUND |
| `RISC-V_FPGA_2D_GPU_Register_Map_Memory_Map_V0.1.md` | V0.1 | Register and memory map | MMIO offsets, address behavior | FOUND |
| `RISC-V_FPGA_2D_GPU_Verification_Plan_V0.1.md` | V0.1 | Verification strategy and acceptance | Verification acceptance criteria | FOUND |
| `RISC-V_FPGA_2D_GPU_Project_Development_Flow_V1.0.md` | V1.0 | Development flow and gates | Stage gates and review process | FOUND |
| `RISC-V_FPGA_2D_GPU_Golden_GPU_Software_Architecture_V0.1.md` | V0.1 | Golden model software architecture | Golden model structure and organization | FOUND |

Related non-specification directories:

- `tasks/` — Architect-issued implementation tasks.
- `reports/` — Agent execution reports.
- `reviews/` — Reviewer findings and gate decisions.
- `decisions/` — Approved architecture/design decisions.

See also `SPEC_STATUS.md` for Gate A readiness and `PROJECT_STATUS.md` for the current stage.
