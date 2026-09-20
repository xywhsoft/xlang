param(
    [Parameter(Mandatory = $true)][string]$ManifestPath,
    [string]$Root = '',
    [string]$ReportPath = '',
    [ValidateRange(1, 8)][int]$MaxParallel = 1,
    [switch]$ListOnly
)

# Discovery batches collect independent failures; they do not waive them.
# Legacy gates remain exclusive. Version 2 permits reviewed resource declarations
# and named arguments; only gates with disjoint resources may overlap.
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
if (![IO.Path]::IsPathRooted($ManifestPath)) { $ManifestPath = Join-Path $Root $ManifestPath }
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if ($manifest.SchemaVersion -notin @(1, 2) -or @($manifest.Gates).Count -eq 0) {
    throw 'An XLIR batch requires SchemaVersion 1 or 2 and a nonempty Gates list.'
}
$names = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$gates = @($manifest.Gates | ForEach-Object {
    $definition = $_
    $name = if ($manifest.SchemaVersion -eq 1) { [string]$_ } else { [string]$_.Script }
    if ($name -notmatch '^check_xlir_[A-Za-z0-9_]+\.ps1$' -or !$names.Add($name)) {
        throw "Invalid or duplicate XLIR batch gate: $name"
    }
    $path = Join-Path $Root "tools/$name"
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing XLIR batch gate: $name" }
    $parameters = [ordered]@{}
    $resources = @('*')
    if ($manifest.SchemaVersion -eq 2) {
        $resources = @($definition.Resources)
        if (!$resources.Count -or @($resources | Where-Object { $_ -isnot [string] -or $_ -notmatch '^(\*|[a-zA-Z0-9_:/.-]+)$' }).Count) {
            throw "Gate requires explicit reviewed output resources: $name"
        }
        $command = Get-Command -Name $path -ErrorAction Stop
        foreach ($argument in $definition.Arguments.PSObject.Properties) {
            if (!$command.Parameters.ContainsKey($argument.Name) -or
                ($argument.Value -isnot [string] -and $argument.Value -isnot [bool] -and $argument.Value -isnot [long] -and $argument.Value -isnot [int])) {
                throw "Unknown or nonscalar gate argument: $name / $($argument.Name)"
            }
            $parameters[$argument.Name] = $argument.Value
        }
    }
    $file = Get-Item -LiteralPath $path
    [pscustomobject]@{ Name=$file.Name; FullName=$file.FullName; BaseName=$file.BaseName; Arguments=$parameters; Resources=$resources }
})
if ($ListOnly) { $gates.Name; exit 0 }

function InputSnapshot {
    # Include compiler, test/host scripts, runtime sources and release dependencies.
    # Generated build outputs are excluded: they are products, not frozen inputs.
    $paths = @('src', 'test/cases', 'tools', 'lib', 'release/examples')
    $files = @(
        foreach ($relative in $paths) {
            $directory = Join-Path $Root $relative
            if (Test-Path -LiteralPath $directory -PathType Container) {
                Get-ChildItem -LiteralPath $directory -Recurse -File |
                    Where-Object { $_.Extension -in '.c','.h','.inc','.l','.y','.xl','.ps1','.bat','.json','.def','.lib','.a','.dll','.exe' }
            }
        }
        foreach ($relative in @('release/xl.dll','release/xlc.dll','release/xl.exe')) {
            $path = Join-Path $Root $relative
            if (Test-Path -LiteralPath $path -PathType Leaf) { Get-Item -LiteralPath $path }
        }
    ) | Sort-Object FullName -Unique
    $rows = @($files | ForEach-Object {
        $_.FullName.Substring($Root.Length + 1).Replace('\','/') + '|' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    })
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($rows -join "`n")))).Replace('-','') }
    finally { $sha.Dispose() }
    [ordered]@{ Sha256 = $hash; FileCount = $files.Count }
}

$runId = (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + [Guid]::NewGuid().ToString('N').Substring(0,8)
if ($ReportPath -eq '') { $ReportPath = "build/tmp/xlir_batch_$runId/results.json" }
if (![IO.Path]::IsPathRooted($ReportPath)) { $ReportPath = Join-Path $Root $ReportPath }
$ReportPath = [IO.Path]::GetFullPath($ReportPath)
if (Test-Path -LiteralPath $ReportPath) { throw "Refusing to overwrite batch evidence: $ReportPath" }
$logDirectory = $ReportPath + '.logs'
if (Test-Path -LiteralPath $logDirectory) { throw "Refusing to reuse batch logs: $logDirectory" }
[IO.Directory]::CreateDirectory((Join-Path $Root 'build/tmp')) | Out-Null
try {
    # The live handle, not the existence of a leftover file, owns this lock.
    $batchLock = [IO.File]::Open((Join-Path $Root 'build/tmp/xlir_batch.lock'),
        [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
} catch { throw 'Another batch owns this workspace; shared gate outputs cannot run concurrently.' }
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($ReportPath)) | Out-Null
[IO.Directory]::CreateDirectory($logDirectory) | Out-Null
$before = InputSnapshot
$runReport = [ordered]@{
    SchemaVersion = 1; Batch = $manifest.Name; ManifestPath = $ManifestPath
    ManifestSha256 = (Get-FileHash -LiteralPath $ManifestPath -Algorithm SHA256).Hash
    Root = $Root; StartedUtc = [DateTime]::UtcNow.ToString('o'); CompletedUtc = $null
    Status = 'running'; InputBefore = $before; InputAfter = $null
    Selected = $gates.Count; Passed = 0; Failed = 0; NotRun = $gates.Count
    MaxParallel = $MaxParallel; PeakRunning = 0
    ProcessHost = (Get-Process -Id $PID).Path; Results = @(); Error = $null
}
function SaveReport {
    $pending = $ReportPath + '.pending'
    [IO.File]::WriteAllText($pending, ($runReport | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    if (Test-Path -LiteralPath $ReportPath) { [IO.File]::Replace($pending, $ReportPath, [NullString]::Value) }
    else { [IO.File]::Move($pending, $ReportPath) }
}
SaveReport
$pendingGates = [Collections.Generic.List[int]]::new()
for ($index = 0; $index -lt $gates.Count; ++$index) { $pendingGates.Add($index) }
$running = [Collections.Generic.List[object]]::new()
$infrastructureFailed = $false
function CanStartGate($Gate) {
    foreach ($active in $running) {
        if ('*' -in $Gate.Resources -or '*' -in $active.Gate.Resources -or
            @($Gate.Resources | Where-Object { $_ -in $active.Gate.Resources }).Count) { return $false }
    }
    return $true
}
# Arguments travel as data, not shell fragments. Spaces, quotes and Unicode
# paths never pass through a string-built command line for the actual gate.
$invokeScript = Join-Path $logDirectory 'invoke.ps1'
[IO.File]::WriteAllText($invokeScript, @'
param([string]$Payload)
$ErrorActionPreference = 'Stop'
$job = Get-Content -LiteralPath $Payload -Raw | ConvertFrom-Json
$parameters = @{}
foreach ($item in $job.Arguments.PSObject.Properties) { $parameters[$item.Name] = $item.Value }
$global:LASTEXITCODE = 0
& $job.Script @parameters
# A script can intentionally handle a nonzero native probe and finish normally.
# Preserve explicit script failures, not a stale LASTEXITCODE from that probe.
if (!$?) { if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; exit 1 }
exit 0
'@, [Text.UTF8Encoding]::new($false))
try {
    while ($pendingGates.Count -or $running.Count) {
        for ($position = 0; $position -lt $pendingGates.Count -and $running.Count -lt $MaxParallel;) {
            $index = $pendingGates[$position]; $gate = $gates[$index]
            if (!(CanStartGate $gate)) { ++$position; continue }
            $pendingGates.RemoveAt($position)
            $prefix = Join-Path $logDirectory ('{0:D3}_{1}' -f ($index + 1), $gate.BaseName)
            $result = [ordered]@{
                Gate = $gate.Name; ScriptSha256 = (Get-FileHash -LiteralPath $gate.FullName -Algorithm SHA256).Hash
                Arguments = $gate.Arguments; Resources = $gate.Resources
                StartedUtc = [DateTime]::UtcNow.ToString('o'); CompletedUtc = $null
                Status = 'running'; ExitCode = $null; ProcessId = $null; DurationSeconds = $null
                Stdout = "$prefix.stdout.log"; Stderr = "$prefix.stderr.log"
            }
            $runReport.Results += $result
            SaveReport
            Write-Host "[$($index + 1)/$($gates.Count)] $($gate.Name)"
            $payloadPath = "$prefix.arguments.json"
            [IO.File]::WriteAllText($payloadPath, (@{Script=$gate.FullName; Arguments=$gate.Arguments} | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
            try {
                $process = Start-Process -FilePath $runReport.ProcessHost -WindowStyle Hidden -PassThru `
                    -WorkingDirectory $Root -ArgumentList @('-NoProfile','-NonInteractive','-File',('"' + $invokeScript + '"'),'-Payload',('"' + $payloadPath + '"')) `
                    -RedirectStandardOutput $result.Stdout -RedirectStandardError $result.Stderr
                $result.ProcessId = $process.Id
                $running.Add([pscustomobject]@{ Process=$process; Gate=$gate; Result=$result; Timer=[Diagnostics.Stopwatch]::StartNew() })
                $runReport.PeakRunning = [Math]::Max($runReport.PeakRunning, $running.Count)
            } catch {
                $result.Status = 'infrastructure_failed'; $result.CompletedUtc = [DateTime]::UtcNow.ToString('o')
                $runReport.Error = $_.Exception.Message; $infrastructureFailed = $true
            }
            SaveReport
        }
        for ($position = $running.Count - 1; $position -ge 0; --$position) {
            $active = $running[$position]
            if (!$active.Process.HasExited) { continue }
            $active.Process.WaitForExit(); $active.Process.Refresh(); $active.Timer.Stop()
            $result = $active.Result
            $result.ExitCode = $active.Process.ExitCode
            if ($null -eq $result.ExitCode) { throw 'Exited gate has no observable exit code' }
            $result.CompletedUtc = [DateTime]::UtcNow.ToString('o')
            $result.DurationSeconds = [Math]::Round($active.Timer.Elapsed.TotalSeconds, 3)
            $result.Status = if ($result.ExitCode -eq 0) { 'passed' } else { 'failed' }
            if ($result.ExitCode -eq 0) { ++$runReport.Passed } else { ++$runReport.Failed }
            --$runReport.NotRun
            $active.Process.Dispose(); $running.RemoveAt($position)
            SaveReport
            Write-Host "  $($result.Gate): $($result.Status), exit=$($result.ExitCode), $($result.DurationSeconds)s"
        }
        if ($running.Count) { Start-Sleep -Milliseconds 100 }
    }
    $runReport.Status = if ($infrastructureFailed) { 'infrastructure_failed' } elseif ($runReport.Failed -eq 0) { 'passed' } else { 'failed' }
} catch {
    $runReport.Status = 'infrastructure_failed'
    $runReport.Error = $_.Exception.Message
} finally {
    foreach ($active in $running) {
        if (!$active.Process.HasExited) { $active.Process.Kill($true); $active.Process.WaitForExit() }
        $active.Result.Status = 'interrupted'; $active.Result.CompletedUtc = [DateTime]::UtcNow.ToString('o')
        $active.Process.Dispose()
    }
    try {
        $runReport.InputAfter = InputSnapshot
        if ($runReport.InputAfter.Sha256 -ne $runReport.InputBefore.Sha256 -or
            (Get-FileHash -LiteralPath $ManifestPath -Algorithm SHA256).Hash -ne $runReport.ManifestSha256) {
            $runReport.Status = 'invalidated'
            $runReport.Error = 'Batch inputs changed during verification; results cannot establish a single-source pass.'
        }
    } catch {
        $runReport.Status = 'invalidated'
        $runReport.Error = 'Cannot verify final input snapshot: ' + $_.Exception.Message
    }
    $runReport.CompletedUtc = [DateTime]::UtcNow.ToString('o')
    SaveReport
    $batchLock.Dispose()
}
Write-Host "XLIR batch $($runReport.Status): $($runReport.Passed) passed, $($runReport.Failed) failed, $($runReport.NotRun) not run."
Write-Host "Report: $ReportPath"
if ($runReport.Status -ne 'passed') { exit 1 }
exit 0
