# spec/

Intended for **machine-readable single-source** constants and specification metadata.

Future files may generate:

- C/C++ headers.
- SystemVerilog packages.
- Documentation tables.

## TASK_000 Status

- No ISA/registry YAML is defined yet (`gpu_isa.yaml`, `gpu_regs.yaml`, generator scripts are **out of scope** for TASK_000).
- Do **not** manually duplicate frozen specifications from `docs/` here.
- The exact machine-readable schema will be reviewed in a dedicated later task.

## generated/

Output directory for future generated headers/packages. Keep empty until generation infrastructure exists.
