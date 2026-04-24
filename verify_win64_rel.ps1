#requires -version 5.1
<#
.SYNOPSIS
  Verifies every PE32+ (AMD64) EXE/DLL in ..\exe\win64_rel. Flags 0x14C (i386) which causes 0xC000007B.
.NOTES
  Run from: D:\titan\client\verify_win64_rel.ps1
#>
$ErrorActionPreference = 'Stop'
# Script lives in <Titan>\client; staging is <Titan>\exe\win64_rel
$root = Split-Path $PSScriptRoot -Parent
$dir = Join-Path $root 'exe\win64_rel'
if (-not (Test-Path -LiteralPath $dir)) {
	Write-Error "Folder not found: $dir  (run bundle_win64_release.ps1 or build + SwgClient PostBuild first)"
}
function Get-PeMachine {
	param([string] $FilePath)
	$fs = [System.IO.File]::OpenRead($FilePath)
	try {
		$br = New-Object System.IO.BinaryReader($fs)
		[void]$fs.Seek(0x3C, [System.IO.SeekOrigin]::Begin)
		$e_lfanew = $br.ReadInt32()
		[void]$fs.Seek($e_lfanew, [System.IO.SeekOrigin]::Begin)
		$sig = $br.ReadUInt32()
		if ($sig -ne 0x00004550) { return 0xFFFF } # "PE\0\0" expected at optional header
		$machine = $br.ReadUInt16()
		return [int]$machine
	} finally {
		$br.Dispose()
		$fs.Dispose()
	}
}
# IMAGE_FILE_MACHINE_AMD64=0x8664, I386=0x14C
$files = @(
	Get-ChildItem -LiteralPath $dir -File -ErrorAction SilentlyContinue |
		Where-Object { $_.Extension -match '^\.(exe|dll)$' }
)
if ($files.Count -eq 0) {
	Write-Warning "No .exe or .dll in: $dir"
	exit 0
}
$bad = @()
foreach ($f in $files) {
	$m = Get-PeMachine $f.FullName
	$tag = switch ($m) {
		0x8664 { 'AMD64' }
		0x14C  { 'x86' }
		0xAA64 { 'ARM64' }
		default { "0x$('{0:X4}' -f $m)" }
	}
	if ($m -ne 0x8664) {
		$bad += [pscustomobject]@{ Name = $f.Name; Machine = $tag }
		Write-Warning "Non-x64: $($f.Name)  machine=$tag"
	} else {
		Write-Verbose "OK: $($f.Name)  $tag" 
	}
}
if ($bad.Count -gt 0) {
	Write-Error "Found $($bad.Count) file(s) that are not AMD64. Replace with x64 builds or remove from $dir (common cause: copied win32 or plugin DLL)."
}
Write-Host "OK: $($files.Count) PE file(s) in $dir are AMD64 (PE32+)." -ForegroundColor Green
Write-Host "Note: subfolders (e.g. libVLC plugins) are not scanned; x86 DLLs there also cause 0xC000007B when loaded." -ForegroundColor DarkGray
