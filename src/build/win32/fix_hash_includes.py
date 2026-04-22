#!/usr/bin/env python3
"""
Replace #include <hash_map> and #include <hash_set> with x64-conditional includes.
Skips STLport source files.
"""
import re, sys
from pathlib import Path

ROOT = Path(r"d:\titan\client\src")
SKIP_DIRS = {"stlport453", "stlport"}

HASH_MAP_OLD = '#include <hash_map>'
HASH_MAP_NEW = """#ifdef _WIN64
#include <unordered_map>
#else
#include <hash_map>
#endif"""

HASH_SET_OLD = '#include <hash_set>'
HASH_SET_NEW = """#ifdef _WIN64
#include <unordered_set>
#else
#include <hash_set>
#endif"""


def should_skip(path: Path) -> bool:
    for part in path.parts:
        if part.lower() in SKIP_DIRS:
            return True
    return False


def fix_file(path: Path) -> bool:
    text = path.read_text(encoding='utf-8', errors='replace')
    original = text

    if HASH_MAP_OLD in text and '#ifdef _WIN64' not in text.split(HASH_MAP_OLD)[0].split('\n')[-1]:
        text = text.replace(HASH_MAP_OLD, HASH_MAP_NEW)

    if HASH_SET_OLD in text and '#ifdef _WIN64' not in text.split(HASH_SET_OLD)[0].split('\n')[-1]:
        text = text.replace(HASH_SET_OLD, HASH_SET_NEW)

    if text != original:
        path.write_text(text, encoding='utf-8')
        return True
    return False


def main():
    fixed = 0
    for ext in ('*.h', '*.cpp'):
        for path in ROOT.rglob(ext):
            if should_skip(path):
                continue
            try:
                text = path.read_text(encoding='utf-8', errors='replace')
                if HASH_MAP_OLD in text or HASH_SET_OLD in text:
                    if fix_file(path):
                        print(f"fixed: {path.relative_to(ROOT)}")
                        fixed += 1
            except Exception as e:
                print(f"ERROR: {path}: {e}", file=sys.stderr)

    print(f"\nDone: {fixed} files fixed")


if __name__ == "__main__":
    main()
