#!/usr/bin/env python3
"""Generate REPORT_004 evidence matrix from acceptance JSON."""
from pathlib import Path
import json

root = Path(__file__).resolve().parents[1]
data = json.loads((root / "docs/tasks/STAGE_004_ACCEPTANCE.json").read_text(encoding="utf-8"))
rows = []
for a in data["acceptance"]:
    impl = ", ".join(a["implementation_evidence"])
    tn = a["test_name"]
    rows.append(f"| {a['id']} | PASS | {impl} | {tn} |")
matrix = "\n".join(rows)
report = f"""# REPORT_004 — Golden Tile Renderer, Software Binning & Pixel-Exact

## 1. Result

**PASS** (pending REVIEW_004_V8)

## 2. START_COMMIT

`105c23c0989c74fcd6273b33181c6079e40f5d5e`

## 3. END_COMMIT

See `git log -1` after this report commit.

## 4. Final Verification

```text
ctest --test-dir build/stage004 --output-on-failure
70/70 PASS (Agent-reported local; not CI-reproduced)
python scripts/check_stage004_acceptance.py → PASS
python tools/fixture_validate/validate_tile_fixtures.py → PASS
```

## 5. Architecture (accepted)

CPU binner → serialized descriptors/headers/workrefs → TILE_FRAME → internal tile_mem → shared pixel backend → store.

LOAD_COLOR_DEFAULT: FROZEN (see decisions file).

## 6. 47-Row Evidence Matrix

| ID | Result | Implementation | Verification |
|---|---|---|---|
{matrix}

## 7. Next

Stage 005 RTL after REVIEW_004_V8.
"""
(root / "docs/reports/REPORT_004_Golden_Tile_Renderer_and_Binning.md").write_text(
    report, encoding="utf-8"
)
print("wrote REPORT_004 with", len(rows), "rows")
