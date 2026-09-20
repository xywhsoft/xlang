param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }
$Root = (Resolve-Path -LiteralPath $Root).Path
$runner = Join-Path $Root 'tools/run_xlir_batch.ps1'
$hostPath = (Get-Process -Id $PID).Path
$fixtureRoot = Join-Path $Root ('build/tmp/xlir batch runner ' + [Guid]::NewGuid().ToString('N'))
foreach ($directory in @('tools','src','build/tmp')) {
    [IO.Directory]::CreateDirectory((Join-Path $fixtureRoot $directory)) | Out-Null
}
function Fixture([string]$path, [string]$body) {
    [IO.File]::WriteAllText((Join-Path $fixtureRoot $path), $body, [Text.UTF8Encoding]::new($false))
}
function Require([bool]$condition, [string]$message) {
    if (!$condition) { throw $message }
}
Fixture 'src/probe.c' '/* immutable fixture input */'
Fixture 'tools/check_xlir_a_fail.ps1' 'Write-Output "first ran"; [Console]::Error.WriteLine("failure detail"); exit 7'
Fixture 'tools/check_xlir_b_pass.ps1' 'Write-Output "second ran"; exit 0'
Fixture 'tools/check_xlir_c_throw.ps1' 'throw "intentional gate exception"'
Fixture 'tools/check_xlir_d_mutate.ps1' '[IO.File]::AppendAllText((Join-Path (Get-Location).Path "src/probe.c"), "changed"); exit 0'
Fixture 'tools/check_xlir_handled_probe.ps1' '$global:LASTEXITCODE = 9; Write-Output "handled native failure; script succeeded"'
function RunBatch([string]$name, [string[]]$gates, [switch]$Reuse, [switch]$ListOnly) {
    $manifestPath = Join-Path $fixtureRoot "$name.manifest.json"
    if (!$Reuse) {
        Fixture "$name.manifest.json" (@{SchemaVersion=1; Name=$name; Gates=$gates} | ConvertTo-Json)
    }
    $arguments = @('-NoProfile','-NonInteractive','-File',('"' + $runner + '"'),
        '-Root',('"' + $fixtureRoot + '"'), '-ManifestPath',('"' + $manifestPath + '"'),
        '-ReportPath',('"' + (Join-Path $fixtureRoot "$name.report.json") + '"'))
    if ($ListOnly) { $arguments += '-ListOnly' }
    $suffix = if ($Reuse) { '.repeat' } elseif ($ListOnly) { '.list' } else { '' }
    $process = Start-Process -FilePath $hostPath -WindowStyle Hidden -Wait -PassThru -ArgumentList $arguments `
        -RedirectStandardOutput (Join-Path $fixtureRoot "$name$suffix.stdout.log") `
        -RedirectStandardError (Join-Path $fixtureRoot "$name$suffix.stderr.log")
    return $process.ExitCode
}
$all = @('check_xlir_a_fail.ps1','check_xlir_b_pass.ps1','check_xlir_c_throw.ps1')
Require ((RunBatch 'mixed' $all) -eq 1) 'A failed batch must return nonzero.'
$mixedPath = Join-Path $fixtureRoot 'mixed.report.json'
$mixed = Get-Content -LiteralPath $mixedPath -Raw | ConvertFrom-Json
Require ($mixed.Status -eq 'failed' -and $mixed.Passed -eq 1 -and $mixed.Failed -eq 2 -and $mixed.NotRun -eq 0) 'Independent gates must all execute after failures.'
Require ($mixed.Results.Count -eq 3 -and $mixed.Results[0].ExitCode -eq 7 -and $mixed.Results[1].ExitCode -eq 0 -and $mixed.Results[2].ExitCode -ne 0) 'Actual child exit codes must be preserved.'
Require ((Get-Content -LiteralPath $mixed.Results[0].Stderr -Raw).Contains('failure detail')) 'Per-gate stderr was lost.'
Require ((Get-Content -LiteralPath $mixed.Results[1].Stdout -Raw).Contains('second ran')) 'The post-failure gate did not execute.'
Require ($mixed.InputBefore.Sha256 -eq $mixed.InputAfter.Sha256) 'Stable inputs should retain their fingerprint.'
$savedHash = (Get-FileHash -LiteralPath $mixedPath).Hash
Require ((RunBatch 'mixed' $all -Reuse) -ne 0) 'Existing evidence must not be overwritten.'
Require ((Get-FileHash -LiteralPath $mixedPath).Hash -eq $savedHash) 'Repeat execution overwrote evidence.'
Require ((RunBatch 'pass' @('check_xlir_b_pass.ps1')) -eq 0) 'A passing batch must return zero.'
$passed = Get-Content -LiteralPath (Join-Path $fixtureRoot 'pass.report.json') -Raw | ConvertFrom-Json
Require ($passed.Status -eq 'passed' -and $passed.Passed -eq 1) 'Passing batch summary is wrong.'
Require ((RunBatch 'handled-probe' @('check_xlir_handled_probe.ps1')) -eq 0) 'A handled native failure must not replace the successful script outcome.'
Require ((RunBatch 'drift' @('check_xlir_d_mutate.ps1','check_xlir_b_pass.ps1')) -ne 0) 'Input drift must invalidate a nominal pass.'
$drift = Get-Content -LiteralPath (Join-Path $fixtureRoot 'drift.report.json') -Raw | ConvertFrom-Json
Require ($drift.Status -eq 'invalidated' -and $drift.InputBefore.Sha256 -ne $drift.InputAfter.Sha256) 'Input drift was not recorded.'
Require ((RunBatch 'duplicates' @('check_xlir_b_pass.ps1','check_xlir_b_pass.ps1')) -ne 0) 'Duplicate gates must be rejected.'
Require ((RunBatch 'missing' @('check_xlir_missing.ps1')) -ne 0) 'Missing gates must be rejected.'
Require ((RunBatch 'traversal' @('../check_xlir_b_pass.ps1')) -ne 0) 'Gate path traversal must be rejected.'
Require ((RunBatch 'empty' @()) -ne 0) 'An empty selection must not pass.'
Require ((RunBatch 'listing' @('check_xlir_b_pass.ps1') -ListOnly) -eq 0) 'ListOnly must succeed without execution.'
Require (!(Test-Path -LiteralPath (Join-Path $fixtureRoot 'listing.report.json'))) 'ListOnly must not create a run report.'
$lock = [IO.File]::Open((Join-Path $fixtureRoot 'build/tmp/xlir_batch.lock'),
    [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    Require ((RunBatch 'busy' @('check_xlir_b_pass.ps1')) -ne 0) 'A second batch must not share live outputs.'
    Require (!(Test-Path -LiteralPath (Join-Path $fixtureRoot 'busy.report.json'))) 'A blocked second batch must not execute.'
} finally { $lock.Dispose() }
Require ((RunBatch 'unlocked' @('check_xlir_b_pass.ps1')) -eq 0) 'A leftover lock file must not block a new batch.'
$parallelBody = @'
param([string]$Own, [string]$Peer, [string]$Payload, [bool]$Enabled)
$ErrorActionPreference = 'Stop'
if ($Payload -cne '空 格 "quoted" $literal; &' -or !$Enabled) { throw 'Arguments were reinterpreted' }
[IO.File]::WriteAllText($Own, 'ready')
$deadline = [DateTime]::UtcNow.AddSeconds(8)
while (!(Test-Path -LiteralPath $Peer)) {
    if ([DateTime]::UtcNow -gt $deadline) { throw 'Independent jobs did not overlap' }
    Start-Sleep -Milliseconds 20
}
Write-Output 'observed live peer'
exit 0
'@
Fixture 'tools/check_xlir_parallel_a.ps1' $parallelBody
Fixture 'tools/check_xlir_parallel_b.ps1' $parallelBody
$serialBody = @'
param([string]$Lease)
$ErrorActionPreference = 'Stop'
$held = [IO.File]::Open($Lease, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try { Start-Sleep -Milliseconds 200 } finally { $held.Dispose() }
exit 0
'@
Fixture 'tools/check_xlir_serial_a.ps1' $serialBody
Fixture 'tools/check_xlir_serial_b.ps1' $serialBody
function RunV2([string]$Name, [object[]]$Gates) {
    Fixture "$Name.manifest.json" (@{SchemaVersion=2; Name=$Name; Gates=$Gates} | ConvertTo-Json -Depth 6)
    $arguments = @('-NoProfile','-NonInteractive','-File',('"' + $runner + '"'),
        '-Root',('"' + $fixtureRoot + '"'), '-ManifestPath',('"' + (Join-Path $fixtureRoot "$Name.manifest.json") + '"'),
        '-ReportPath',('"' + (Join-Path $fixtureRoot "$Name.report.json") + '"'), '-MaxParallel','2')
    $process = Start-Process -FilePath $hostPath -WindowStyle Hidden -Wait -PassThru -ArgumentList $arguments `
        -RedirectStandardOutput (Join-Path $fixtureRoot "$Name.stdout.log") `
        -RedirectStandardError (Join-Path $fixtureRoot "$Name.stderr.log")
    return $process.ExitCode
}
$a = Join-Path $fixtureRoot 'build/tmp/a.ready'; $b = Join-Path $fixtureRoot 'build/tmp/b.ready'
$parallel = @(
    @{Script='check_xlir_parallel_a.ps1'; Resources=@('private-a'); Arguments=@{Own=$a;Peer=$b;Payload='空 格 "quoted" $literal; &';Enabled=$true}},
    @{Script='check_xlir_parallel_b.ps1'; Resources=@('private-b'); Arguments=@{Own=$b;Peer=$a;Payload='空 格 "quoted" $literal; &';Enabled=$true}},
    @{Script='check_xlir_a_fail.ps1'; Resources=@('private-c'); Arguments=@{}})
Require ((RunV2 'parallel' $parallel) -eq 1) 'A parallel batch must preserve the failing gate exit'
$parallelReport = Get-Content -LiteralPath (Join-Path $fixtureRoot 'parallel.report.json') -Raw | ConvertFrom-Json
Require ($parallelReport.PeakRunning -eq 2 -and $parallelReport.Passed -eq 2 -and $parallelReport.Failed -eq 1 -and $parallelReport.NotRun -eq 0) 'Disjoint jobs must overlap, collect all failures and preserve data arguments'
foreach ($resource in @('same-output', '*')) {
    $serial = @(
        @{Script='check_xlir_serial_a.ps1'; Resources=@($resource); Arguments=@{Lease=(Join-Path $fixtureRoot 'build/tmp/shared.lock')}},
        @{Script='check_xlir_serial_b.ps1'; Resources=@('same-output'); Arguments=@{Lease=(Join-Path $fixtureRoot 'build/tmp/shared.lock')}})
    $name = if ($resource -eq '*') {'exclusive-v2'} else {'shared-v2'}
    Require ((RunV2 $name $serial) -eq 0) 'Overlapping resource declarations must serialize jobs'
    $report = Get-Content -LiteralPath (Join-Path $fixtureRoot "$name.report.json") -Raw | ConvertFrom-Json
    Require ($report.PeakRunning -eq 1 -and $report.Passed -eq 2) 'Shared/wildcard resources must never overlap'
}
Require ((RunV2 'unknown-argument' @(@{Script='check_xlir_serial_a.ps1';Resources=@('private');Arguments=@{Unknown='value'}})) -ne 0) 'Unknown arguments must fail before scheduling'
Require ((RunV2 'undeclared-resources' @(@{Script='check_xlir_serial_a.ps1';Resources=@();Arguments=@{}})) -ne 0) 'Parallel safety must be explicit, never inferred from an empty resource set'
Write-Output "XLIR batch runner passed: original collect-all/frozen-input/evidence/exit/lock contracts, plus real disjoint overlap, shared-resource exclusion, data arguments and parallel failure collection. Evidence: $fixtureRoot"
exit 0
