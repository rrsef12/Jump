"""Keep tests/output bounded: each suite keeps only its newest per-run folders.

Run folders are the 10-hex-digit `uuid4().hex[:10]` directories created by the
checks; anything else (fixtures, build outputs, logs) is left untouched.
Usage: `python tests/output_retention.py [keep]` or call `prune()`.
"""
from pathlib import Path
import re
import shutil
import sys

OUTPUT = Path(__file__).resolve().parent / 'output'
_RUN = re.compile(r'^[0-9a-f]{10}$')


def prune(keep=3):
    if not OUTPUT.is_dir():
        return 0
    removed = 0
    for suite in OUTPUT.iterdir():
        if not suite.is_dir():
            continue
        runs = sorted((d for d in suite.iterdir() if d.is_dir() and _RUN.match(d.name)),
                      key=lambda d: d.stat().st_mtime, reverse=True)
        for old in runs[keep:]:
            shutil.rmtree(old, ignore_errors=True)
            removed += 1
    return removed


if __name__ == '__main__':
    print(f'Removed {prune(int(sys.argv[1]) if len(sys.argv) > 1 else 3)} old run folders')
