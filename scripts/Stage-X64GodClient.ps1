[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$ClientRoot = "D:\titan\exe\win64_rel",

    [ValidateSet("Release")]
    [string]$Configuration = "Release",

    [switch]$NoBackup
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-PeMachine {
    param([Parameter(Mandatory)][string]$Path)

    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d) {
            throw "Not a PE file: $Path"
        }

        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "Invalid PE signature: $Path"
        }

        return $reader.ReadUInt16()
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $ClientRoot -PathType Container)) {
    throw "ClientRoot does not exist: $ClientRoot"
}

$clientRootPath = (Resolve-Path -LiteralPath $ClientRoot).Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path

if (-not (Test-Path -LiteralPath (Join-Path $clientRootPath "client.cfg") -PathType Leaf)) {
    throw "ClientRoot does not contain client.cfg: $clientRootPath"
}

$runtimeFiles = @(
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\SwgGodClient_r.exe"
        Name   = "SwgGodClient_r.exe"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\gl05_r.dll"
        Name   = "gl05_r.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\gl06_r.dll"
        Name   = "gl06_r.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\gl07_r.dll"
        Name   = "gl07_r.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\DllExport.dll"
        Name   = "DllExport.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "deps\qt3-win64-src\lib\qt-mt3.dll"
        Name   = "qt-mt3.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "deps\x64\bin\libxml2.dll"
        Name   = "libxml2.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "deps\x64\bin\iconv-2.dll"
        Name   = "iconv-2.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "deps\x64\bin\z.dll"
        Name   = "z.dll"
    }
)

foreach ($file in $runtimeFiles) {
    if (-not (Test-Path -LiteralPath $file.Source -PathType Leaf)) {
        throw "Runtime source is missing. Build the x64 God client first: $($file.Source)"
    }

    if ((Get-PeMachine -Path $file.Source) -ne 0x8664) {
        throw ("Refusing to stage non-x64 PE 0x{0:x4}: {1}" -f (Get-PeMachine -Path $file.Source), $file.Source)
    }
}

foreach ($path in @(
    (Join-Path $env:SystemRoot "System32\d3dx9_43.dll"),
    (Join-Path $env:SystemRoot "System32\vcruntime140.dll")
)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-PeMachine -Path $path) -ne 0x8664) {
        throw "Required x64 system runtime is missing or incompatible: $path"
    }
}

$localSystemDllNames = @("dbghelp.dll", "dbghelp_6.3.17.0.dll", "d3d9.dll", "d3dx9_43.dll", "ddraw.dll", "dinput8.dll", "vcruntime140.dll")
$incompatibleLocalPaths = @(
    $localSystemDllNames | ForEach-Object {
        $path = Join-Path $clientRootPath $_
        if ((Test-Path -LiteralPath $path -PathType Leaf) -and ((Get-PeMachine -Path $path) -ne 0x8664)) {
            $path
        }
    }
)
$obsoleteRuntimePaths = @(
    Join-Path $clientRootPath "mss64.dll" |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
)

if (-not $PSCmdlet.ShouldProcess($clientRootPath, "stage $Configuration x64 God client")) {
    return
}

$backupDirectory = $null
$newFiles = @($runtimeFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $clientRootPath $_.Name) -PathType Leaf) } | ForEach-Object Name)
if (-not $NoBackup) {
    $existingTargets = @(
        $runtimeFiles | ForEach-Object { Join-Path $clientRootPath $_.Name } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
    )
    $manifestPath = Join-Path $clientRootPath "x64-godclient-runtime-manifest.json"
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        $existingTargets += $manifestPath
    }
    $existingTargets += $incompatibleLocalPaths
    $existingTargets += $obsoleteRuntimePaths
    $existingTargets = @($existingTargets | Sort-Object -Unique)

    if ($existingTargets.Count -gt 0) {
        $backupDirectory = Join-Path $clientRootPath (".x64-backups\godclient-{0}" -f (Get-Date -Format "yyyyMMdd-HHmmss"))
        if (Test-Path -LiteralPath $backupDirectory) {
            $backupDirectory += "-$PID"
        }
        New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
        foreach ($path in $existingTargets) {
            Copy-Item -LiteralPath $path -Destination (Join-Path $backupDirectory ([IO.Path]::GetFileName($path))) -Force
        }
    }
}

foreach ($path in @($incompatibleLocalPaths) + @($obsoleteRuntimePaths)) {
    Remove-Item -LiteralPath $path -Force
}

foreach ($file in $runtimeFiles) {
    Copy-Item -LiteralPath $file.Source -Destination (Join-Path $clientRootPath $file.Name) -Force
}

$gitCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
$gitBranch = (& git -C $repoRoot branch --show-current).Trim()
$workingTreeDirty = @(& git -C $repoRoot status --porcelain).Count -gt 0
$stagedFiles = @(
    $runtimeFiles | ForEach-Object {
        $destination = Join-Path $clientRootPath $_.Name
        [ordered]@{
            name = $_.Name
            bytes = (Get-Item -LiteralPath $destination).Length
            sha256 = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
            machine = "x64"
        }
    }
)

$manifest = [ordered]@{
    formatVersion = 2
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    configuration = $Configuration
    platform = "x64"
    sourceRepository = $repoRoot
    sourceCommit = $gitCommit
    sourceBranch = $gitBranch
    workingTreeDirty = $workingTreeDirty
    clientRoot = $clientRootPath
    qtRuntime = "local Qt 3 Windows port, x64"
    audioBackend = "JUCE 8.0.14 with WASAPI and WAV/MP3/Ogg decoders"
    perforceIntegration = "disabled in x64 build"
    backupDirectory = $backupDirectory
    newlyAddedFiles = $newFiles
    removedIncompatibleLocalFiles = @($incompatibleLocalPaths | ForEach-Object { [IO.Path]::GetFileName($_) })
    removedObsoleteRuntimeFiles = @($obsoleteRuntimePaths | ForEach-Object { [IO.Path]::GetFileName($_) })
    files = $stagedFiles
}
$utf8NoBom = [Text.UTF8Encoding]::new($false)
$manifestPath = Join-Path $clientRootPath "x64-godclient-runtime-manifest.json"
[IO.File]::WriteAllText($manifestPath, (($manifest | ConvertTo-Json -Depth 5) + [Environment]::NewLine), $utf8NoBom)

Write-Host "Staged $($runtimeFiles.Count) x64 God client runtime files to $clientRootPath"
if ($backupDirectory) {
    Write-Host "Previous runtime files were backed up to $backupDirectory"
}
Write-Warning "Audio uses JUCE 8.0.14/WASAPI; the legacy Perforce integration remains disabled in this x64 build."
