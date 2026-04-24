# Align SwgMapRasterizer Debug|x64 PreLink+Link with SwgClient (x64 Debug).
import re

P_SC = r"d:/titan/client/src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj"
P_SM = r"d:/titan/client/src/engine/shared/application/SwgMapRasterizer/build/win32/SwgMapRasterizer.vcxproj"

with open(P_SC, encoding="utf-8") as f:
    sc_lines = f.readlines()

dep = sc_lines[218].rstrip("\n")
libdir = sc_lines[220].rstrip("\n")
ignore = sc_lines[221].rstrip("\n")

# SwgMapRasterizer: six .. from build/win32 to client/src. SwgClient vcxproj uses seven .. + "src\\".
# Replace the repeated prefix in every path segment.
sevensrc = 7 * "..\\" + "src\\"
sixroot = 6 * "..\\"
if sevensrc not in libdir:
    raise SystemExit("Expected " + sevensrc + " in libdir")
libdir = libdir.replace(sevensrc, sixroot)

prelink = r"""    <PreLinkEvent>
      <Command>"$(MSBuildBinPath)\MSBuild.exe" "$(MSBuildThisFileDirectory)..\..\..\..\..\..\external\3rd\library\directx9\build\win32\directx9_x64_stubs.vcxproj" /p:Configuration=$(Configuration) /p:Platform=$(Platform) /v:minimal /nologo</Command>
    </PreLinkEvent>"""

link_block = (
    "    <Link>\n"
    + dep
    + "\n      <OutputFile>$(OutDir)$(ProjectName)_d.exe</OutputFile>\n"
    + libdir
    + "\n"
    + ignore
    + "\n"
    + r"""      <GenerateDebugInformation>true</GenerateDebugInformation>
      <ProgramDatabaseFile>$(OutDir)$(ProjectName)_d.pdb</ProgramDatabaseFile>
      <SubSystem>Windows</SubSystem>
      <LargeAddressAware>true</LargeAddressAware>
      <ForceFileOutput>Enabled</ForceFileOutput>
    </Link>"""
)

with open(P_SM, encoding="utf-8") as f:
    sm = f.read()

idx = sm.find('Condition="\'$(Configuration)|$(Platform)\'==\'Debug|x64\'"')
if idx < 0:
    raise SystemExit("Debug|x64 block not found")
i0 = sm.find("<PreLinkEvent>", idx)
i1 = sm.find("</Link>", idx) + len("</Link>")
sm2 = sm[:i0] + prelink + "\n" + link_block + sm[i1:]
with open(P_SM, "w", encoding="utf-8", newline="\r\n") as f:
    f.write(sm2)
print("Patched", P_SM)
