param(
    [ValidateSet('check', 'build', 'all')]
    [string]$Mode = 'all',
    [string]$Project = '',
    [switch]$KeepGoing
)

$ErrorActionPreference = 'Stop'

$demoRoot = Split-Path -Parent $PSScriptRoot
$devRoot = Join-Path $demoRoot 'release\projects\dev'
$xl = Join-Path $demoRoot 'release\xl.exe'
$outputRoot = Join-Path $demoRoot 'build\dev_project_audit'

if (-not (Test-Path -LiteralPath $xl -PathType Leaf)) {
    throw "xl.exe not found: $xl"
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

# Keep ordinary projects zero-config; list only required C sources and system libraries here.
$entryOverrides = @{
    xagent5000 = 'xflowsim.xl'
}
$companionSources = @{
    cmix = @('cmix_native.c')
    winui = @('winui_native.c')
}
$linkLibraries = @{
    winui = @('User32', 'Gdi32')
}
$fixtureProjects = @('xanalyze')

$results = [System.Collections.Generic.List[object]]::new()
$directories = @(Get-ChildItem -LiteralPath $devRoot -Directory | Sort-Object Name)
if ($Project -ne '') {
    $directories = @($directories | Where-Object Name -EQ $Project)
    if ($directories.Count -eq 0) {
        throw "unknown dev project: $Project"
    }
}

foreach ($directory in $directories) {
    $name = $directory.Name
    if ($fixtureProjects -contains $name) {
        $results.Add([pscustomobject]@{ Project=$name; Check='fixture'; Build='fixture'; Detail='analyzer probes' })
        continue
    }

    $entryName = if ($entryOverrides.ContainsKey($name)) { $entryOverrides[$name] } else { "$name.xl" }
    $entry = Join-Path $directory.FullName $entryName
    if (-not (Test-Path -LiteralPath $entry -PathType Leaf)) {
        $results.Add([pscustomobject]@{ Project=$name; Check='skip'; Build='skip'; Detail='no application entry' })
        continue
    }

    $checkStatus = '-'
    $buildStatus = '-'
    $detail = ''
    try {
        if ($Mode -eq 'check' -or $Mode -eq 'all') {
            & $xl check $entry
            if ($LASTEXITCODE -ne 0) {
                throw "check exit $LASTEXITCODE"
            }
            $checkStatus = 'ok'
        }

        if ($Mode -eq 'build' -or $Mode -eq 'all') {
            $arguments = [System.Collections.Generic.List[string]]::new()
            $arguments.Add('build')
            $arguments.Add($entry)
            if ($companionSources.ContainsKey($name)) {
                foreach ($sourceName in $companionSources[$name]) {
                    $arguments.Add((Join-Path $directory.FullName $sourceName))
                }
            }
            $arguments.Add('-o')
            $arguments.Add((Join-Path $outputRoot "$name.exe"))
            if ($linkLibraries.ContainsKey($name)) {
                foreach ($library in $linkLibraries[$name]) {
                    $arguments.Add('--link')
                    $arguments.Add($library)
                }
            }

            & $xl @arguments
            if ($LASTEXITCODE -ne 0) {
                throw "build exit $LASTEXITCODE"
            }
            $buildStatus = 'ok'
        }
    } catch {
        $detail = $_.Exception.Message
        if ($checkStatus -eq '-') { $checkStatus = 'fail' }
        if (($Mode -eq 'build' -or $Mode -eq 'all') -and $buildStatus -eq '-') { $buildStatus = 'fail' }
        if (-not $KeepGoing) {
            $results.Add([pscustomobject]@{ Project=$name; Check=$checkStatus; Build=$buildStatus; Detail=$detail })
            break
        }
    }

    $results.Add([pscustomobject]@{ Project=$name; Check=$checkStatus; Build=$buildStatus; Detail=$detail })
}

$results | Format-Table -AutoSize
$failed = @($results | Where-Object { $_.Check -eq 'fail' -or $_.Build -eq 'fail' })
$applications = @($results | Where-Object { $_.Check -ne 'skip' -and $_.Check -ne 'fixture' })
Write-Host "applications=$($applications.Count) failed=$($failed.Count) fixtures=$(@($results | Where-Object Check -EQ 'fixture').Count) skipped=$(@($results | Where-Object Check -EQ 'skip').Count)"

if ($failed.Count -ne 0) {
    exit 1
}
