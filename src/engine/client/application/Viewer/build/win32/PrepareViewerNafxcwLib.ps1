# Builds $(IntDir)\NafxcwViewer_$(Configuration).lib for Viewer: the toolchain nafxcw(.d).lib
# ships afxmem.obj with global operator new/delete, which conflicts with sharedMemoryManager
# (OsNewDel). Vendor afxmem.cpp omits those via #if 0; we swap in a compiled afxmem.obj.
#
# Prefer MFC static libs shipped under src\external\3rd\library\atlmfc\lib\ (and INTEL\).
param(
    [Parameter(Mandatory = $true)][string]$IntDir,
    [Parameter(Mandatory = $true)][string]$Configuration,
    [Parameter(Mandatory = $true)][string]$Platform,
    # Prefer v120 (same as Viewer) so afxmem.obj matches the link step; MSBuild sets VCInstallDir.
    [string]$ClExe = '',
    [string]$LibExe = ''
)

$ErrorActionPreference = "Stop"

# build\win32 -> …\titan\client (seven .. segments)
$titanClient = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..\..\..\..")).Path

function Get-NafxcwPath {
    param(
        [string]$TitanClientRoot,
        [string]$BuildConfiguration
    )

    if ($env:VIEWER_NAFXCW_LIB -and (Test-Path -LiteralPath $env:VIEWER_NAFXCW_LIB)) {
        return $env:VIEWER_NAFXCW_LIB
    }

    $isDbg = ($BuildConfiguration -eq "Debug") -or ($BuildConfiguration -eq "Optimized")
    if ($TitanClientRoot) {
        if ($isDbg) {
            $rels = @(
                "src\external\3rd\library\atlmfc\lib\nafxcwd.lib",
                "src\external\3rd\library\atlmfc\lib\INTEL\NafxcWD.lib"
            )
        }
        else {
            $rels = @(
                "src\external\3rd\library\atlmfc\lib\nafxcw.lib",
                "src\external\3rd\library\atlmfc\lib\INTEL\NafxcW.lib"
            )
        }
        foreach ($rel in $rels) {
            $p = Join-Path $TitanClientRoot $rel
            if (Test-Path -LiteralPath $p) { return $p }
        }
    }

    if ($env:VCToolsInstallDir) {
        $p = Join-Path $env:VCToolsInstallDir.TrimEnd('\') "atlmfc\lib\x86\nafxcw.lib"
        if (Test-Path -LiteralPath $p) { return $p }
    }
    foreach ($dir in ($env:LIB -split ';' | Where-Object { $_ })) {
        $libX86 = $dir.TrimEnd('\')
        if ($libX86 -notmatch '[\\/]lib[\\/]x86$') { continue }
        $msvcToolsetRoot = Split-Path -Parent (Split-Path -Parent $libX86)
        $p = Join-Path $msvcToolsetRoot "atlmfc\lib\x86\nafxcw.lib"
        if (Test-Path -LiteralPath $p) { return $p }
    }
    $libExe = Get-Command lib.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($libExe) {
        $bin = Split-Path -Parent $libExe.Source
        $msvcRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $bin))
        $p = Join-Path $msvcRoot "atlmfc\lib\x86\nafxcw.lib"
        if (Test-Path -LiteralPath $p) { return $p }
    }
    $vs2013 = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio 12.0\VC\atlmfc\lib\nafxcw.lib"
    if (Test-Path -LiteralPath $vs2013) { return $vs2013 }
    $roots = @(
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022"),
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2019"),
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2017"),
        (Join-Path ${env:ProgramFiles} "Microsoft Visual Studio\2022")
    )
    foreach ($r in $roots) {
        if (-not (Test-Path -LiteralPath $r)) { continue }
        $hit = Get-ChildItem -LiteralPath $r -Filter nafxcw.lib -Recurse -Depth 7 -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $inst = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($inst) {
            $candidates = @()
            $candidates += Get-ChildItem -Path (Join-Path $inst "VC\Tools\MSVC") -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending |
                ForEach-Object { Join-Path $_.FullName "atlmfc\lib\x86\nafxcw.lib" } |
                Where-Object { Test-Path -LiteralPath $_ }
            $alt = Join-Path $inst "VC\atlmfc\lib\nafxcw.lib"
            if (Test-Path -LiteralPath $alt) { $candidates += $alt }
            if ($candidates) { return $candidates[0] }
        }
    }
    throw @"
PrepareViewerNafxcwLib: could not find nafxcw/nafxcwd (or NafxcW/NafxcWD) static lib.
Expected under $TitanClientRoot\src\external\3rd\library\atlmfc\lib\ or toolset atlmfc\lib\x86, or set VIEWER_NAFXCW_LIB.
"@
}

$afxmemCpp = Join-Path $titanClient "src\external\3rd\library\atlmfc\src\mfc\afxmem.cpp"
$incRoot = Join-Path $titanClient "src\external\3rd\library\atlmfc\include"
$mfcSrc = Join-Path $titanClient "src\external\3rd\library\atlmfc\src\mfc"
if (-not (Test-Path -LiteralPath $afxmemCpp)) {
    throw "PrepareViewerNafxcwLib: missing afxmem.cpp at $afxmemCpp"
}

$IntDir = $IntDir.TrimEnd('\')
if (-not (Test-Path -LiteralPath $IntDir)) {
    New-Item -ItemType Directory -Path $IntDir | Out-Null
}

if ($env:VCToolsInstallDir) {
    $toolRoots = @(
        (Join-Path $env:VCToolsInstallDir.TrimEnd('\') "bin\Hostx86\x86"),
        (Join-Path $env:VCToolsInstallDir.TrimEnd('\') "bin\Hostx64\x86")
    )
    foreach ($toolBin in $toolRoots) {
        if (Test-Path -LiteralPath $toolBin) {
            $env:PATH = "$toolBin;$env:PATH"
            break
        }
    }
}

$isDebug = ($Configuration -eq "Debug") -or ($Configuration -eq "Optimized")

function Resolve-ToolPath {
    param(
        [string]$Override,
        [string]$Name
    )
    if ($Override -and (Test-Path -LiteralPath $Override)) {
        return (Resolve-Path -LiteralPath $Override).Path
    }
    if ($env:VCINSTALLDIR) {
        $c = Join-Path $env:VCINSTALLDIR.TrimEnd('\') "bin\$Name"
        if (Test-Path -LiteralPath $c) {
            return $c
        }
    }
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) { return $cmd.Source }
    return $null
}

$nafxcwSrc = Get-NafxcwPath -TitanClientRoot $titanClient -BuildConfiguration $Configuration

$inCopy = Join-Path $IntDir "nafxcw_in_$Configuration.lib"
$stripped = Join-Path $IntDir "nafxcw_stripped_$Configuration.lib"
$afxmemObj = Join-Path $IntDir "afxmem_viewer_$Configuration.obj"
$outLib = Join-Path $IntDir "NafxcwViewer_$Configuration.lib"

Copy-Item -LiteralPath $nafxcwSrc -Destination $inCopy -Force

$libTool = Resolve-ToolPath -Override $LibExe -Name 'lib.exe'
$clTool = Resolve-ToolPath -Override $ClExe -Name 'cl.exe'
if (-not $libTool) { throw "PrepareViewerNafxcwLib: lib.exe not found (set LibExe or VCINSTALLDIR)." }
if (-not $clTool) { throw "PrepareViewerNafxcwLib: cl.exe not found (set ClExe or VCINSTALLDIR)." }

$libDir = Split-Path -Parent $libTool
if ($libDir) {
    $env:PATH = "$libDir;$env:PATH"
}

$listOutput = (& $libTool /nologo /list $inCopy 2>&1 | ForEach-Object { $_.ToString() })
$member = ($listOutput | Where-Object { $_ -match 'afxmem\.obj' } | Select-Object -First 1)
if ($member) {
    $member = $member.Trim()
    $stripArgs = @('/nologo', "/OUT:$stripped", $inCopy, "/REMOVE:$member")
    $p = Start-Process -FilePath $libTool -ArgumentList $stripArgs -Wait -NoNewWindow -PassThru
    if ($p.ExitCode -ne 0) {
        throw "lib.exe /REMOVE:$member failed (exit $($p.ExitCode))"
    }
} else {
    Write-Warning "PrepareViewerNafxcwLib: no afxmem.obj in library; using unmodified import lib (link may duplicate operator new if lib defines it)."
    Copy-Item -LiteralPath $inCopy -Destination $stripped -Force
}

# Only when cl is not the v120-era tool (VC\bin\cl or explicit -ClExe) add /Zc: flags so a newer cl
# produces an .obj the legacy link can consume; otherwise same cl as the rest of Viewer avoids LNK2019.
$usingToolsetCl = $false
if ($ClExe -and (Test-Path -LiteralPath $ClExe)) {
    $usingToolsetCl = $true
}
elseif ($env:VCINSTALLDIR) {
    $vcCl = Join-Path $env:VCINSTALLDIR.TrimEnd('\') 'bin\cl.exe'
    if ((Test-Path -LiteralPath $vcCl) -and ($clTool -eq (Resolve-Path -LiteralPath $vcCl).Path)) {
        $usingToolsetCl = $true
    }
}
$useModernClCompat = -not $usingToolsetCl

$clCommon = @(
    "/nologo", "/c", "/Fo$afxmemObj", "/EHsc", "/W3", "/Y-",
    "/I$mfcSrc", "/I$incRoot",
    "/DWIN32", "/D_WINDOWS", "/D_MBCS",
    "/D_AFX_NOFORCE_LIBS", "/DVC_EXTRALEAN", "/DNO_ANSIUNI_ONLY",
    "/D_MFC_OVERRIDES_NEW", "/DWINVER=0x0502", "/D_WIN32_WINNT=0x0502"
)
if ($useModernClCompat) {
    $clCommon += @("/Zc:sizedDealloc-", "/Zc:threadSafeInit-")
}

if ($isDebug) {
    $clArgs = $clCommon + @("/Od", "/MTd", "/Gy", "/D_DEBUG", $afxmemCpp)
} else {
    $clArgs = $clCommon + @("/O2", "/MT", "/DNDEBUG", $afxmemCpp)
}

& $clTool @clArgs
if ($LASTEXITCODE -ne 0) {
    throw "cl.exe afxmem.cpp failed (exit $LASTEXITCODE)"
}

& $libTool /nologo "/OUT:$outLib" $stripped $afxmemObj
if ($LASTEXITCODE -ne 0) {
    throw "lib.exe merge nafxcw_viewer failed (exit $LASTEXITCODE)"
}

Write-Host "PrepareViewerNafxcwLib: wrote $outLib (from $nafxcwSrc)"
