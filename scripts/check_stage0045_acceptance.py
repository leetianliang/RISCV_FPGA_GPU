#!/usr/bin/env python3
"""Strict Stage-0045 acceptance checker. Stdlib only.

Parses the 56-row REPORT matrix and compares it to STAGE_0045_ACCEPTANCE.json.
"""
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
STRESS_JSON = ROOT / "results" / "stage0045" / "stress" / "stress_stats.json"

GROUPS = [("HOST", 5), ("API", 6), ("BACK", 6), ("SIM", 5), ("GAME", 6),
          ("FX", 6), ("XR", 5), ("STR", 6), ("SYS", 7), ("AUD", 4)]
AUTHORITATIVE_IDS = [f"{p}-{i:02d}" for p, n in GROUPS for i in range(1, n + 1)]

PLACEHOLDER = re.compile(
    r"^(ctest\b|covered by\b|application implementation\b|see report\b|similar test\b)",
    re.IGNORECASE,
)


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


def parse_report_matrix(text: str) -> list[dict]:
    """Parse | ID | Requirement | Result | Impl | Ver | Test | rows."""
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith("|"):
            continue
        parts = [p.strip() for p in line.strip("|").split("|")]
        if len(parts) < 6:
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
            "test": parts[5],
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
                    print(f"[FAIL] {iid} empty evidence string")
                    bad += 1
                    continue
                # Reject bare placeholder evidence without a concrete path/test.
                if PLACEHOLDER.match(e.strip()) and "/" not in e and not e.startswith(
                        ("gpu2d_", "golden_")):
                    print(f"[FAIL] {iid} placeholder evidence: {e!r}")
                    bad += 1
                    continue
                if e.startswith("ctest") and "/" not in e and "*" in e:
                    # e.g. "ctest stage0045 includes golden_*" — not concrete
                    print(f"[FAIL] {iid} non-concrete ctest evidence: {e!r}")
                    bad += 1
                    continue
                if e.startswith("ctest"):
                    continue
                cand = ROOT / e.split()[0]
                if "/" in e and not cand.exists() and not (ROOT / e).exists():
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
        if len(ids) != len(set(ids)):
            print("[FAIL] duplicate IDs in manifest")
        bad += 1

    if not REPORT.is_file():
        print("[FAIL] missing REPORT_0045")
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
        if len(row_ids) != len(set(row_ids)):
            print("[FAIL] REPORT matrix duplicate IDs")
            bad += 1
        for r in rows:
            iid = r["id"]
            m = by_id.get(iid)
            if not m:
                continue
            if r["result"].upper() != "PASS":
                print(f"[FAIL] REPORT {iid} result={r['result']!r} expected PASS")
                bad += 1
            # Compare test name / evidence presence
            mt = m.get("test_name", "")
            if mt and mt not in r["test"] and mt not in r["ver"]:
                print(f"[FAIL] REPORT {iid} test {mt!r} not in row test/ver")
                bad += 1
            if not r["impl"] or r["impl"] in ("-", "TODO"):
                print(f"[FAIL] REPORT {iid} empty impl evidence")
                bad += 1
            if not r["ver"] or r["ver"] in ("-", "TODO"):
                print(f"[FAIL] REPORT {iid} empty ver evidence")
                bad += 1

    # STR-06 / AUD-04: stress stats JSON must exist and be non-empty
    if not STRESS_JSON.is_file():
        print("[FAIL] missing stress_stats.json")
        bad += 1
    else:
        try:
            sj = json.loads(STRESS_JSON.read_text(encoding="utf-8"))
            if not sj.get("scenes"):
                print("[FAIL] stress_stats.json empty scenes")
                bad += 1
        except json.JSONDecodeError:
            print("[FAIL] stress_stats.json invalid JSON")
            bad += 1

    if bad:
        print(f"stage0045 acceptance: FAIL ({bad})")
        return 1
    print(f"stage0045 acceptance: PASS ({len(ids)} ids, 56 report rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
