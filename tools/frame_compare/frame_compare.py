#!/usr/bin/env python3
"""Exact raw framebuffer compare. Python standard library only.

Exit 0 on exact match; non-zero on mismatch or error.
"""

from __future__ import annotations

import sys
from pathlib import Path


def read_bytes(path: Path) -> bytes:
    return path.read_bytes()


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("Usage: frame_compare.py <expected.raw> <actual.raw>", file=sys.stderr)
        return 2

    expected_path = Path(argv[1])
    actual_path = Path(argv[2])

    if not expected_path.is_file():
        print(f"[FAIL] missing expected: {expected_path}", file=sys.stderr)
        return 2
    if not actual_path.is_file():
        print(f"[FAIL] missing actual: {actual_path}", file=sys.stderr)
        return 2

    expected = read_bytes(expected_path)
    actual = read_bytes(actual_path)

    if len(expected) != len(actual):
        print(
            f"[FAIL] size mismatch: expected {len(expected)} bytes, actual {len(actual)} bytes"
        )
        return 1

    if expected == actual:
        print(f"[PASS] exact match ({len(expected)} bytes)")
        print(f"  expected: {expected_path}")
        print(f"  actual:   {actual_path}")
        return 0

    mismatch_count = 0
    first = None
    for i, (e, a) in enumerate(zip(expected, actual)):
        if e != a:
            mismatch_count += 1
            if first is None:
                first = i

    print(f"[FAIL] mismatch_count={mismatch_count}")
    if first is not None:
        print(f"  first_mismatch_offset={first}")
        print(f"  expected_byte=0x{expected[first]:02X}")
        print(f"  actual_byte=0x{actual[first]:02X}")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
