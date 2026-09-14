#!/usr/bin/env python3
"""Intake FACILITY-Omega source pack: verify SHA256 and extract to immutable source/."""
from __future__ import annotations

import hashlib
import json
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ZIP = ROOT / "docs" / "tasks" / "FACILITY_OMEGA_First_Asset_Pack_V0.1.zip"
SRC = ROOT / "assets" / "facility_omega" / "source"
PREFIX = "FACILITY_OMEGA_First_Asset_Pack_V0.1/"


def main() -> int:
    if not ZIP.is_file():
        print(f"missing {ZIP}", file=sys.stderr)
        return 1
    SRC.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(ZIP) as z:
        man = json.loads(z.read(PREFIX + "MANIFEST.json"))
        bad = 0
        for f in man["files"]:
            name = f["path"]
            data = z.read(PREFIX + name)
            h = hashlib.sha256(data).hexdigest()
            ok = h == f["sha256"] and len(data) == f["bytes"]
            print(("PASS" if ok else "FAIL"), name, h[:16], len(data))
            if not ok:
                bad += 1
            dest = SRC / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            if not dest.exists() or dest.read_bytes() != data:
                dest.write_bytes(data)
    note = SRC / "PACK_SOURCE.txt"
    note.write_text(
        "Immutable extraction of FACILITY_OMEGA_First_Asset_Pack_V0.1.zip.\n"
        "Original: docs/tasks/FACILITY_OMEGA_First_Asset_Pack_V0.1.zip\n"
        "Do not modify files under source/.\n",
        encoding="utf-8",
    )
    print(f"extracted to {SRC}, failures={bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
