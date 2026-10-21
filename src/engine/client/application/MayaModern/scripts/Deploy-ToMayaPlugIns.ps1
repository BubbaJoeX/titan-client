# Sync companion assets Debug -> Release (non-debug files only), then copy Release payload to Maya plug-ins.
# Usage (close Maya first; the .mll is locked while loaded):
#   powershell -NoProfile -ExecutionPolicy Bypass -File Deploy-ToMayaPlugIns.ps1
# Optional:
#   powershell -File ... -MayaPlugInsDir "C:\Program Files\Autodesk\Maya2027\bin\plug-ins"   (elevated)
#   powershell -File ... -BuildRoot "D:\titan\...\MayaModern\build-relock"

param(
    [string] $BuildRoot = "",
    [string] $MayaPlugInsDir = ""
)

if (-not $MayaPlugInsDir) {
    # Documents\maya\2027\plugins is on MAYA_PLUG_IN_PATH (Maya.env) and needs no admin rights;
    # userSetup.mel prefers it over the Program Files copy.
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
    # .../MayaModern/scripts -> .../MayaModern/build (build-mayamodern.ps1 output)
    $BuildRoot = Join-Path (Split-Path $PSScriptRoot -Parent) "build"
}

$Dbg = Join-Path $BuildRoot "Debug"
$Rel = Join-Path $BuildRoot "Release"
if (-not (Test-Path (Join-Path $Rel "SwgMayaEditor.mll"))) {
    Write-Error "SwgMayaEditor.mll missing under $Rel (run build-mayamodern.ps1 first)"
}

Write-Host "[Deploy] Build root: $BuildRoot"

# Debug is optional: build-mayamodern.ps1 only builds Release unless -Config Debug was used.
if (Test-Path $Dbg) {
    $syncExtensions = @(".mel", ".py")
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
}

Write-Host "[Deploy] Copy Release -> Maya plug-ins: $MayaPlugInsDir"
New-Item -ItemType Directory -Force -Path $MayaPlugInsDir | Out-Null

# Leftovers from earlier locked deploys (see below); ignore the ones still in use.
Get-ChildItem -Path $MayaPlugInsDir -Filter "*.locked-*" -File -ErrorAction SilentlyContinue |
    ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue }

$copyPatterns = @("SwgMayaEditor.mll", "SwgMayaEditor.pdb", "*.mel", "*.py")
foreach ($pat in $copyPatterns) {
    Get-ChildItem -Path $Rel -Filter $pat -File | ForEach-Object {
        $src = $_
        $dest = Join-Path $MayaPlugInsDir $src.Name
        try {
            Copy-Item -LiteralPath $src.FullName -Destination $dest -Force -ErrorAction Stop
        } catch [System.IO.IOException] {
            # A running Maya/mayapy maps the .mll (and holds the .pdb). Windows still allows renaming it, so move
            # it aside and copy the new build in; that process keeps the old one until it restarts.
            $aside = "$($src.Name).locked-$(Get-Date -Format yyyyMMddHHmmss)"
            Rename-Item -LiteralPath $dest -NewName $aside -ErrorAction Stop
            Copy-Item -LiteralPath $src.FullName -Destination $dest -Force -ErrorAction Stop
            Write-Warning "$($src.Name) was in use: old copy moved to $aside; restart Maya to load the new build."
        }
        Write-Host "  -> $($src.Name)"
    }
}

# The plug-in reads SwgMayaEditor.cfg from its own folder; seed one from the template, never overwrite.
$cfgDest = Join-Path $MayaPlugInsDir "SwgMayaEditor.cfg"
if (-not (Test-Path $cfgDest)) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "SwgMayaEditor.cfg.example") -Destination $cfgDest
    Write-Host "  -> SwgMayaEditor.cfg (from SwgMayaEditor.cfg.example; edit paths if yours differ)"
}

# Python packages shipped beside the .mll (e.g. swg_mocap/).
Get-ChildItem -Path $Rel -Directory | Where-Object { $_.Name -notlike "*.dir" } | ForEach-Object {
    $destDir = Join-Path $MayaPlugInsDir $_.Name
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    Copy-Item -Path (Join-Path $_.FullName "*") -Destination $destDir -Recurse -Force
    Write-Host "  -> $($_.Name)\"
}

Write-Host "[Deploy] Done."
