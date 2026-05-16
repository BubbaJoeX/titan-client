"""Patch Viewer.vcxproj Debug/Optimized/Release Link settings."""
from pathlib import Path

root = Path(__file__).resolve().parent
vcx = root / "Viewer.vcxproj"

deps_debug = (root / "viewer_deps_debug_gen.txt").read_text(encoding="utf-8").strip()
deps_opt = (root / "viewer_deps_optimized_gen.txt").read_text(encoding="utf-8").strip()
lib_debug = (root / "viewer_libdirs_debug.txt").read_text(encoding="utf-8").strip()
lib_opt = (root / "viewer_libdirs_optimized.txt").read_text(encoding="utf-8").strip()

text = vcx.read_text(encoding="utf-8", errors="replace")

DEBUG_LINK = f"""    <PreLinkEvent>
      <Command>powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(ProjectDir)PrepareViewerNafxcwLib.ps1" -IntDir "$([System.String]::Copy('$(IntDir)').TrimEnd('\'))" -Configuration "$(Configuration)" -Platform "$(Platform)" -ClExe "$(PrepareViewerClExe)" -LibExe "$(PrepareViewerLibExe)"</Command>
    </PreLinkEvent>
    <Link>
      <AdditionalDependencies>$(IntDir)NafxcwViewer_$(Configuration).lib;{deps_debug};%(AdditionalDependencies)</AdditionalDependencies>
      <OutputFile>$(OutDir)$(ProjectName)_d.exe</OutputFile>
      <AdditionalLibraryDirectories>{lib_debug};%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <IgnoreSpecificDefaultLibraries>LIBCMT;MSVCRTD;libc;MSVCRT;nafxcw.lib;nafxcwd.lib;%(IgnoreSpecificDefaultLibraries)</IgnoreSpecificDefaultLibraries>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <ProgramDatabaseFile>$(OutDir)$(ProjectName)_d.pdb</ProgramDatabaseFile>
      <SubSystem>Windows</SubSystem>
      <LargeAddressAware>true</LargeAddressAware>
      <AdditionalOptions>/SAFESEH:NO %(AdditionalOptions)</AdditionalOptions>
    </Link>"""

OPT_LINK = f"""    <PreLinkEvent>
      <Command>powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(ProjectDir)PrepareViewerNafxcwLib.ps1" -IntDir "$([System.String]::Copy('$(IntDir)').TrimEnd('\'))" -Configuration "$(Configuration)" -Platform "$(Platform)" -ClExe "$(PrepareViewerClExe)" -LibExe "$(PrepareViewerLibExe)"</Command>
    </PreLinkEvent>
    <Link>
      <AdditionalDependencies>$(IntDir)NafxcwViewer_$(Configuration).lib;{deps_opt};%(AdditionalDependencies)</AdditionalDependencies>
      <OutputFile>$(OutDir)$(ProjectName)_o.exe</OutputFile>
      <AdditionalLibraryDirectories>{lib_opt};%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <IgnoreSpecificDefaultLibraries>LIBCMT;MSVCRTD;libc;MSVCRT;nafxcw.lib;nafxcwd.lib;%(IgnoreSpecificDefaultLibraries)</IgnoreSpecificDefaultLibraries>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <ProgramDatabaseFile>$(OutDir)$(ProjectName)_o.pdb</ProgramDatabaseFile>
      <SubSystem>Windows</SubSystem>
      <LargeAddressAware>true</LargeAddressAware>
      <AdditionalOptions>/SAFESEH:NO %(AdditionalOptions)</AdditionalOptions>
    </Link>"""


def replace_link_block(content: str, cond: str, replacement: str) -> str:
    cond_marker = f"<ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='{cond}'\">"
    i = content.find(cond_marker)
    if i < 0:
        raise SystemExit(f"block {cond} not found")
    j = content.find("</ItemDefinitionGroup>", i)
    block = content[i:j]
    a = block.find("    <PreLinkEvent>")
    if a < 0:
        a = block.find("    <Link>")
    if a < 0:
        raise SystemExit(f"<PreLinkEvent> or <Link> not in {cond}")
    b = block.find("    </Link>", a)
    if b < 0:
        raise SystemExit(f"</Link> not in {cond}")
    b += len("    </Link>")
    new_block = block[:a] + replacement + "\n" + block[b:]
    return content[:i] + new_block + content[j:]


text = replace_link_block(text, "Debug|Win32", DEBUG_LINK)
text = replace_link_block(text, "Optimized|Win32", OPT_LINK)

rel_cond = "<ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|Win32'\">"
ri = text.find(rel_cond)
if ri < 0:
    raise SystemExit("Release block not found")
rj = text.find("</ItemDefinitionGroup>", ri)
rel_block = text[ri:rj]
if "/FORCE:MULTIPLE" in rel_block:
    rel_block = rel_block.replace("/FORCE:MULTIPLE ", "")
# Ensure nafxcw is ignored (OsNewDel / MFC afxmem LNK2005 fix)
if "nafxcw.lib" not in rel_block:
    rel_block = rel_block.replace(
        "<IgnoreSpecificDefaultLibraries>libc;MSVCRT;%(IgnoreSpecificDefaultLibraries)</IgnoreSpecificDefaultLibraries>",
        "<IgnoreSpecificDefaultLibraries>libc;MSVCRT;nafxcw.lib;nafxcwd.lib;%(IgnoreSpecificDefaultLibraries)</IgnoreSpecificDefaultLibraries>",
        1,
    )
text = text[:ri] + rel_block + text[rj:]

vcx.write_text(text, encoding="utf-8", newline="\r\n")
print("patched", vcx)
