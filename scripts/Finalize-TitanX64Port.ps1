[CmdletBinding()]
param(
    [string]$SourceRoot = "D:\titan\gr\client-tools",
    [string]$TargetRoot = "D:\titan\client"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-BlockRange {
    param(
        [Parameter(Mandatory)][string[]]$Lines,
        [Parameter(Mandatory)][string]$Marker
    )

    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -notlike "*<ItemDefinitionGroup Condition=*$Marker*") {
            continue
        }

        $depth = 0
        for ($j = $i; $j -lt $Lines.Count; $j++) {
            if ($Lines[$j] -match '<ItemDefinitionGroup\b') { $depth++ }
            if ($Lines[$j] -match '</ItemDefinitionGroup>') {
                $depth--
                if ($depth -eq 0) {
                    return @{ Start = $i; End = $j }
                }
            }
        }
    }

    return $null
}

function Merge-SwgClientProject {
    param(
        [string]$PortProjectPath,
        [string]$TitanBackupPath,
        [string]$DestinationPath
    )

    $portLines = [string[]](Get-Content -LiteralPath $PortProjectPath)
    $titanLines = [string[]](Get-Content -LiteralPath $TitanBackupPath)
    $merged = New-Object System.Collections.Generic.List[string]
    foreach ($line in $portLines) { [void]$merged.Add($line) }

    foreach ($platform in @('Debug|Win32', 'Optimized|Win32', 'Release|Win32')) {
        $portRange = Get-BlockRange -Lines $portLines -Marker $platform
        $titanRange = Get-BlockRange -Lines $titanLines -Marker $platform
        if ($null -eq $portRange -or $null -eq $titanRange) {
            throw "Missing ItemDefinitionGroup for $platform"
        }

        $replacement = @($titanLines[$titanRange.Start..$titanRange.End])
        $removeCount = $portRange.End - $portRange.Start + 1
        $merged.RemoveRange($portRange.Start, $removeCount)
        for ($k = 0; $k -lt $replacement.Count; $k++) {
            $merged.Insert($portRange.Start + $k, $replacement[$k])
        }

        # Recompute on updated merged list for subsequent platforms.
        $portLines = [string[]]$merged.ToArray()
    }

    $text = ($merged -join [Environment]::NewLine)
    if ($text -notmatch '<ProjectName>SwgTitan</ProjectName>') {
        $text = $text -replace '(<Keyword>Win32Proj</Keyword>)', "`$1`r`n    <ProjectName>SwgTitan</ProjectName>"
    }

    $text = $text.Replace('<TargetName>SwgClient_r</TargetName>', '<TargetName>SwgTitan_r</TargetName>')
    $text = $text.Replace('<TargetName>SwgClient_d</TargetName>', '<TargetName>SwgTitan_d</TargetName>')
    $text = $text.Replace('<TargetName>SwgClient_o</TargetName>', '<TargetName>SwgTitan_o</TargetName>')

    $outputLines = New-Object System.Collections.Generic.List[string]
    $forceWin32Toolset = $false
    foreach ($line in ($text -split "`r?`n")) {
        if ($line -like "*PropertyGroup Condition=*|Win32'*Label=`"Configuration`"*") {
            $forceWin32Toolset = $true
        }
        elseif ($forceWin32Toolset -and $line -like '*<PlatformToolset>v145</PlatformToolset>*') {
            $line = $line.Replace('v145', 'v120')
            $forceWin32Toolset = $false
        }
        elseif ($line -like '*</PropertyGroup>*') {
            $forceWin32Toolset = $false
        }

        [void]$outputLines.Add($line)
    }

    [IO.File]::WriteAllText($DestinationPath, (($outputLines -join [Environment]::NewLine) + [Environment]::NewLine))
}

function Merge-SolutionX64 {
    param(
        [string]$PortSolutionPath,
        [string]$TargetSolutionPath
    )

    $portLines = Get-Content -LiteralPath $PortSolutionPath
    $targetLines = New-Object System.Collections.Generic.List[string]
    foreach ($line in (Get-Content -LiteralPath $TargetSolutionPath)) { [void]$targetLines.Add($line) }

    $targetText = $targetLines -join [Environment]::NewLine
    if ($targetText -notmatch 'Debug\|x64 = Debug\|x64') {
        for ($i = 0; $i -lt $targetLines.Count; $i++) {
            if ($targetLines[$i] -match 'Release\|Win32 = Release\|Win32') {
                $targetLines.Insert($i + 1, "`t`tDebug|x64 = Debug|x64")
                $targetLines.Insert($i + 2, "`t`tOptimized|x64 = Optimized|x64")
                $targetLines.Insert($i + 3, "`t`tRelease|x64 = Release|x64")
                break
            }
        }
    }

    $x64Mappings = @{}
    foreach ($line in $portLines) {
        if ($line -match '^\s*(\{[0-9A-Fa-f-]+\})\.(Debug|Optimized|Release)\|x64\.(ActiveCfg|Build\.0)\s*=\s*(.+)$') {
            $guid = $Matches[1].ToUpperInvariant()
            if (-not $x64Mappings.ContainsKey($guid)) {
                $x64Mappings[$guid] = New-Object System.Collections.Generic.List[string]
            }
            [void]$x64Mappings[$guid].Add($line)
        }
    }

    $projectStart = -1
    for ($i = 0; $i -lt $targetLines.Count; $i++) {
        if ($targetLines[$i] -match 'GlobalSection\(ProjectConfigurationPlatforms\)') {
            $projectStart = $i
            break
        }
    }
    if ($projectStart -lt 0) {
        throw "Could not locate ProjectConfigurationPlatforms in $TargetSolutionPath"
    }

    $projectEnd = -1
    for ($i = $projectStart + 1; $i -lt $targetLines.Count; $i++) {
        if ($targetLines[$i] -match '^\s*EndGlobalSection') {
            $projectEnd = $i
            break
        }
    }
    $existing = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    for ($i = $projectStart + 1; $i -lt $projectEnd; $i++) {
        if ($targetLines[$i] -match '\|x64\.') {
            [void]$existing.Add($targetLines[$i].Trim())
        }
    }

    $insertionPoints = @{}
    for ($i = $projectStart + 1; $i -lt $projectEnd; $i++) {
        if ($targetLines[$i] -match '^\s*(\{[0-9A-Fa-f-]+\})\.Release\|Win32\.Build\.0\s*=') {
            $insertionPoints[$Matches[1].ToUpperInvariant()] = $i + 1
        }
    }

    $offset = 0
    foreach ($guid in ($insertionPoints.Keys | Sort-Object { $insertionPoints[$_] })) {
        if (-not $x64Mappings.ContainsKey($guid)) {
            continue
        }

        $linesToAdd = @()
        foreach ($line in $x64Mappings[$guid]) {
            if (-not $existing.Contains($line.Trim())) {
                $linesToAdd += $line
            }
        }

        if ($linesToAdd.Count -eq 0) {
            continue
        }

        $insertAt = $insertionPoints[$guid] + $offset
        for ($j = 0; $j -lt $linesToAdd.Count; $j++) {
            $targetLines.Insert($insertAt + $j, $linesToAdd[$j])
        }
        $offset += $linesToAdd.Count
    }

    [IO.File]::WriteAllLines($TargetSolutionPath, $targetLines)
}

function Update-BuildScriptsForTitan {
    param([string]$ScriptsRoot)

    foreach ($scriptName in @('Build-X64Client.ps1', 'Stage-X64Client.ps1')) {
        $path = Join-Path $ScriptsRoot $scriptName
        if (-not (Test-Path -LiteralPath $path)) {
            continue
        }

        $text = Get-Content -LiteralPath $path -Raw
        $text = $text.Replace('SwgClient_r.exe', 'SwgTitan_r.exe')
        $text = $text.Replace('SwgClient_o.exe', 'SwgTitan_o.exe')
        $text = $text.Replace('SwgClient_d.exe', 'SwgTitan_d.exe')
        [IO.File]::WriteAllText($path, $text)
    }
}

$targetSrc = Join-Path $TargetRoot "src"
$portSrc = Join-Path $SourceRoot "src"
$backupPath = Join-Path $TargetRoot "scripts\port-backups\SwgClient.vcxproj.titan-win32"
$swgClientProject = Join-Path $targetSrc "game\client\application\SwgClient\build\win32\SwgClient.vcxproj"
$portSwgClientProject = Join-Path $portSrc "game\client\application\SwgClient\build\win32\SwgClient.vcxproj"

if (-not (Test-Path -LiteralPath $backupPath)) {
    throw "Missing Titan Win32 backup. Run scripts\Import-X64Port.ps1 first."
}

Merge-SwgClientProject -PortProjectPath $portSwgClientProject -TitanBackupPath $backupPath -DestinationPath $swgClientProject

$buildTargets = Join-Path $TargetRoot "Directory.Build.targets"
$targetsText = Get-Content -LiteralPath $buildTargets -Raw
$targetsText = $targetsText.Replace(
    '<TargetName Condition="''$(MSBuildProjectName)'' == ''SwgClient''">SwgClient_$(SwgOutputSuffix)</TargetName>',
    '<TargetName Condition="''$(MSBuildProjectName)'' == ''SwgClient''">SwgTitan_$(SwgOutputSuffix)</TargetName>'
)
[IO.File]::WriteAllText($buildTargets, $targetsText)

Merge-SolutionX64 `
    -PortSolutionPath (Join-Path $portSrc "build\win32\swg.sln") `
    -TargetSolutionPath (Join-Path $targetSrc "build\win32\swg.sln")

Update-BuildScriptsForTitan -ScriptsRoot (Join-Path $TargetRoot "scripts")

Write-Host "Finalized Titan x64 port:"
Write-Host "  - SwgClient.vcxproj merged (Titan Win32 + port x64)"
Write-Host "  - Directory.Build.targets uses SwgTitan output names"
Write-Host "  - swg.sln x64 platform mappings merged"
Write-Host "  - Build/Stage scripts target SwgTitan_r.exe"
