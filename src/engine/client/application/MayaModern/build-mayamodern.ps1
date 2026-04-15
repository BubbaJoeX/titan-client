#requires -Version 5.1
<#
.SYNOPSIS
    Configure and build SwgMayaEditor (MayaModern) for Windows x64.

.DESCRIPTION
    By default, deletes MayaModern\build\, runs CMake (Visual Studio 2022 x64), then
    builds SwgMayaEditor.mll. With -Shallow, the existing build tree is kept so MSBuild
    only recompiles changed translation units (incremental build).

.PARAMETER Config
    MSBuild configuration: Release or Debug.

.PARAMETER Generator
    CMake generator (default matches README).

.PARAMETER MayaLocation
    Optional MAYA_LOCATION for FindMaya. If omitted, uses env MAYA_LOCATION when set.

.PARAMETER Shallow
    Do not remove the build directory; run CMake configure (updates cache if needed)
    then incremental compile. Use after a full build or when CMakeLists / generator
    have not changed and you only edited sources.

.EXAMPLE
    .\build-mayamodern.ps1

.EXAMPLE
    .\build-mayamodern.ps1 -Config Debug

.EXAMPLE
    .\build-mayamodern.ps1 -Shallow

.EXAMPLE
    .\build-mayamodern.ps1 -MayaLocation "C:\Program Files\Autodesk\Maya2026"
#>
param(
    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',

    [string] $Generator = 'Visual Studio 17 2022',

    [string] $MayaLocation = $env:MAYA_LOCATION,

    [switch] $Shallow
)

$ErrorActionPreference = 'Stop'

$SourceRoot = $PSScriptRoot
$BuildDir = Join-Path $SourceRoot 'build'

function Invoke-Tool {
    param([string[]] $Arguments)
    Write-Host ("cmake " + ($Arguments -join ' ')) -ForegroundColor Cyan
    & cmake @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE"
    }
}

if ($Shallow) {
    Write-Host "Shallow: keeping $BuildDir (incremental compile)" -ForegroundColor DarkCyan
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
$cfgArgs = @(
    '-S', $SourceRoot,
    '-B', $BuildDir,
    '-G', $Generator,
    '-A', 'x64'
)
if ($MayaLocation) {
    $cfgArgs += "-DMAYA_LOCATION=$MayaLocation"
    Write-Host "Using MAYA_LOCATION: $MayaLocation" -ForegroundColor DarkGray
}
Invoke-Tool $cfgArgs

Invoke-Tool @('--build', $BuildDir, '--config', $Config)

$mll = Join-Path (Join-Path $BuildDir $Config) 'SwgMayaEditor.mll'
if (Test-Path -LiteralPath $mll) {
    Write-Host "Output: $mll" -ForegroundColor Green
} else {
    Write-Host "Build finished; expected output not found at $mll (check config path)." -ForegroundColor Yellow
}
