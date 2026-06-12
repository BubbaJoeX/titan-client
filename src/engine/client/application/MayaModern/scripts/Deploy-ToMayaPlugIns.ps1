# Sync companion assets Debug -> Release (non-debug files only), then copy Release payload to Maya plug-ins.
# Usage (elevated CMD if Program Files is locked):
#   powershell -NoProfile -ExecutionPolicy Bypass -File Deploy-ToMayaPlugIns.ps1
# Optional:
#   powershell -File ... -MayaPlugInsDir "$env:USERPROFILE\Documents\maya\2027\plugins"
#   powershell -File ... -BuildRoot "D:\titan\...\MayaModern\build"

param(
    [string] $BuildRoot = "",
    [string] $MayaPlugInsDir = ""
)

if (-not $MayaPlugInsDir) {
    $candidates = @(
        (Join-Path $env:USERPROFILE "Documents\maya\2027\plugins"),
        "C:\Program Files\Autodesk\Maya2027\bin\plug-ins",
        "D:\Program Files\Autodesk\Maya2027\bin\plug-ins"
    )
    foreach ($dir in $candidates) {
        if (Test-Path (Split-Path $dir -Parent)) {
            $MayaPlugInsDir = $dir
            break
        }
    }
    if (-not $MayaPlugInsDir) {
        $MayaPlugInsDir = (Join-Path $env:USERPROFILE "Documents\maya\2027\plugins")
    }
}

$ErrorActionPreference = "Stop"
if (-not $BuildRoot) {
    # .../MayaModern/scripts -> .../MayaModern/build
    $BuildRoot = Join-Path (Split-Path $PSScriptRoot -Parent) "build"
}

$Dbg = Join-Path $BuildRoot "Debug"
$Rel = Join-Path $BuildRoot "Release"
foreach ($d in @($Dbg, $Rel)) {
    if (-not (Test-Path $d)) {
        Write-Error "Build directory missing: $d (configure & build MayaModern first)"
    }
}

$syncExtensions = @(".mel", ".py")

Write-Host "[Deploy] Build root: $BuildRoot"
Write-Host "[Deploy] Sync Debug -> Release (add missing non-debug companions)..."

Get-ChildItem -Path $Dbg -File | ForEach-Object {
    $name = $_.Name
    $ext = $_.Extension.ToLowerInvariant()
    if ($syncExtensions -notcontains $ext) { return }
    $dest = Join-Path $Rel $name
    if (-not (Test-Path $dest)) {
        Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
        Write-Host "  + copied to Release: $name"
    }
}

Write-Host "[Deploy] Copy Release -> Maya plug-ins: $MayaPlugInsDir"
New-Item -ItemType Directory -Force -Path $MayaPlugInsDir | Out-Null

$copyPatterns = @("SwgMayaEditor.mll", "*.mel", "*.py")
foreach ($pat in $copyPatterns) {
    Get-ChildItem -Path $Rel -Filter $pat -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $MayaPlugInsDir -Force
        Write-Host "  -> $($_.Name)"
    }
}

Write-Host "[Deploy] Done."
