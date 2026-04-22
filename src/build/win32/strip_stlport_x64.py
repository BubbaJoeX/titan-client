#!/usr/bin/env python3
"""
Strips STLport include dirs, link libraries, and library directories
from all x64 ItemDefinitionGroup blocks in .vcxproj files referenced by swg.sln.

Also strips _USE_32BIT_TIME_T from x64 preprocessor definitions.
"""
from __future__ import annotations
import re, sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
SLN_FILE   = SCRIPT_DIR / "swg.sln"


def parse_vcxproj_paths(sln_path: Path) -> list[Path]:
    sln_dir = sln_path.parent
    projects = []
    pattern = re.compile(
        r'Project\("[^"]*"\)\s*=\s*"[^"]*"\s*,\s*"([^"]+\.vcxproj)"',
        re.IGNORECASE,
    )
    text = sln_path.read_text(encoding='utf-8')
    for m in pattern.finditer(text):
        rel_path = m.group(1).replace("\\", "/")
        full_path = (sln_dir / rel_path).resolve()
        projects.append(full_path)
    return projects


def strip_stlport_from_value(val: str) -> str:
    """Remove STLport entries from a semicolon-delimited value string."""
    parts = val.split(';')
    filtered = []
    for p in parts:
        low = p.lower().replace('\\', '/')
        if 'stlport453/stlport' in low or 'stlport/stlport' in low:
            continue
        if 'stlport453/lib/' in low:
            continue
        if p.strip().lower() in ('stlport_vc71_static.lib', 'stlport_vc71_stldebug_static.lib'):
            continue
        filtered.append(p)
    return ';'.join(filtered)


def process_vcxproj(path: Path) -> bool:
    """Strip STLport from x64 ItemDefinitionGroup blocks. Returns True if modified."""
    text = path.read_text(encoding='utf-8')
    if '|x64' not in text:
        return False

    original = text

    x64_idg_re = re.compile(
        r'(<ItemDefinitionGroup\s+Condition="[^"]*\|x64[^"]*"[^>]*>)(.*?)(</ItemDefinitionGroup>)',
        re.DOTALL,
    )

    def fix_block(m: re.Match) -> str:
        open_tag = m.group(1)
        body = m.group(2)
        close_tag = m.group(3)

        def fix_tag_value(tag_match: re.Match) -> str:
            tag_name = tag_match.group(1)
            value = tag_match.group(2)
            new_val = strip_stlport_from_value(value)
            # Never emit a bare/empty include list: MSBuild inheritance only.
            if tag_name == "AdditionalIncludeDirectories":
                new_val = re.sub(r";{2,}", ";", new_val).strip(";")
                if not new_val:
                    new_val = "%(AdditionalIncludeDirectories)"
            return f'<{tag_name}>{new_val}</{tag_name}>'

        body = re.sub(
            r'<(AdditionalIncludeDirectories)>(.*?)</\1>',
            fix_tag_value, body
        )
        body = re.sub(
            r'<(AdditionalDependencies)>(.*?)</\1>',
            fix_tag_value, body
        )
        body = re.sub(
            r'<(AdditionalLibraryDirectories)>(.*?)</\1>',
            fix_tag_value, body
        )

        body = body.replace('_USE_32BIT_TIME_T=1;', '')
        body = body.replace('_USE_32BIT_TIME_T=1', '')

        return open_tag + body + close_tag

    text = x64_idg_re.sub(fix_block, text)

    if text != original:
        path.write_text(text, encoding='utf-8')
        return True
    return False


def main() -> int:
    if not SLN_FILE.is_file():
        print(f"ERROR: Cannot find {SLN_FILE}", file=sys.stderr)
        return 1

    projects = parse_vcxproj_paths(SLN_FILE)
    print(f"Found {len(projects)} projects in swg.sln")

    fixed = 0
    skipped = 0
    missing = 0

    for p in projects:
        if not p.is_file():
            missing += 1
            continue
        if process_vcxproj(p):
            print(f"fixed: {p.name}")
            fixed += 1
        else:
            skipped += 1

    print(f"\nDone: {fixed} fixed, {skipped} unchanged, {missing} missing")
    return 0


if __name__ == "__main__":
    sys.exit(main())
