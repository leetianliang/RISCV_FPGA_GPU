#!/usr/bin/env python3
"""Strict Stage-0045 acceptance checker. Stdlib only."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "tasks" / "STAGE_0045_ACCEPTANCE.json"
REPORT = ROOT / "docs" / "reports" / "REPORT_0045_PC_Golden_Interactive_Application.md"
BUILD = ROOT / "build" / "stage0045"

GROUPS = [("HOST", 5), ("API", 6), ("BACK", 6), ("SIM", 5), ("GAME", 6),
          ("FX", 6), ("XR", 5), ("STR", 6), ("SYS", 7), ("AUD", 4)]
AUTHORITATIVE_IDS = [f"{p}-{i:02d}" for p, n in GROUPS for i in range(1, n + 1)]


def ctest_names(build: Path) -> set[str]:
    import subprocess
    r = subprocess.run(["ctest", "--test-dir", str(build), "-N"],
                       capture_output=True, text=True)
    names = set()
    for line in r.stdout.splitlines():
        line = line.strip()
        if line.startswith("Test") and ":" in line and "#" in line:
            names.add(line.split(":", 1)[1].strip())
    return names


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--allow-no-build", action="store_true")
    args = ap.parse_args()

    if not MANIFEST.is_file():
        print(f"missing {MANIFEST}", file=sys.stderr)
        return 1
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    acc = data.get("acceptance", [])

    ctests: set[str] = set()
    if BUILD.is_dir():
        ctests = ctest_names(BUILD)
        if not ctests:
            if args.allow_no_build:
                print("[WARN] empty ctest -N")
            else:
                print("[FAIL] ctest -N empty")
                return 1
    else:
        if args.allow_no_build:
            print("[WARN] missing build/stage0045")
        else:
            print("[FAIL] missing build/stage0045")
            return 1

    bad = 0
    ids = []
    for item in acc:
        iid = item.get("id", "")
        ids.append(iid)
        if item.get("mandatory", True) and item.get("status") != "PASS":
            print(f"[FAIL] {iid} status={item.get('status')}")
            bad += 1
        if item.get("mandatory", True) and not item.get("requirement"):
            print(f"[FAIL] {iid} empty requirement")
            bad += 1
        for key in ("implementation_evidence", "verification_evidence"):
            ev = item.get(key) or []
            if item.get("mandatory", True) and not ev:
                print(f"[FAIL] {iid} empty {key}")
                bad += 1
            for e in ev:
                if not isinstance(e, str) or not e.strip():
                    print(f"[FAIL] {iid} empty evidence string")
                    bad += 1
                    continue
                if e.startswith("ctest"):
                    continue
                # directory evidence OK if exists
                cand = ROOT / e.split()[0]
                if "/" in e and not cand.exists() and not (ROOT / e).exists():
                    # allow trailing slash dirs
                    if not (ROOT / e.rstrip("/")).exists():
                        print(f"[FAIL] {iid} evidence path missing: {e}")
                        bad += 1
        tn = item.get("test_name", "")
        if (item.get("mandatory", True) and tn and "*" not in tn and ctests
                and (tn.startswith("gpu2d_") or tn.startswith("golden_"))
                and tn not in ctests):
            print(f"[FAIL] {iid} test_name not in ctest -N: {tn}")
            bad += 1

    if ids != AUTHORITATIVE_IDS:
        print(f"[FAIL] ID set mismatch got {len(ids)} expected 56")
        bad += 1

    if not REPORT.is_file():
        print("[FAIL] missing REPORT_0045")
        bad += 1
    else:
        text = REPORT.read_text(encoding="utf-8", errors="replace")
        for iid in AUTHORITATIVE_IDS:
            if iid not in text:
                print(f"[FAIL] REPORT missing {iid}")
                bad += 1
        if not re.search(r"## 2\. START_COMMIT\s*\n+\s*`[0-9a-f]{40}`", text):
            print("[FAIL] REPORT START_COMMIT not 40-hex")
            bad += 1
        if not re.search(r"## 3\. END_COMMIT\s*\n+\s*`[0-9a-f]{40}`", text):
            print("[FAIL] REPORT END_COMMIT not 40-hex")
            bad += 1
        if "| ID | Requirement |" not in text:
            print("[FAIL] REPORT matrix missing Requirement column")
            bad += 1

    if bad:
        print(f"stage0045 acceptance: FAIL ({bad})")
        return 1
    print(f"stage0045 acceptance: PASS ({len(ids)} ids)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
