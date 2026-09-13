#!/usr/bin/env python3
"""Flag obviously non-discriminating test assertion patterns. Stdlib only."""

from __future__ import annotations

import re
import sys
from pathlib import Path

FORBIDDEN = [
    re.compile(r"\|\|\s*true\b"),
    re.compile(r"&&\s*false\b"),
    # !x || x  and  x || !x  including member expressions like st.ok
    re.compile(r"!\s*[\w.]+\s*\|\|\s*[\w.]+\b"),
    re.compile(r"\b[\w.]+\s*\|\|\s*!\s*[\w.]+\b"),
]

ROOT = Path(__file__).resolve().parents[1] / "model" / "golden" / "tests"


def is_tautology(line: str) -> bool:
    for pat in FORBIDDEN:
        if pat.search(line):
            if "EXPECT" in line or "ASSERT" in line or "true)" in line:
                return True
    return False


def main() -> int:
    if not ROOT.is_dir():
        print(f"missing {ROOT}", file=sys.stderr)
        return 2
    bad = 0
    for path in ROOT.rglob("*"):
        if path.suffix not in {".cpp", ".hpp", ".h", ".c"}:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for i, line in enumerate(text.splitlines(), 1):
            if is_tautology(line):
                print(f"[FAIL] {path}:{i}: {line.strip()}")
                bad += 1
    # self-check: the script itself must reject a synthetic sample
    sample = "EXPECT_TRUE(!st.ok || st.ok);"
    if not is_tautology(sample):
        print("[FAIL] integrity script failed synthetic tautology sample", file=sys.stderr)
        return 1
    if bad:
        print(f"test integrity: FAIL ({bad} finding(s))")
        return 1
    print("test integrity: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
