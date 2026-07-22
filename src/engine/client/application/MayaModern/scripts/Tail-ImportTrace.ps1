#requires -Version 5.1
<#
.SYNOPSIS
  Tail SwgMayaEditor import trace log while Maya is running (or frozen).

.DESCRIPTION
  Logs flush on every line to %TEMP%\SwgMayaEditor-import.log even when Maya hangs.
  Run this in a separate PowerShell window BEFORE importing a POB.

.EXAMPLE
  .\Tail-ImportTrace.ps1
#>
param(
    [int] $Tail = 40,
    [string] $LogPath = ""
)

if (-not $LogPath) {
    $envPath = $env:SWG_IMPORT_TRACE_LOG
    if ($envPath) {
        $LogPath = $envPath
    }
    else {
        $LogPath = Join-Path $env:TEMP "SwgMayaEditor-import.log"
    }
}

Write-Host "Tailing import trace: $LogPath"
Write-Host "Press Ctrl+C to stop."
Write-Host ""

if (-not (Test-Path -LiteralPath $LogPath)) {
    Write-Host "Log not created yet — start Maya import, then lines will appear here."
}

Get-Content -LiteralPath $LogPath -Wait -Tail $Tail
