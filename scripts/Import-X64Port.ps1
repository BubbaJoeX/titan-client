[CmdletBinding()]
param(
    [string]$SourceRoot = "D:\titan\gr\client-tools",
    [string]$TargetRoot = "D:\titan\client",
    [switch]$SkipJuceCopy,
    [switch]$WhatIfOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Copy-Tree {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination,
        [string[]]$RobocopyArgs = @("/E", "/NFL", "/NDL", "/NJH", "/NJS", "/nc", "/ns", "/np")
    )

    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Source path is missing: $Source"
    }

    if ($WhatIfOnly) {
        Write-Host "[whatif] robocopy $Source -> $Destination"
        return
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $null = & robocopy $Source $Destination @RobocopyArgs
    if ($LASTEXITCODE -ge 8) {
        throw "robocopy failed ($LASTEXITCODE): $Source -> $Destination"
    }
}

function Copy-FileIfChanged {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Missing source file: $Source"
    }

    if ($WhatIfOnly) {
        Write-Host "[whatif] copy $Source -> $Destination"
        return
    }

    $destDir = Split-Path -Parent $Destination
    if ($destDir -and -not (Test-Path -LiteralPath $destDir)) {
        New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    }

    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

$sourceSrc = Join-Path $SourceRoot "src"
$targetSrc = Join-Path $TargetRoot "src"

Write-Host "Importing x64-dx9-vanilla-juce port"
Write-Host "  Source: $SourceRoot"
Write-Host "  Target: $TargetRoot"

# 1. Vendored deps and prerequisite manifests
Copy-Tree (Join-Path $SourceRoot "deps\x64") (Join-Path $TargetRoot "deps\x64")
Copy-Tree (Join-Path $SourceRoot "deps\build-prerequisites") (Join-Path $TargetRoot "deps\build-prerequisites")

# 2. Build scripts and docs
Copy-Tree (Join-Path $SourceRoot "scripts") (Join-Path $TargetRoot "scripts")

foreach ($doc in @("docs\x64-gameplay-client.md", "docs\god-client.md")) {
    $src = Join-Path $SourceRoot $doc
    if (Test-Path -LiteralPath $src) {
        Copy-FileIfChanged $src (Join-Path $TargetRoot $doc)
    }
}

# 3. MSBuild hooks (Titan-specific naming applied after copy)
Copy-FileIfChanged (Join-Path $SourceRoot "Directory.Build.props") (Join-Path $TargetRoot "Directory.Build.props")
Copy-FileIfChanged (Join-Path $SourceRoot "Directory.Build.targets") (Join-Path $TargetRoot "Directory.Build.targets")

# 4. JUCE + Miles header shim
if (-not $SkipJuceCopy) {
    Copy-Tree `
        (Join-Path $sourceSrc "external\3rd\library\JUCE-8.0.14") `
        (Join-Path $targetSrc "external\3rd\library\JUCE-8.0.14")
}

# 5. Runtime-critical source and x64 project files from the port branch
#    (Titan-specific gameplay/UI sources are intentionally excluded.)
$relativeFiles = @(
    "engine\client\library\clientAudio\src\shared\FirstClientAudio.h",
    "engine\client\library\clientAudio\src\win32\JuceMiles.cpp",
    "engine\client\library\clientAudio\src\win32\Audio.cpp",
    "engine\client\library\clientAudio\build\win32\clientAudio.vcxproj",
    "engine\shared\library\sharedMemoryManager\src\shared\MemoryManager.cpp",
    "engine\shared\library\sharedFoundation\src\win32\SetupSharedFoundation.cpp",
    "engine\shared\library\sharedFoundation\src\win32\Os.cpp",
    "engine\shared\library\sharedFoundation\src\win32\FloatingPointUnit.cpp",
    "engine\shared\library\sharedFoundation\src\shared\NetworkIdArchive.cpp",
    "engine\shared\library\sharedFoundation\src\shared\Tag.h",
    "engine\client\application\Direct3d9\src\win32\Direct3d9_VertexShaderData.cpp",
    "engine\client\application\Direct3d9\build\win32\Direct3d9.vcxproj",
    "engine\client\application\Direct3d9\build\win32\Direct3d9_ffp.vcxproj",
    "engine\client\application\Direct3d9\build\win32\Direct3d9_vsps.vcxproj",
    "engine\client\application\DllExport\build\win32\DllExport.vcxproj",
    "external\3rd\library\miles\include\Mss.h"
)

$backupDir = Join-Path $TargetRoot "scripts\port-backups"
if (-not $WhatIfOnly) {
    New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
    $swgClientProject = Join-Path $targetSrc "game\client\application\SwgClient\build\win32\SwgClient.vcxproj"
    if (Test-Path -LiteralPath $swgClientProject) {
        Copy-Item -LiteralPath $swgClientProject -Destination (Join-Path $backupDir "SwgClient.vcxproj.titan-win32") -Force
    }
}

foreach ($relativePath in $relativeFiles) {
    $src = Join-Path $sourceSrc $relativePath
    $dst = Join-Path $targetSrc $relativePath
    if (Test-Path -LiteralPath $src -PathType Leaf) {
        Copy-FileIfChanged $src $dst
    }
    else {
        Write-Warning "Skipped missing source file: $relativePath"
    }
}

# 6. Bulk-sync x64-enabled vcxproj files (preserves matching relative paths only)
$vcxprojFiles = Get-ChildItem -LiteralPath $sourceSrc -Recurse -Filter *.vcxproj -File
$synced = 0
foreach ($projectFile in $vcxprojFiles) {
    $relativePath = $projectFile.FullName.Substring($sourceSrc.Length + 1)
    $destination = Join-Path $targetSrc $relativePath
    if (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
        continue
    }

    $sourceText = Get-Content -LiteralPath $projectFile.FullName -Raw
    if ($sourceText -notmatch '\|x64') {
        continue
    }

    if ($relativePath -match '(?i)game\\client\\application\\SwgClient\\build\\win32\\SwgClient\.vcxproj$') {
        continue
    }

    if ($WhatIfOnly) {
        Write-Host "[whatif] sync vcxproj $relativePath"
    }
    else {
        Copy-Item -LiteralPath $projectFile.FullName -Destination $destination -Force
    }
    $synced++
}

Write-Host "Synced $synced x64-enabled vcxproj files."

if (-not $WhatIfOnly) {
    Write-Host "Run scripts\Finalize-TitanX64Port.ps1 next to apply Titan-specific naming and solution mappings."
}
