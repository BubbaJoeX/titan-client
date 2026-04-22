#!/usr/bin/env python3
"""
Adds Debug|x64, Optimized|x64, Release|x64 platform configurations to legacy
Win32-only .vcxproj files.  Duplicates every Win32 Condition block as an x64
sibling, redirecting compile/x64 output dirs and stripping _USE_32BIT_TIME_T.

Usage:  python add_x64_platform.py
"""
from __future__ import annotations
import re, sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
SRC_ROOT   = SCRIPT_DIR / "../../../../../../"          # client/src

SHARED_LIB = SRC_ROOT / "engine/shared/library"
CLIENT_LIB = SRC_ROOT / "engine/client/library"
EXT_OURS   = SRC_ROOT / "external/ours/library"

PROJECTS: list[Path] = [
    EXT_OURS   / "archive/build/win32/archive.vcxproj",
    EXT_OURS   / "fileInterface/build/win32/fileInterface.vcxproj",
    EXT_OURS   / "unicodeArchive/build/win32/unicodeArchive.vcxproj",
    SHARED_LIB / "sharedCompression/build/win32/sharedCompression.vcxproj",
    SHARED_LIB / "sharedDebug/build/win32/sharedDebug.vcxproj",
    SHARED_LIB / "sharedFile/build/win32/sharedFile.vcxproj",
    SHARED_LIB / "sharedFoundationTypes/build/win32/sharedFoundationTypes.vcxproj",
    SHARED_LIB / "sharedFoundation/build/win32/sharedFoundation.vcxproj",
    SHARED_LIB / "sharedImage/build/win32/sharedImage.vcxproj",
    SHARED_LIB / "sharedIoWin/build/win32/sharedIoWin.vcxproj",
    SHARED_LIB / "sharedLog/build/win32/sharedLog.vcxproj",
    SHARED_LIB / "sharedMath/build/win32/sharedMath.vcxproj",
    SHARED_LIB / "sharedMemoryManager/build/win32/sharedMemoryManager.vcxproj",
    SHARED_LIB / "sharedMessageDispatch/build/win32/sharedMessageDispatch.vcxproj",
    SHARED_LIB / "sharedObject/build/win32/sharedObject.vcxproj",
    SHARED_LIB / "sharedRandom/build/win32/sharedRandom.vcxproj",
    SHARED_LIB / "sharedRegex/build/win32/sharedRegex.vcxproj",
    SHARED_LIB / "sharedSynchronization/build/win32/sharedSynchronization.vcxproj",
    SHARED_LIB / "sharedTerrain/build/win32/sharedTerrain.vcxproj",
    SHARED_LIB / "sharedThread/build/win32/sharedThread.vcxproj",
    SHARED_LIB / "sharedUtility/build/win32/sharedUtility.vcxproj",
    SHARED_LIB / "sharedXml/build/win32/sharedXml.vcxproj",
    CLIENT_LIB / "clientGraphics/build/win32/clientGraphics.vcxproj",
    CLIENT_LIB / "clientObject/build/win32/clientObject.vcxproj",
    CLIENT_LIB / "clientTerrain/build/win32/clientTerrain.vcxproj",
    SCRIPT_DIR / "SwgMapRasterizer.vcxproj",
]

CONFIGS = ["Debug", "Optimized", "Release"]

def make_x64_block(block: str) -> str:
    b = block.replace("|Win32", "|x64")
    b = b.replace("'Win32'", "'x64'")
    b = b.replace("compile\\win32\\", "compile\\x64\\")
    b = b.replace("compile/win32/", "compile/x64/")
    b = b.replace("_USE_32BIT_TIME_T=1;", "")
    return b

def patch(text: str) -> str:
    if "Debug|x64" in text:
        return text

    # --- 1. Add x64 ProjectConfiguration entries ---
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

    # --- 2. Duplicate top-level Condition blocks (PropertyGroup, ImportGroup, ItemDefinitionGroup) ---
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

    # --- 3. Duplicate per-file Condition attributes inside <ItemGroup> for ClCompile/ClInclude ---
    attr_re = re.compile(
        r'(Condition="[^"]*\|Win32[^"]*")'
    )
    # Find lines with per-file Win32 conditions (PrecompiledHeader, Optimization, etc.)
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


def main() -> int:
    ok = 0
    fail = 0
    for p in PROJECTS:
        p = p.resolve()
        if not p.is_file():
            print(f"SKIP (missing): {p}", file=sys.stderr)
            fail += 1
            continue
        original = p.read_text(encoding='utf-8')
        if "Debug|x64" in original:
            print(f"skip (already x64): {p.name}")
            ok += 1
            continue
        try:
            result = patch(original)
            p.write_text(result, encoding='utf-8')
            print(f"patched: {p.name}")
            ok += 1
        except Exception as e:
            print(f"FAIL {p.name}: {e}", file=sys.stderr)
            fail += 1
    print(f"\nDone: {ok} ok, {fail} failed")
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
