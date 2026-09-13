#!/usr/bin/env python3
"""Check Stage-004 acceptance manifest. Stdlib only."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "tasks" / "STAGE_004_ACCEPTANCE.json"
REPORT = ROOT / "docs" / "reports" / "REPORT_004_Golden_Tile_Renderer_and_Binning.md"


def main() -> int:
    if not MANIFEST.is_file():
        print(f"missing {MANIFEST}", file=sys.stderr)
        return 1
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    acc = data.get("acceptance", [])
    bad = 0
    ids = set()
    for item in acc:
        iid = item.get("id", "")
        ids.add(iid)
        status = item.get("status", "")
        if item.get("mandatory", True) and status != "PASS":
            print(f"[FAIL] {iid} status={status}")
            bad += 1
        for key in ("implementation_evidence", "verification_evidence"):
            if item.get("mandatory", True) and not item.get(key):
                print(f"[FAIL] {iid} empty {key}")
                bad += 1
        if item.get("mandatory", True) and not item.get("test_name"):
            print(f"[FAIL] {iid} empty test_name")
            bad += 1
    if not REPORT.is_file():
        print("[FAIL] missing REPORT_004")
        bad += 1
    else:
        text = REPORT.read_text(encoding="utf-8", errors="replace")
        for iid in ids:
            if iid not in text:
                print(f"[FAIL] REPORT_004 missing id {iid}")
                bad += 1
    if bad:
        print(f"stage004 acceptance: FAIL ({bad})")
        return 1
    print(f"stage004 acceptance: PASS ({len(ids)} ids)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
