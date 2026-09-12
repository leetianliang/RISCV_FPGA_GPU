# AGENTS.md — Project Operating Rules

Project: RISC-V–FPGA General-Purpose 2D GPU  
Root: `FPGA_2D_GPU/`

These rules bind every local agent (and any automated tooling) that modifies this repository.

---

## 1. Authority Hierarchy

When implementing this project, use this hierarchy:

```text
Requirements
  ↓
System Architecture
  ↓
Command ISA
  ↓
Internal Interface Specification
  ↓
Pixel Format & Arithmetic Specification
  ↓
Register/Memory Map
  ↓
Verification Plan
  ↓
Stage Task Document
  ↓
Implementation
```

When rules overlap:

- Command binary layout → Command ISA is authoritative.
- Pixel numerical behavior → Pixel Arithmetic Specification is authoritative.
- RTL module communication → Internal Interface Specification is authoritative.
- MMIO/address behavior → Register/Memory Map is authoritative.
- Verification acceptance → Verification Plan is authoritative.

Specifications under `docs/` are read-only for infrastructure tasks unless the task document explicitly authorizes edits. Never silently rewrite architecture, ISA, arithmetic, register, or interface semantics to make implementation easier.

If a specification conflict or missing required specification is found:

```text
BLOCKER:
...
```

or:

```text
DESIGN_QUESTION:
...
```

Stop the affected work and record the issue in the implementation report. Do not invent a replacement rule.

---

## 2. Forbidden Autonomous Changes

The local agent must not autonomously alter:

- Command size.
- Opcode encoding.
- Pixel rounding.
- Alpha equations.
- RGB565 conversion.
- Q16.16 semantics.
- Tile draw ordering.
- Immediate/Tile compatibility semantics.
- Internal interface payload semantics.
- Register offsets.
- Fault semantics.
- Capability meanings.

If a change appears necessary, write a `DESIGN_QUESTION:` or `BLOCKER:` and stop the affected implementation.

---

## 3. Test Integrity

> A failing test may only be changed if the authoritative specification changed. Do not modify expected results merely to match the implementation.

Also:

- Never delete a valid failing regression to make CI pass.
- Every fixed bug should receive a regression test.
- Do not weaken assertions without review.

---

## 4. Scope Discipline

The agent may make local implementation decisions only when they do not alter externally visible specified behavior.

**Allowed local decisions (examples):**

- Function decomposition.
- Private helper naming.
- Internal container choice.
- Local C++ class organization.
- RTL state encoding.
- FIFO implementation details when not frozen.

**Requires review (examples):**

- New ISA flags.
- Changed arithmetic.
- Changed Tile semantics.
- Changed register fields.
- Changed public interfaces.

---

## 5. Reporting

At the end of every task, the agent must create the requested report and include:

- Files changed.
- Tests run.
- Exact commands run.
- Test results.
- Warnings.
- Known limitations.
- Blockers.
- Design questions.
- Suggested next steps.

Reports go under `docs/reports/` using the naming convention `REPORT_<ID>_<Title>.md`. Task documents live under `docs/tasks/`.

---

## 6. Scope of Implementation Work

Unless a task document explicitly expands scope, agents must not:

- Implement Golden rendering algorithms beyond the authorized task.
- Implement FILL/BLIT/RTL/FPGA board logic outside the authorized task.
- Add third-party dependencies without review.
- Modify frozen specification documents.
- Push to remotes or rewrite Git history.

Functional implementation work requires:

1. All required specifications present (`docs/SPEC_STATUS.md` Gate A READY), and
2. A task document authorizing the work, and
3. Prior stage review approval when the project flow requires it.
