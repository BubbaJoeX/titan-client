#requires -Version 5.1
<#
.SYNOPSIS
  Build, deploy, and batch-import POB files to verify SwgMayaEditor import path.

.EXAMPLE
  .\Smoke-ImportPob.ps1
  .\Smoke-ImportPob.ps1 -SkipBuild
#>
param(
    [switch] $SkipBuild,
    [string] $MayaBin = "C:\Program Files\Autodesk\Maya2027\bin",
    [string] $PluginDir = "C:\Program Files\Autodesk\Maya2027\bin\plug-ins",
    [string] $DataRoot = "D:/titan/data/sku.0/sys.client/compiled/game/appearance"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $repoRoot "build-mayamodern.ps1"
$builtMll = Join-Path $repoRoot "build\Release\SwgMayaEditor.mll"
$traceLog = Join-Path $env:TEMP "SwgMayaEditor-import-smoke.log"

$pobCases = @(
    @{ Name = "small-house"; Path = "$DataRoot/ply_all_house_sm_s01_fp2.pob" },
    @{ Name = "window-hue-v4"; Path = "$DataRoot/ply_tato_house_sm_s01_window_hue.pob" }
)

function Write-Step([string] $Message)
{
    Write-Host ""
    Write-Host "== $Message =="
}

function Stop-MayaIfRunning()
{
    $procs = Get-Process -Name "maya", "mayabatch" -ErrorAction SilentlyContinue
    if ($procs)
    {
        Write-Step "Stopping Maya processes for plugin rebuild"
        $procs | Stop-Process -Force
        Start-Sleep -Seconds 2
    }
}

function Get-LastTraceSession([string] $LogPath, [string] $PobPath)
{
    $lines = Get-Content -LiteralPath $LogPath
    $sessionStart = -1
    for ($i = $lines.Count - 1; $i -ge 0; $i--)
    {
        if ($lines[$i] -match '======== SwgMayaEditor import trace ========')
        {
            if ($sessionStart -lt 0)
            {
                $sessionStart = $i
            }
            elseif ($lines[$i + 2] -match [regex]::Escape($PobPath))
            {
                return $lines[$i..($sessionStart - 1)]
            }
            else
            {
                $sessionStart = $i
            }
        }
    }

    if ($sessionStart -ge 0)
    {
        return $lines[$sessionStart..($lines.Count - 1)]
    }

    return @()
}

function Test-TraceSuccess([string] $LogPath, [string] $PobPath)
{
    if (-not (Test-Path -LiteralPath $LogPath))
    {
        return @{ Ok = $false; Reason = "trace log missing: $LogPath" }
    }

    $session = Get-LastTraceSession -LogPath $LogPath -PobPath ($PobPath -replace '\\','/')
    if ($session.Count -eq 0)
    {
        return @{ Ok = $false; Reason = "trace log has no session for $PobPath" }
    }

    $text = $session -join "`n"
    $required = @(
        "importPob complete",
        "importPob returning",
        "import refresh unsuspend OK",
        "PobTranslator::reader doIt OK",
        "PobTranslator::reader returning"
    )

    $missing = @()
    foreach ($stage in $required)
    {
        if ($text -notmatch [regex]::Escape($stage))
        {
            $missing += $stage
        }
    }

    if ($missing.Count -gt 0)
    {
        return @{ Ok = $false; Reason = "missing stages: $($missing -join ', ')" }
    }

    return @{ Ok = $true; Reason = "all required stages present" }
}

Write-Step "SwgMayaEditor POB import smoke test"
Write-Host "Trace log: $traceLog"

if (-not $SkipBuild)
{
    Stop-MayaIfRunning
    Write-Step "Building SwgMayaEditor"
    Set-Location $repoRoot
    & $buildScript -Shallow
    if ($LASTEXITCODE -ne 0)
    {
        throw "build-mayamodern.ps1 failed with exit code $LASTEXITCODE"
    }

    if (-not (Test-Path -LiteralPath $builtMll))
    {
        throw "built plugin not found: $builtMll"
    }

    Write-Step "Deploying plugin"
    Copy-Item -LiteralPath $builtMll -Destination (Join-Path $PluginDir "SwgMayaEditor.mll") -Force
}

$mayabatch = Join-Path $MayaBin "mayabatch.exe"
if (-not (Test-Path -LiteralPath $mayabatch))
{
    throw "mayabatch not found: $mayabatch"
}

$pluginPath = Join-Path $PluginDir "SwgMayaEditor.mll"
if (-not (Test-Path -LiteralPath $pluginPath))
{
    throw "deployed plugin not found: $pluginPath"
}

if (Test-Path -LiteralPath $traceLog)
{
    Remove-Item -LiteralPath $traceLog -Force
}

$failures = @()
$passed = 0

foreach ($case in $pobCases)
{
    if (-not (Test-Path -LiteralPath $case.Path))
    {
        $failures += "$($case.Name): POB missing at $($case.Path)"
        continue
    }

    Write-Step "Batch import: $($case.Name)"
    Write-Host $case.Path

    if (Test-Path -LiteralPath $traceLog)
    {
        Remove-Item -LiteralPath $traceLog -Force
    }

    $mel = @"
loadPlugin "$($pluginPath -replace '\\','/')";
file -import -type "SwgPob" -ra true "$($case.Path -replace '\\','/')";
quit -force;
"@

    $melFile = Join-Path $env:TEMP "SwgMayaEditor-smoke-import.mel"
    Set-Content -LiteralPath $melFile -Value $mel -Encoding ASCII

    $env:SWG_IMPORT_TRACE_LOG = $traceLog

    $batchLog = Join-Path $env:TEMP "SwgMayaEditor-smoke-mayabatch.log"

    & $mayabatch -batch -noAutoloadPlugins -log $batchLog -script $melFile
    $exitCode = $LASTEXITCODE

    $result = Test-TraceSuccess -LogPath $traceLog -PobPath ($case.Path -replace '\\','/')
    if (-not $result.Ok)
    {
        $failures += "$($case.Name): $($result.Reason)"
        if ($exitCode -ne 0)
        {
            $failures += "$($case.Name): mayabatch exit code $exitCode"
        }
    }
    elseif ($exitCode -ne 0)
    {
        Write-Host "WARN $($case.Name): mayabatch exit code $exitCode (trace stages OK)"
        $passed++
        Write-Host "PASS $($case.Name): $($result.Reason)"
    }
    else
    {
        $passed++
        Write-Host "PASS $($case.Name): $($result.Reason)"
    }
}

Write-Step "Summary"
Write-Host "Passed: $passed / $($pobCases.Count)"
if ($failures.Count -gt 0)
{
    Write-Host "Failures:"
    foreach ($f in $failures) { Write-Host "  - $f" }
    Write-Host ""
    Write-Host "Tail trace log:"
    if (Test-Path -LiteralPath $traceLog)
    {
        Get-Content -LiteralPath $traceLog -Tail 30
    }
    exit 1
}

Write-Host "Smoke test PASSED"
exit 0
