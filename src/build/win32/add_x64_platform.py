#!/usr/bin/env python3
"""
Adds Debug|x64, Optimized|x64, Release|x64 platform configurations to all
Win32-only .vcxproj files referenced in swg.sln.

Duplicates every Win32 Condition block as an x64 sibling, redirecting
compile/x64 output dirs, stripping _USE_32BIT_TIME_T, removing STLport
include/link references, and removing /SAFESEH from x64 linker options.

Usage:  python add_x64_platform.py [--dry-run]
"""
from __future__ import annotations
import re, sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
SLN_FILE   = SCRIPT_DIR / "swg.sln"

CONFIGS = ["Debug", "Optimized", "Release"]

STLPORT_INCLUDE_PATTERNS = [
    re.compile(r'[^;]*stlport453[/\\]stlport[^;]*;?', re.IGNORECASE),
    re.compile(r'[^;]*stlport[/\\]stlport[^;]*;?', re.IGNORECASE),
]

STLPORT_LIB_PATTERNS = [
    re.compile(r'stlport_vc71_static\.lib;?', re.IGNORECASE),
    re.compile(r'stlport_vc71_stldebug_static\.lib;?', re.IGNORECASE),
]

STLPORT_LIBDIR_PATTERNS = [
    re.compile(r'[^;]*stlport453[/\\]lib[/\\]win32[^;]*;?', re.IGNORECASE),
]


def parse_vcxproj_paths(sln_path: Path) -> list[Path]:
    """Extract all .vcxproj relative paths from a .sln file."""
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


def strip_stlport_from_block(block: str) -> str:
    """Remove STLport include dirs, link libs, and lib dirs from x64 block."""
    for pat in STLPORT_INCLUDE_PATTERNS:
        block = pat.sub('', block)
    for pat in STLPORT_LIB_PATTERNS:
        block = pat.sub('', block)
    for pat in STLPORT_LIBDIR_PATTERNS:
        block = pat.sub('', block)
    # Clean up trailing/leading/double semicolons
    block = re.sub(r';{2,}', ';', block)
    block = re.sub(r'>;', '>', block)
    block = re.sub(r';(<)', r'\1', block)
    return block


def strip_safeseh(block: str) -> str:
    """Remove /SAFESEH and ImageHasSafeExceptionHandlers from x64 blocks."""
    block = re.sub(r'\s*/SAFESEH(?::NO)?', '', block)
    block = re.sub(
        r'\s*<ImageHasSafeExceptionHandlers>[^<]*</ImageHasSafeExceptionHandlers>',
        '', block,
    )
    return block


def make_x64_block(block: str) -> str:
    """Convert a Win32 condition block to x64."""
    b = block.replace("|Win32", "|x64")
    b = b.replace("'Win32'", "'x64'")
    b = b.replace("compile\\win32\\", "compile\\x64\\")
    b = b.replace("compile/win32/", "compile/x64/")
    b = b.replace("_USE_32BIT_TIME_T=1;", "")
    b = b.replace("_USE_32BIT_TIME_T=1", "")
    b = b.replace("<TargetMachine>MachineX86</TargetMachine>",
                   "<TargetMachine>MachineX64</TargetMachine>")
    b = strip_stlport_from_block(b)
    b = strip_safeseh(b)
    return b


def patch(text: str) -> str:
    """Add x64 platform configurations to a .vcxproj file."""
    if "Debug|x64" in text:
        return text

    # 1. Add x64 ProjectConfiguration entries
    x64_entries = ""
    for cfg in CONFIGS:
        x64_entries += (
            f'    <ProjectConfiguration Include="{cfg}|x64">\n'
            f'      <Configuration>{cfg}</Configuration>\n'
            f'      <Platform>x64</Platform>\n'
            f'    </ProjectConfiguration>\n'
        )
    text = text.replace(
        "  </ItemGroup>\n  <PropertyGroup Label=\"Globals\">",
        x64_entries + "  </ItemGroup>\n  <PropertyGroup Label=\"Globals\">",
        1,
    )

    # 2. Duplicate top-level Condition blocks
    block_re = re.compile(
        r'(  <(?:PropertyGroup|ImportGroup|ItemDefinitionGroup)'
        r'[^>]*Condition="[^"]*\|Win32[^"]*"[^>]*>.*?</(?:PropertyGroup|ImportGroup|ItemDefinitionGroup)>)',
        re.DOTALL,
    )
    matches = list(block_re.finditer(text))
    for m in reversed(matches):
        x64_block = make_x64_block(m.group(0))
        insert_pos = m.end()
        text = text[:insert_pos] + "\n" + x64_block + text[insert_pos:]

    # 3. Duplicate per-file Condition attributes inside ItemGroups
    attr_re = re.compile(r'(Condition="[^"]*\|Win32[^"]*")')
    lines = text.split('\n')
    new_lines: list[str] = []
    for line in lines:
        new_lines.append(line)
        if attr_re.search(line) and '<ItemDefinitionGroup' not in line and \
           '<PropertyGroup' not in line and '<ImportGroup' not in line and \
           '<Import ' not in line:
            x64_line = make_x64_block(line)
            if x64_line != line and '|x64' not in line:
                new_lines.append(x64_line)
    text = '\n'.join(new_lines)

    return text


def patch_sln(sln_path: Path) -> bool:
    """Add x64 solution configurations to swg.sln."""
    text = sln_path.read_text(encoding='utf-8')
    if "Debug|x64" in text:
        print("swg.sln: already has x64 configs, skipping")
        return True

    # Add solution-level x64 configs
    sln_configs = ""
    for cfg in CONFIGS:
        sln_configs += f"\t\t{cfg}|x64 = {cfg}|x64\n"

    text = text.replace(
        "\tEndGlobalSection\n\tGlobalSection(ProjectConfigurationPlatforms)",
        sln_configs + "\tEndGlobalSection\n\tGlobalSection(ProjectConfigurationPlatforms)",
        1,
    )

    # Add per-project x64 mappings
    # For each project GUID that has Win32 mappings, add x64 mappings
    guid_re = re.compile(
        r'(\{[A-F0-9-]+\})\.(\w+)\|Win32\.(ActiveCfg|Build\.0) = (\w+)\|Win32',
        re.IGNORECASE,
    )
    lines = text.split('\n')
    new_lines: list[str] = []
    for line in lines:
        new_lines.append(line)
        m = guid_re.search(line)
        if m:
            guid = m.group(1)
            cfg = m.group(2)
            prop = m.group(3)
            orig_cfg = m.group(4)
            x64_line = f"\t\t{guid}.{cfg}|x64.{prop} = {orig_cfg}|x64"
            if x64_line not in text:
                new_lines.append(x64_line)

    text = '\n'.join(new_lines)
    sln_path.write_text(text, encoding='utf-8')
    print(f"patched: swg.sln (added x64 solution configs)")
    return True


def main() -> int:
    dry_run = "--dry-run" in sys.argv

    if not SLN_FILE.is_file():
        print(f"ERROR: Cannot find {SLN_FILE}", file=sys.stderr)
        return 1

    projects = parse_vcxproj_paths(SLN_FILE)
    print(f"Found {len(projects)} projects in swg.sln")

    ok = 0
    fail = 0
    skip = 0

    for p in projects:
        if not p.is_file():
            print(f"SKIP (missing): {p.name}")
            skip += 1
            continue
        original = p.read_text(encoding='utf-8')
        if "Debug|x64" in original:
            print(f"skip (already x64): {p.name}")
            skip += 1
            continue
        try:
            result = patch(original)
            if not dry_run:
                p.write_text(result, encoding='utf-8')
            print(f"{'[dry-run] ' if dry_run else ''}patched: {p.name}")
            ok += 1
        except Exception as e:
            print(f"FAIL {p.name}: {e}", file=sys.stderr)
            fail += 1

    # Also patch swg.sln
    if not dry_run:
        patch_sln(SLN_FILE)

    print(f"\nDone: {ok} patched, {skip} skipped, {fail} failed")
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
