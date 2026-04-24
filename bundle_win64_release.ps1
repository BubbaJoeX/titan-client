#requires -version 5.1
<#
.SYNOPSIS
  Stages 64-bit SwgTitan (and optional tools) with matching *_r.dll and known vendor DLLs
  into <titan>\exe\win64_rel. Avoids mixing 32-bit DLLs (0xc000007b).

.DESCRIPTION
  - Copies client/src/compile/x64/**/<Configuration>/*_r.dll
  - Copies SwgTitan_r.exe, dpvs.dll (if built)
  - Copies x64 OpenAL Soft + libsndfile when present under external/3rd
  - Optional: SwgGodClient_r.exe

  Run after MSBuild SwgClient (and dpvs, if you need that DLL) for Release|x64.

.PARAMETER TitanRoot
  Root of the titan tree (parent of the client directory). Default: parent of the folder
  that contains this script (expects script at client/bundle_win64_release.ps1).

.PARAMETER Configuration
  MSBuild project output folder: Release, Debug, or Optimized. Default: Release.

.EXAMPLE
  cd D:\titan\client
  .\bundle_win64_release.ps1 -Verbose

.EXAMPLE
  .\bundle_win64_release.ps1 -TitanRoot D:\titan -Configuration Release -IncludeGodClient
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
	[string] $TitanRoot,
	[ValidateSet('Release', 'Debug', 'Optimized')]
	[string] $Configuration = 'Release',
	[switch] $IncludeGodClient
)

$ErrorActionPreference = 'Stop'

$clientDir = if ($TitanRoot) { Join-Path $TitanRoot 'client' } else { $PSScriptRoot }
if (-not $TitanRoot) { $TitanRoot = Split-Path $clientDir -Parent }
$sln = Join-Path $clientDir 'src\build\win32\swg.sln'
if (-not (Test-Path -LiteralPath $sln -PathType Leaf)) {
	Write-Error "Could not find client tree at ``$clientDir``. Set -TitanRoot to the titan root (the folder that contains a ``client`` directory with ``src\build\win32\swg.sln``)."
}
$outDir = Join-Path $TitanRoot 'exe\win64_rel'
$srcX64 = Join-Path $clientDir 'src\compile\x64'
$ext3 = Join-Path $clientDir 'src\external\3rd\library'

if ($PSCmdlet.ShouldProcess($outDir, 'Create output directory')) {
	New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

function Copy-IfExists {
	param([string] $Path, [string] $Message)
	if (Test-Path -LiteralPath $Path) {
		$name = Split-Path $Path -Leaf
		$dest = Join-Path $outDir $name
		if ($PSCmdlet.ShouldProcess($Path, "Copy to $dest")) {
			Copy-Item -LiteralPath $Path -Destination $dest -Force
			Write-Verbose "Copied $Message : $name"
		}
	} else {
		Write-Warning "Missing (skipped): $Path"
	}
}

# --- Main game client ---
$swgExe = Join-Path $srcX64 "SwgTitan\$Configuration\SwgTitan_r.exe"
Copy-IfExists $swgExe 'SwgTitan_r.exe'

# --- In-tree x64 build outputs: *_r.dll (only the chosen build configuration folder) ---
$dlls = @()
if (Test-Path -LiteralPath $srcX64) {
	$dirGlob = [IO.Path]::DirectorySeparatorChar + $Configuration + [IO.Path]::DirectorySeparatorChar
	$dlls = Get-ChildItem -Path $srcX64 -Recurse -File -Filter '*_r.dll' -ErrorAction SilentlyContinue |
		Where-Object { $_.DirectoryName.Contains($dirGlob) }
}
foreach ($d in $dlls) {
	$rel = $d.FullName.Substring($srcX64.Length).TrimStart('\')
	if ($PSCmdlet.ShouldProcess($d.FullName, "Copy to $outDir ($rel)")) {
		Copy-Item -LiteralPath $d.FullName -Destination (Join-Path $outDir $d.Name) -Force
	}
}
if ($dlls.Count -gt 0) { Write-Verbose "Copied $($dlls.Count) file(s) matching *\_r.dll under ...\x64\...\$Configuration\ ..." }

# --- dpvs: linked as dpvs.dll (no _r suffix) ---
Copy-IfExists (Join-Path $srcX64 "dpvs\$Configuration\dpvs.dll") 'dpvs.dll'

# --- Vendor runtime DLLs (x64) — paths match X64_MIGRATION / SwgClient PostBuild ---
$openalRelease = Join-Path $ext3 'openal-soft\lib\x64\Release\OpenAL32.dll'
$openalDebug   = Join-Path $ext3 'openal-soft\lib\x64\Debug\OpenAL32.dll'
$sfRelease     = Join-Path $ext3 'libsndfile\lib\x64\Release\sndfile.dll'
$sfDebug       = Join-Path $ext3 'libsndfile\lib\x64\Debug\sndfile.dll'
if ($Configuration -eq 'Release') {
	Copy-IfExists $openalRelease 'OpenAL32.dll'
	Copy-IfExists $sfRelease    'libsndfile'
} else {
	# Debug / Optimized: prefer /MTd vendor drop folders; fall back to Release
	if (Test-Path -LiteralPath $openalDebug) { Copy-IfExists $openalDebug 'OpenAL32.dll' } else { Copy-IfExists $openalRelease 'OpenAL32.dll (Release fallback)' }
	if (Test-Path -LiteralPath $sfDebug)     { Copy-IfExists $sfDebug     'libsndfile' }   else { Copy-IfExists $sfRelease    'libsndfile (Release fallback)' }
}

# --- libVLC (x64, LGPL): drop libvlc.dll, libvlccore.dll, and plugins/ into client\src\external\3rd\library\vlc-3.0.22\ from a VideoLAN 3.x win64 build ---
$vlcRoot = Join-Path $ext3 'vlc-3.0.22'
Copy-IfExists (Join-Path $vlcRoot 'libvlc.dll') 'libvlc'
Copy-IfExists (Join-Path $vlcRoot 'libvlccore.dll') 'libvlccore'
$vlcPlugins = Join-Path $vlcRoot 'plugins'
if (Test-Path -LiteralPath $vlcPlugins) {
	$destPlug = Join-Path $outDir 'plugins'
	if ($PSCmdlet.ShouldProcess($destPlug, 'Copy libVLC plugins')) {
		New-Item -ItemType Directory -Path $destPlug -Force | Out-Null
		Copy-Item -Path (Join-Path $vlcPlugins '*') -Destination $destPlug -Recurse -Force
		Write-Verbose 'Copied libVLC plugins folder'
	}
}

# --- Optional second EXE ---
if ($IncludeGodClient) {
	Copy-IfExists (Join-Path $srcX64 "SwgGodClient\$Configuration\SwgGodClient_r.exe") 'SwgGodClient'
}

# --- Summary ---
$exe = Get-ChildItem -Path $outDir -Filter '*.exe' -File -ErrorAction SilentlyContinue
$countDll = (Get-ChildItem -Path $outDir -Filter '*.dll' -File -ErrorAction SilentlyContinue | Measure-Object).Count
Write-Host "Staged: $($exe.Name -join ', ') and $countDll DLL(s) under"
Write-Host "  $outDir"
