"""The pinned identity of the oracle: assets/KE.EXE must be exactly the original executable.

Every verifier that compares against assets/KE.EXE (tools/check.py original(), tools/image.py Original/compare)
fails closed unless the file's full SHA-256 equals ORIGINAL_SHA256.  The digest is also the one recorded in
manifest.json "original" -> "KE.EXE" (checked to agree, so neither copy can drift silently).

    python tools/oracle.py            # prints OK or the expected/actual digests (exit 1)
"""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ORIGINAL = ROOT / "assets" / "KE.EXE"
ORIGINAL_SHA256 = "5a465cc78d7ef797934f4a95029a278ff10c1ae547ce0cd067eb8c076f50b7ee"
ORIGINAL_SIZE = 210031


class OracleMismatch(SystemExit):
    pass


def require_original_bytes(data: bytes, path=ORIGINAL) -> bytes:
    """Fail closed unless `data` (the bytes actually used as the oracle) has the pinned identity."""
    actual = hashlib.sha256(data).hexdigest()
    if actual != ORIGINAL_SHA256 or len(data) != ORIGINAL_SIZE:
        raise OracleMismatch(f"ORACLE MISMATCH: {path} is not the pinned original KE.EXE\n"
                             f"  expected sha256 {ORIGINAL_SHA256} ({ORIGINAL_SIZE} bytes)\n"
                             f"  actual   sha256 {actual} ({len(data)} bytes)")
    try:
        rec = json.loads((ROOT / "manifest.json").read_text())["original"]["KE.EXE"]
    except (OSError, ValueError, KeyError) as exc:
        raise OracleMismatch(f"ORACLE MISMATCH: manifest.json has no original KE.EXE identity ({exc})") from None
    if rec.get("sha256") != ORIGINAL_SHA256 or rec.get("size") != ORIGINAL_SIZE:
        raise OracleMismatch("ORACLE MISMATCH: manifest.json original KE.EXE identity "
                             f"{rec.get('sha256')} ({rec.get('size')} bytes) != pinned {ORIGINAL_SHA256} "
                             f"({ORIGINAL_SIZE} bytes)")
    return data


def require_original(path=ORIGINAL) -> bytes:
    """Read the oracle file and return its bytes only if it has the pinned identity."""
    p = Path(path)
    if not p.is_file():
        raise OracleMismatch(f"ORACLE MISSING: {p} (expected sha256 {ORIGINAL_SHA256})")
    return require_original_bytes(p.read_bytes(), p)


def main():
    require_original()
    print(f"oracle OK: {ORIGINAL} sha256 {ORIGINAL_SHA256}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
