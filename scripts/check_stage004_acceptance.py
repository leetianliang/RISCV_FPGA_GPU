#!/usr/bin/env python3
"""Check Stage-004 acceptance manifest. Stdlib only. Strict final gate."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "tasks" / "STAGE_004_ACCEPTANCE.json"
REPORT = ROOT / "docs" / "reports" / "REPORT_004_Golden_Tile_Renderer_and_Binning.md"
BUILD = ROOT / "build" / "stage004"


def ctest_names(build: Path) -> set[str]:
    import subprocess

    r = subprocess.run(
        ["ctest", "--test-dir", str(build), "-N"],
        capture_output=True,
        text=True,
    )
    names: set[str] = set()
    for line in r.stdout.splitlines():
        line = line.strip()
        # "Test  #N: name" (two spaces after Test on CMake)
        if line.startswith("Test") and ":" in line and "#" in line:
            names.add(line.split(":", 1)[1].strip())
    return names


AUTHORITATIVE_IDS = [
    f"{p}-{i:02d}"
    for p, n in [
        ("GVF", 6),
        ("TDS", 6),
        ("BIN", 6),
        ("TR", 8),
        ("EQ", 9),
        ("PROF", 5),
        ("EXP", 4),
        ("AUD", 3),
    ]
    for i in range(1, n + 1)
]


def looks_like_repo_path(s: str) -> bool:
    return ("/" in s or s.endswith((".cpp", ".hpp", ".py", ".md", ".json"))) and not s.startswith(
        "ctest "
    )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--allow-no-build",
        action="store_true",
        help="dev only: WARN instead of FAIL when build/stage004 is missing",
    )
    args = ap.parse_args()

    if not MANIFEST.is_file():
        print(f"missing {MANIFEST}", file=sys.stderr)
        return 1
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    acc = data.get("acceptance", [])

    ctests: set[str] = set()
    have_build = BUILD.is_dir()
    if have_build:
        ctests = ctest_names(BUILD)
        if not ctests:
            if args.allow_no_build:
                print("[WARN] ctest -N produced no tests; skipping name checks")
            else:
                print("[FAIL] ctest -N produced no tests")
                return 1
    else:
        if args.allow_no_build:
            print("[WARN] missing build/stage004; skipping ctest -N name checks")
        else:
            print("[FAIL] missing build/stage004 for ctest -N")
            return 1

    bad = 0
    ids: list[str] = []
    for item in acc:
        iid = item.get("id", "")
        ids.append(iid)
        status = item.get("status", "")
        if item.get("mandatory", True) and status != "PASS":
            print(f"[FAIL] {iid} status={status}")
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
                    print(f"[FAIL] {iid} empty evidence in {key}")
                    bad += 1
                    continue
                if looks_like_repo_path(e) and not e.startswith("ctest"):
                    p = ROOT / e
                    if "/" in e and not p.exists() and not (ROOT / e.split()[0]).exists():
                        cand = ROOT / e.split()[0]
                        if not cand.exists():
                            print(f"[FAIL] {iid} evidence path missing: {e}")
                            bad += 1
        if item.get("mandatory", True) and not item.get("test_name"):
            print(f"[FAIL] {iid} empty test_name")
            bad += 1
        tn = item.get("test_name", "")
        if (
            item.get("mandatory", True)
            and tn
            and "*" not in tn
            and tn.startswith("golden_")
            and ctests
            and tn not in ctests
        ):
            print(f"[FAIL] {iid} test_name not in ctest -N: {tn}")
            bad += 1

    if ids != AUTHORITATIVE_IDS:
        print(f"[FAIL] ID set mismatch (got {len(ids)} expected 47)")
        if len(ids) != len(set(ids)):
            print("[FAIL] duplicate IDs")
        bad += 1

    if not REPORT.is_file():
        print("[FAIL] missing REPORT_004")
        bad += 1
    else:
        text = REPORT.read_text(encoding="utf-8", errors="replace")
        for iid in AUTHORITATIVE_IDS:
            if iid not in text:
                print(f"[FAIL] REPORT_004 missing id {iid}")
                bad += 1
        if (
            "See git log" in text
            or "See `git log" in text
            or "66/66" in text
            or "PARTIAL" in text
        ):
            print("[FAIL] REPORT_004 still contains stale/PARTIAL markers")
            bad += 1
        # END_COMMIT must be a 40-hex SHA on a code-span line
        m = re.search(r"## 3\. END_COMMIT\s*\n+\s*`([0-9a-f]{40})`", text)
        if not m:
            print("[FAIL] REPORT_004 END_COMMIT is not an exact 40-hex SHA")
            bad += 1
        # matrix must include Requirement column header
        if "| ID | Requirement | Result |" not in text and "| ID | Requirement |" not in text:
            print("[FAIL] REPORT_004 matrix missing Requirement column")
            bad += 1

    if bad:
        print(f"stage004 acceptance: FAIL ({bad})")
        return 1
    print(f"stage004 acceptance: PASS ({len(ids)} ids)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
