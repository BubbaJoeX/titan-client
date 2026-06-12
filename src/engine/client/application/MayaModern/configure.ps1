# Configure MayaModern for VS Insiders + Maya 2027 devkit (x64)
# Run from MayaModern directory: .\configure.ps1

$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "build-mayamodern.ps1") @PSBoundParameters

Write-Host "`nOpen build\SwgMayaEditor.sln in VS Insiders to build, or run:"
Write-Host "  .\build-mayamodern.ps1 -Shallow" -ForegroundColor DarkGray
