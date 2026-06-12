#requires -Version 5.1
<#
.SYNOPSIS
    Configure and build SwgMayaEditor (MayaModern) for Windows x64 / Maya 2027.

.DESCRIPTION
    Defaults:
      - Devkit:  D:\titan\lib\Maya2027\devkitBase
      - Maya:    C:\Program Files\Autodesk\Maya2027
      - VS:      D:\Program Files\Microsoft Visual Studio\18\Insiders

    By default, deletes MayaModern\build\, runs CMake, then builds SwgMayaEditor.mll.
    With -Shallow, keeps the build tree for incremental compiles.

.PARAMETER Config
    MSBuild configuration: Release or Debug.

.PARAMETER Generator
    CMake generator. Auto-detected when omitted (VS 18 2026 if Insiders present, else VS 17 2022).

.PARAMETER DevKitLocation
    Path to devkitBase (headers + lib). Passed as -DDEVKIT_LOCATION and DEVKIT_LOCATION env.

.PARAMETER MayaLocation
    Maya install root (runtime). Passed as -DMAYA_LOCATION and MAYA_LOCATION env.

.PARAMETER VsInstall
    Visual Studio instance for CMAKE_GENERATOR_INSTANCE (Insiders path).

.PARAMETER Shallow
    Keep build directory; configure + incremental compile only.

.EXAMPLE
    .\build-mayamodern.ps1

.EXAMPLE
    .\build-mayamodern.ps1 -Shallow

.EXAMPLE
    .\build-mayamodern.ps1 -Config Debug
#>
param(
    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',

    [string] $Generator = '',

    [string] $DevKitLocation = '',

    [string] $MayaLocation = '',

    [string] $VsInstall = '',

    [switch] $Shallow
)

$ErrorActionPreference = 'Stop'

$DefaultDevKit   = 'D:\titan\lib\Maya2027\devkitBase'
$DefaultMaya     = 'C:\Program Files\Autodesk\Maya2027'
$DefaultVs       = 'D:\Program Files\Microsoft Visual Studio\18\Insiders'

if (-not $DevKitLocation) {
    $DevKitLocation = if ($env:DEVKIT_LOCATION) { $env:DEVKIT_LOCATION } else { $DefaultDevKit }
}
if (-not $MayaLocation) {
    $MayaLocation = if ($env:MAYA_LOCATION) { $env:MAYA_LOCATION } else { $DefaultMaya }
}
if (-not $VsInstall) {
    $VsInstall = if ($env:CMAKE_GENERATOR_INSTANCE) { $env:CMAKE_GENERATOR_INSTANCE } else { $DefaultVs }
}

$SourceRoot = $PSScriptRoot
$BuildDir = Join-Path $SourceRoot 'build'

function Resolve-CMake {
    $candidates = @(
        'C:\Program Files\CMake\bin\cmake.exe',
        'D:\Program Files\CMake\bin\cmake.exe'
    )
    foreach ($path in $candidates) {
        if (Test-Path -LiteralPath $path) { return $path }
    }
    $fromPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($fromPath) { return $fromPath.Source }
    throw 'cmake.exe not found. Install CMake or add it to PATH.'
}

function Resolve-Generator {
    if ($Generator) { return $Generator }
    # Let CMake auto-detect VS Insiders; explicit CMAKE_GENERATOR_INSTANCE paths often fail
    # when the install is not registered with vswhere.
    return 'Visual Studio 18 2026'
}

$cmakeExe = Resolve-CMake
$resolvedGenerator = Resolve-Generator -VsPath $VsInstall

function Invoke-Tool {
    param([string[]] $Arguments)
    Write-Host ("& $cmakeExe " + ($Arguments -join ' ')) -ForegroundColor Cyan
    & $cmakeExe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE"
    }
}

if (-not (Test-Path -LiteralPath $DevKitLocation)) {
    throw "Devkit not found: $DevKitLocation"
}
if (-not (Test-Path -LiteralPath (Join-Path $DevKitLocation 'cmake\pluginEntry.cmake'))) {
    throw "Invalid devkit (missing cmake\pluginEntry.cmake): $DevKitLocation"
}

Write-Host '============================================' -ForegroundColor Cyan
Write-Host '  SwgMayaEditor build (Maya 2027)' -ForegroundColor Cyan
Write-Host '============================================' -ForegroundColor Cyan
Write-Host "Devkit:    $DevKitLocation"
Write-Host "Maya:      $MayaLocation"
Write-Host "Generator: $resolvedGenerator"
Write-Host "VS:        (CMake auto-detect; Insiders at $VsInstall)" -ForegroundColor DarkGray
Write-Host ''

if ($Shallow) {
    Write-Host "Shallow: keeping $BuildDir" -ForegroundColor DarkCyan
    if (-not (Test-Path -LiteralPath $BuildDir)) {
        New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    }
} else {
    if (Test-Path -LiteralPath $BuildDir) {
        Write-Host "Clean: removing $BuildDir" -ForegroundColor Yellow
        Remove-Item -LiteralPath $BuildDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

$env:DEVKIT_LOCATION = $DevKitLocation
$env:MAYA_LOCATION = $MayaLocation

$cfgArgs = @(
    '-S', $SourceRoot,
    '-B', $BuildDir,
    '-G', $resolvedGenerator,
    '-A', 'x64',
    '-DCMAKE_SYSTEM_VERSION=10.0',
    "-DDEVKIT_LOCATION=$DevKitLocation",
    "-DMAYA_LOCATION=$MayaLocation"
)

try {
    Invoke-Tool $cfgArgs
} catch {
    if ($resolvedGenerator -eq 'Visual Studio 18 2026') {
        Write-Host 'Retrying with Visual Studio 17 2022 generator...' -ForegroundColor Yellow
        if (Test-Path -LiteralPath $BuildDir) {
            Remove-Item -LiteralPath $BuildDir -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
        $resolvedGenerator = 'Visual Studio 17 2022'
        $cfgArgs = @(
            '-S', $SourceRoot,
            '-B', $BuildDir,
            '-G', $resolvedGenerator,
            '-A', 'x64',
            '-DCMAKE_SYSTEM_VERSION=10.0',
            "-DDEVKIT_LOCATION=$DevKitLocation",
            "-DMAYA_LOCATION=$MayaLocation"
        )
        Invoke-Tool $cfgArgs
    } else {
        throw
    }
}

Invoke-Tool @('--build', $BuildDir, '--config', $Config)

$mll = Join-Path (Join-Path $BuildDir $Config) 'SwgMayaEditor.mll'
if (Test-Path -LiteralPath $mll) {
    Write-Host "Output: $mll" -ForegroundColor Green
    Write-Host ''
    Write-Host 'Deploy:' -ForegroundColor Cyan
    Write-Host "  .\scripts\Deploy-ToMayaPlugIns.ps1"
} else {
    Write-Host "Build finished; expected output not found at $mll" -ForegroundColor Yellow
}
