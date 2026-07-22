[CmdletBinding()]
param(
    [ValidateSet("Release", "Optimized", "Debug")]
    [string]$Configuration = "Release",

    [string]$PlatformToolset = "v145",

    [string]$VisualStudioRoot,

    [string]$StagePath,

    [ValidateSet("minimal", "normal", "detailed", "diagnostic")]
    [string]$Verbosity = "normal",

    [int]$HeartbeatSeconds = 30,

    [string]$LogPath
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

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$solution = Join-Path $repoRoot "src\build\win32\swg.sln"
$prerequisites = & (Join-Path $PSScriptRoot "Test-X64BuildPrerequisites.ps1") `
    -PlatformToolset $PlatformToolset `
    -VisualStudioRoot $VisualStudioRoot `
    -Quiet `
    -PassThru
$msbuild = $prerequisites.VisualStudio.MSBuildPath
$env:DXSDK_DIR = $prerequisites.DirectXSdk.Root.TrimEnd("\") + "\"
$env:CL = "/Zm512"

$requiredInputs = @(
    "deps\x64\include\libjpeg-turbo\jpeglib.h",
    "deps\x64\lib\jpeg-static.lib",
    "deps\x64\lib\libxml2.lib",
    "deps\x64\lib\pcre.lib",
    "deps\x64\lib\dpvs.lib",
    "deps\x64\lib\libEverQuestTCG.lib",
    "deps\x64\lib\vivoxSharedWrapper.lib",
    "deps\x64\lib\swg-stubs.lib"
)

foreach ($relativePath in $requiredInputs) {
    $path = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required vendored x64 input is missing: $path"
    }
}

$arguments = @(
    $solution,
    "/t:SwgTitan",
    "/p:Configuration=$Configuration",
    "/p:Platform=x64",
    "/p:PlatformToolset=$PlatformToolset",
    "/m:4",
    "/nr:false",
    "/v:$Verbosity",
    "/clp:Summary;ShowTimestamp;ShowProgress=Force"
)

if (-not $LogPath) {
    $LogPath = Join-Path $PSScriptRoot ("build-{0}-last.log" -f $Configuration.ToLowerInvariant())
}

Write-Host "MSBuild: $msbuild"
Write-Host "Solution: $solution"
Write-Host "Configuration: $Configuration|x64 ($PlatformToolset)"
Write-Host "Verbosity: $Verbosity"
Write-Host "Heartbeat: every ${HeartbeatSeconds}s (shows progress during silent compile phases)"
Write-Host "Log file: $LogPath"
Write-Host "Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host ""

function Write-BuildHeartbeat {
    param(
        [datetime]$Started,
        [string]$LogFile,
        [int]$LastPrintedOffset
    )

    $elapsed = (Get-Date) - $Started
    $compilerCount = @(Get-Process -Name cl -ErrorAction SilentlyContinue).Count
    $linkCount = @(Get-Process -Name link -ErrorAction SilentlyContinue).Count
    $lastLine = "(no log output yet)"
    if (Test-Path -LiteralPath $LogFile -PathType Leaf) {
        $tail = Get-Content -LiteralPath $LogFile -Tail 8 -ErrorAction SilentlyContinue |
            Where-Object { $_.Trim().Length -gt 0 }
        if ($tail) {
            $lastLine = ($tail[-1] -replace '\s{2,}', ' ').Trim()
            if ($lastLine.Length -gt 180) {
                $lastLine = $lastLine.Substring(0, 177) + "..."
            }
        }
    }

    Write-Host ""
    Write-Host ("=== BUILD HEARTBEAT [{0:HH:mm:ss}] elapsed {1:g} | cl.exe={2} link.exe={3} ===" -f (Get-Date), $elapsed, $compilerCount, $linkCount)
    Write-Host $lastLine
    Write-Host ""

    if (Test-Path -LiteralPath $LogFile -PathType Leaf) {
        return (Get-Item -LiteralPath $LogFile).Length
    }

    return $LastPrintedOffset
}

function Invoke-MsBuildWithLiveOutput {
    param(
        [Parameter(Mandatory)][string]$MsBuildPath,
        [Parameter(Mandatory)][string[]]$MsBuildArguments,
        [Parameter(Mandatory)][string]$LogFile,
        [Parameter(Mandatory)][datetime]$Started,
        [Parameter(Mandatory)][int]$HeartbeatIntervalSeconds
    )

    if (Test-Path -LiteralPath $LogFile -PathType Leaf) {
        Remove-Item -LiteralPath $LogFile -Force
    }

    $stderrFile = "${LogFile}.err"
    if (Test-Path -LiteralPath $stderrFile -PathType Leaf) {
        Remove-Item -LiteralPath $stderrFile -Force
    }

    $process = Start-Process `
        -FilePath $MsBuildPath `
        -ArgumentList $MsBuildArguments `
        -PassThru `
        -NoNewWindow `
        -RedirectStandardOutput $LogFile `
        -RedirectStandardError $stderrFile

    $logOffset = 0L
    $nextHeartbeat = (Get-Date).AddSeconds($HeartbeatIntervalSeconds)

    while (-not $process.HasExited) {
        if ((Get-Date) -ge $nextHeartbeat) {
            $logOffset = Write-BuildHeartbeat -Started $Started -LogFile $LogFile -LastPrintedOffset $logOffset
            $nextHeartbeat = (Get-Date).AddSeconds($HeartbeatIntervalSeconds)
        }

        if (Test-Path -LiteralPath $LogFile -PathType Leaf) {
            $stream = [System.IO.File]::Open($LogFile, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            try {
                if ($stream.Length -gt $logOffset) {
                    $stream.Seek($logOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
                    $reader = [System.IO.StreamReader]::new($stream)
                    while (-not $reader.EndOfStream) {
                        $line = $reader.ReadLine()
                        if ($null -ne $line) {
                            Write-Host $line
                        }
                    }
                    $logOffset = $stream.Length
                }
            }
            finally {
                $stream.Dispose()
            }
        }

        Start-Sleep -Milliseconds 500
    }

    $null = $process.WaitForExit()

    if (Test-Path -LiteralPath $LogFile -PathType Leaf) {
        $stream = [System.IO.File]::Open($LogFile, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try {
            if ($stream.Length -gt $logOffset) {
                $stream.Seek($logOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
                $reader = [System.IO.StreamReader]::new($stream)
                while (-not $reader.EndOfStream) {
                    $line = $reader.ReadLine()
                    if ($null -ne $line) {
                        Write-Host $line
                    }
                }
            }
        }
        finally {
            $stream.Dispose()
        }
    }

    if (Test-Path -LiteralPath $stderrFile -PathType Leaf) {
        $stderr = Get-Content -LiteralPath $stderrFile -Raw -ErrorAction SilentlyContinue
        if ($stderr -and $stderr.Trim().Length -gt 0) {
            Write-Host $stderr
        }
    }

    return $process.ExitCode
}

$buildStarted = Get-Date
$buildExitCode = Invoke-MsBuildWithLiveOutput `
    -MsBuildPath $msbuild `
    -MsBuildArguments $arguments `
    -LogFile $LogPath `
    -Started $buildStarted `
    -HeartbeatIntervalSeconds $HeartbeatSeconds
$buildElapsed = (Get-Date) - $buildStarted

Write-Host ""
Write-Host ("Finished: {0}  Elapsed: {1:g}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $buildElapsed)
Write-Host ("Log file: {0}" -f $LogPath)

if ($buildExitCode -ne 0) {
    throw "The x64 client build failed with exit code $buildExitCode."
}

$suffix = @{
    Release   = "r"
    Optimized = "o"
    Debug     = "d"
}[$Configuration]

$artifacts = @(
    "src\build\win32\x64\$Configuration\SwgTitan_$suffix.exe",
    "src\build\win32\x64\$Configuration\gl05_$suffix.dll",
    "src\build\win32\x64\$Configuration\gl06_$suffix.dll",
    "src\build\win32\x64\$Configuration\gl07_$suffix.dll",
    "src\build\win32\x64\$Configuration\DllExport.dll"
)

foreach ($relativePath in $artifacts) {
    $path = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Expected build artifact is missing: $path"
    }

    $machine = Get-PeMachine -Path $path
    if ($machine -ne 0x8664) {
        throw ("Expected x64 PE machine 0x8664, found 0x{0:x4}: {1}" -f $machine, $path)
    }

    $item = Get-Item -LiteralPath $path
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    Write-Host ("x64  {0,10:N0} bytes  {1}  {2}" -f $item.Length, $hash, $path)
}

if ($StagePath) {
    & (Join-Path $PSScriptRoot "Stage-X64Client.ps1") `
        -ClientRoot $StagePath `
        -Configuration $Configuration
}
