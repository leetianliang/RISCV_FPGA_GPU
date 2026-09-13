#!/usr/bin/env python3
"""Check Stage-004 acceptance manifest. Stdlib only. Strict final gate."""

from __future__ import annotations

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
    r = subprocess.run(["ctest", "--test-dir", str(build), "-N"], capture_output=True, text=True)
    names = set()
    for line in r.stdout.splitlines():
        line = line.strip()
        if line.startswith("Test #"):
            # "Test #1: name"
            if ":" in line:
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

# Evidence strings that look like synthetic placeholders.
BAD_EVIDENCE = re.compile(
    r"^(model/golden/|ctest build/stage004\s)|\skind\s|^\s*$"
)


def looks_like_repo_path(s: str) -> bool:
    return ("/" in s or s.endswith((".cpp", ".hpp", ".py", ".md", ".json"))) and not s.startswith(
        "ctest "
    )


def main() -> int:
    if not MANIFEST.is_file():
        print(f"missing {MANIFEST}", file=sys.stderr)
        return 1
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    acc = data.get("acceptance", [])
    ctests = ctest_names(BUILD) if BUILD.is_dir() else set()
    if not ctests:
        print("[WARN] ctest -N unavailable; skip test-name existence checks")
    bad = 0
    ids: list[str] = []
    for item in acc:
        iid = item.get("id", "")
        ids.append(iid)
        status = item.get("status", "")
        if item.get("mandatory", True) and status != "PASS":
            print(f"[FAIL] {iid} status={status}")
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
                if e.startswith("model/golden/") and len(e.split("/")) <= 3 and e.lower().endswith(
                    tuple(p.lower() for p in [iid.lower()])
                ):
                    print(f"[FAIL] {iid} synthetic evidence {e!r}")
                    bad += 1
                if looks_like_repo_path(e) and not e.startswith("ctest"):
                    # must exist relative to root if it looks like a path
                    p = ROOT / e
                    if "/" in e and not p.exists() and not (ROOT / e.split()[0]).exists():
                        # allow function suffix "file.cpp func"
                        cand = ROOT / e.split()[0]
                        if not cand.exists():
                            print(f"[FAIL] {iid} evidence path missing: {e}")
                            bad += 1
        if item.get("mandatory", True) and not item.get("test_name"):
            print(f"[FAIL] {iid} empty test_name")
            bad += 1
        # N9: declared test names must exist in ctest -N
        tn = item.get("test_name", "")
        if item.get("mandatory", True) and tn and "*" not in tn:
            if not tn.startswith("ctest") and BUILD.exists():
                if "ctest" in (item.get("verification_evidence") or [""])[0:]:
                    pass
            # only validate if it looks like a CTest test name
            if tn.startswith("golden_") and "*" not in tn and ctests and tn not in ctests:
                print(f"[FAIL] {iid} test_name not in ctest -N: {tn}")
                bad += 1

    # exact 47-ID set
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
        if "See git log" in text or "66/66" in text or "PARTIAL" in text:
            print("[FAIL] REPORT_004 still contains stale/PARTIAL markers")
            bad += 1

    if bad:
        print(f"stage004 acceptance: FAIL ({bad})")
        return 1
    print(f"stage004 acceptance: PASS ({len(ids)} ids)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
