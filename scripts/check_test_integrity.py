#!/usr/bin/env python3
"""Flag obviously non-discriminating test assertion patterns. Stdlib only."""

from __future__ import annotations

import re
import sys
from pathlib import Path

FORBIDDEN = [
    re.compile(r"\|\|\s*true\b"),
    re.compile(r"&&\s*false\b"),
    re.compile(r"!\s*\w+\s*\|\|\s*\w+\b"),
]

ROOT = Path(__file__).resolve().parents[1] / "model" / "golden" / "tests"


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
            for pat in FORBIDDEN:
                if pat.search(line):
                    # allow legitimate multi-line logic that isn't always-true asserts
                    if "EXPECT" in line or "ASSERT" in line or "true)" in line:
                        print(f"[FAIL] {path}:{i}: {line.strip()}")
                        bad += 1
    if bad:
        print(f"test integrity: FAIL ({bad} finding(s))")
        return 1
    print("test integrity: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
