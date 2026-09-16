#!/usr/bin/env python3
"""Strict TASK_APP2_001 acceptance checker. Stdlib only."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "tasks" / "APP2_001_ACCEPTANCE.json"
REPORT = ROOT / "docs" / "reports" / "REPORT_APP2_001_FACILITY_OMEGA_VERTICAL_SLICE.md"
BUILD = ROOT / "build" / "stage0045"

GROUPS = [("AST", 8), ("MAP", 8), ("PLY", 5), ("ENM", 5), ("CMB", 6),
          ("XP", 6), ("UI", 4), ("EQ", 4), ("SYS", 5), ("VIS", 5)]
AUTHORITATIVE_IDS = [f"{p}-{i:02d}" for p, n in GROUPS for i in range(1, n + 1)]

PLACEHOLDER = re.compile(
    r"^(covered by\b|application implementation\b|see report\b|similar test\b)",
    re.IGNORECASE,
)


def ctest_names(build: Path) -> set[str]:
    import subprocess
    r = subprocess.run(["ctest", "--test-dir", str(build), "-N"],
                       capture_output=True, text=True)
    names: set[str] = set()
    for line in r.stdout.splitlines():
        line = line.strip()
        if line.startswith("Test") and ":" in line and "#" in line:
            names.add(line.split(":", 1)[1].strip())
    return names


def parse_report_matrix(text: str) -> list[dict]:
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith("|"):
            continue
        parts = [p.strip() for p in line.strip("|").split("|")]
        if len(parts) < 5:
            continue
        iid = parts[0]
        if not re.fullmatch(r"[A-Z]+-\d{2}", iid):
            continue
        rows.append({
            "id": iid,
            "requirement": parts[1],
            "result": parts[2],
            "impl": parts[3],
            "ver": parts[4],
            "test": parts[5] if len(parts) > 5 else "",
        })
    return rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--allow-no-build", action="store_true")
    args = ap.parse_args()

    if not MANIFEST.is_file():
        print(f"missing {MANIFEST}", file=sys.stderr)
        return 1
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    acc = data.get("acceptance", [])
    by_id = {a["id"]: a for a in acc}

    ctests: set[str] = set()
    if BUILD.is_dir():
        ctests = ctest_names(BUILD)
        if not ctests and not args.allow_no_build:
            print("[FAIL] ctest -N empty")
            return 1
    elif not args.allow_no_build:
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
                    print(f"[FAIL] {iid} empty evidence")
                    bad += 1
                    continue
                if PLACEHOLDER.match(e.strip()):
                    print(f"[FAIL] {iid} placeholder evidence: {e!r}")
                    bad += 1
                    continue
                if e.startswith("gpu2d_") or e.startswith("golden_"):
                    continue
                cand = ROOT / e.split()[0]
                if "/" in e and not cand.exists() and not (ROOT / e.rstrip("/")).exists():
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
        print("[FAIL] missing REPORT_APP2_001")
        bad += 1
    else:
        text = REPORT.read_text(encoding="utf-8", errors="replace")
        if not re.search(r"## 2\. START_COMMIT\s*\n+\s*`[0-9a-f]{40}`", text):
            print("[FAIL] REPORT START_COMMIT not 40-hex")
            bad += 1
        if not re.search(r"## 3\. END_COMMIT\s*\n+\s*`[0-9a-f]{40}`", text):
            print("[FAIL] REPORT END_COMMIT not 40-hex")
            bad += 1
        if "| ID | Requirement |" not in text:
            print("[FAIL] REPORT matrix missing Requirement column")
            bad += 1
        rows = parse_report_matrix(text)
        if len(rows) != 56:
            print(f"[FAIL] REPORT matrix rows={len(rows)} expected 56")
            bad += 1
        row_ids = [r["id"] for r in rows]
        if row_ids != AUTHORITATIVE_IDS:
            print("[FAIL] REPORT matrix ID order/set mismatch")
            bad += 1
        for r in rows:
            iid = r["id"]
            m = by_id.get(iid)
            if not m:
                continue
            if r["result"].upper() != "PASS":
                print(f"[FAIL] REPORT {iid} result={r['result']!r}")
                bad += 1
            mt = m.get("test_name", "")
            if mt and mt not in r["test"] and mt not in r["ver"]:
                print(f"[FAIL] REPORT {iid} test {mt!r} not in row")
                bad += 1

    if bad:
        print(f"app2_001 acceptance: FAIL ({bad})")
        return 1
    print(f"app2_001 evidence structure: PASS ({len(ids)} ids, 56 report rows)")
    print(f"Stage status: {data.get('stage_status', 'NOT_REVIEWED')}; "
          f"owner visual approval: {data.get('owner_visual_approval', 'PENDING')}. "
          "This checker does not execute tests or grant visual acceptance.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
