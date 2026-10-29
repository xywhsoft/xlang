param(
    [string]$Root = '',
    [Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [int]$MinimumSourceCount = 531,
    [int]$MinimumXlirSuccess = 514,
    [int]$MinimumDefaultSuccess = 514,
    [ValidateRange(60, 600)][int]$CompilerTimeoutSeconds = 600,
    [string]$ReportPath = ''
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_public_cgen.ps1')

if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$outputPath = if ([IO.Path]::IsPathRooted($Directory)) {
    [IO.Path]::GetFullPath($Directory)
} else {
    [IO.Path]::GetFullPath((Join-Path $Root $Directory))
}
if (Test-Path -LiteralPath $outputPath) {
    throw "Refusing to reuse XLIR default-admission evidence: $outputPath"
}
[IO.Directory]::CreateDirectory($outputPath) | Out-Null
if (!$ReportPath) { $ReportPath = Join-Path $outputPath 'results.json' }
elseif (![IO.Path]::IsPathRooted($ReportPath)) { $ReportPath = Join-Path $Root $ReportPath }
$ReportPath = [IO.Path]::GetFullPath($ReportPath)
$null = Get-XlirContainedRelativePath -Base $outputPath -Path $ReportPath
if (Test-Path -LiteralPath $ReportPath) {
    throw "Refusing to overwrite XLIR default-admission report: $ReportPath"
}

$session = Open-XlirPublicCgen -Root $Root -PublicDirectory $PublicDirectory
$suiteDirectory = Join-Path $Root 'test\suites'
if (!(Test-Path -LiteralPath $suiteDirectory -PathType Container)) {
    throw "XLIR suite directory is missing: $suiteDirectory"
}
$suiteFiles = @(Get-ChildItem -LiteralPath $suiteDirectory -Filter '*.bat' -File | Sort-Object FullName)
$suiteSnapshot = @(New-XlirPublicCgenSnapshot -Paths @($suiteFiles | ForEach-Object { $_.FullName }))

$sources = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$generatedSources = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
# All production commands default to XLIR. Cgen-only extraction misses
# runtime/build-only fixtures and cannot prove the default backend's admission
# boundary. Code generation is sufficient here because suite hosts retain
# linkage and observable-runtime coverage.
$pattern = [regex]'(?i)release\\xl\.exe\s+(?:cgen|run|build)\s+(?:"([^"]+\.xl)"|([^\s>]+\.xl))'
$suiteFiles | ForEach-Object {
    foreach ($line in [IO.File]::ReadAllLines($_.FullName)) {
        if ($line -match '(?i)--backend\s+xlir') { continue }
        $match = $pattern.Match($line)
        if (!$match.Success) { continue }
        $source = if ($match.Groups[1].Success) { $match.Groups[1].Value } else { $match.Groups[2].Value }
        $source = $source.Replace('/', '\')
        if ($source -match '^(?i:build|tmp)\\') {
            [void]$generatedSources.Add($source)
            continue
        }
        $sourcePath = [IO.Path]::GetFullPath((Join-Path $Root $source))
        $relative = Get-XlirContainedRelativePath -Base $Root -Path $sourcePath
        if (Test-Path -LiteralPath $sourcePath -PathType Leaf) { [void]$sources.Add($relative) }
    }
}
$sourceSnapshot = @(New-XlirPublicCgenSnapshot -Paths @($sources | ForEach-Object {
    [IO.Path]::GetFullPath((Join-Path $Root $_))
}))

# Exercise the actual private ancestry queries against the independent child
# traversal reference. The same checked full source cohort supplies all other
# compiler/runtime units; the fixture includes xl6_ir.c exactly once.
$ancestryFixture = Join-Path $Root 'test/cases/xlir_cleanup_ancestry_contract.c'
$ancestryInputs = @(New-XlirPublicCgenSnapshot -Paths @($PSCommandPath, $ancestryFixture))
$ancestryObjects = @($session.Manifest.Objects | Where-Object {
    [IO.Path]::GetFileName($_.Path) -notin @('xl6_ir.o','xlir_public_source_driver.o')
} | ForEach-Object Path)
foreach ($name in @('xl6_ir.o','xlir_public_source_driver.o')) {
    if (@($session.Manifest.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq $name}).Count -ne 1) {
        throw "Missing or ambiguous cleanup ancestry source role: $name"
    }
}
$ancestryResults = @()
Copy-Item -LiteralPath (Join-Path $Root 'release/xl.dll') -Destination (Join-Path $outputPath 'xl.dll')
Push-Location $Root
try {
    foreach ($level in @(0,2)) {
        $prefix = Join-Path $outputPath "cleanup-ancestry-O$level"
        $arguments = @($session.Manifest.CompileFlags) + @("-O$level",$ancestryFixture) +
            $ancestryObjects + @((Join-Path $Root 'release/xl.dll'),'-lws2_32','-liphlpapi',
                '-lshell32','-lole32','-ladvapi32','-o',"$prefix.exe")
        $buildExit = Invoke-XlirGccToLog $arguments "$prefix.build.log"
        if ($buildExit) { throw 'Cleanup ancestry reference contract failed to build' }
        $runExit = Invoke-XlirNativeToLog "$prefix.exe" @() "$prefix.run.log"
        $marker = 'cleanup ancestry passed: trees=257 contains=1115236 lifetimes=1115236 malformed=6'
        $passed = $runExit -eq 0 -and (Select-String -LiteralPath "$prefix.run.log" -SimpleMatch $marker -Quiet)
        $ancestryResults += [pscustomobject]@{Level=$level;BuildExit=$buildExit;Exit=$runExit;Passed=[bool]$passed;
            Binary="$prefix.exe";Sha256=(Get-FileHash -LiteralPath "$prefix.exe").Hash;Log="$prefix.run.log"}
        if (!$passed) { throw 'Cleanup ancestry reference or malformed-tree contract failed' }
    }
} finally { Pop-Location }

$scratch = Join-Path $outputPath 'scratch'
[IO.Directory]::CreateDirectory($scratch) | Out-Null
$defaultSuccess = 0
$xlirSuccess = 0
$defaultMismatches = [System.Collections.Generic.List[object]]::new()
$executionFailures = [System.Collections.Generic.List[object]]::new()
$results = [System.Collections.Generic.List[object]]::new()

function Get-AdmissionDiagnostic([object]$Invocation) {
    $text = [string]$Invocation.Output
    if ($Invocation.Artifact) { $text = $text.Replace([string]$Invocation.Artifact, '<artifact>') }
    return $text.Trim()
}

$index = 0
# These complete successfully but exercise the largest task/callback graphs in
# an unoptimized source-built compiler. Keep them in one batch so the gate pays
# the measured CPU-bound tail once instead of serially by directory/name order.
$prioritySources = @(
    'release\examples\task_api.xl',
    'test\cases\cgen_task_api.xl',
    'test\cases\cgen_task_spawn.xl'
)
$orderedSources = @($sources | Sort-Object `
    @{ Expression = { if ($_ -in $prioritySources) { 0 } else { 1 } } },
    @{ Expression = { $_ } })
$sourceBatchSize = 4
for ($offset = 0; $offset -lt $orderedSources.Count; $offset += $sourceBatchSize) {
    $sourceBatch = @($orderedSources | Select-Object -Skip $offset -First $sourceBatchSize)
    $requests = [System.Collections.Generic.List[object]]::new()
    foreach ($source in $sourceBatch) {
        $slot = [int]($requests.Count / 2)
        $requests.Add([pscustomobject]@{ Source = $source; Backend = 'default'; Output = (Join-Path $scratch "slot$slot-default.c") })
        $requests.Add([pscustomobject]@{ Source = $source; Backend = 'xlir'; Output = (Join-Path $scratch "slot$slot-xlir.c") })
    }
    $batch = @(Invoke-XlirPublicCgenBatch -Session $session -Requests $requests.ToArray() `
        -TimeoutSeconds $CompilerTimeoutSeconds)
    if ($batch.Count -ne $sourceBatch.Count * 2) {
        throw 'XLIR default-admission batch returned an incomplete result set'
    }
    for ($sourceIndex = 0; $sourceIndex -lt $sourceBatch.Count; ++$sourceIndex) {
        ++$index
        $source = $sourceBatch[$sourceIndex]
        $default = $batch[$sourceIndex * 2]
        $xlir = $batch[$sourceIndex * 2 + 1]
    $sourceHashesMatch = $default.SourceSha256 -eq $xlir.SourceSha256
    if (!$sourceHashesMatch) {
        $executionFailures.Add([pscustomobject]@{
            Source = $source
            Backend = 'batch'
            Failure = 'backend invocations did not consume the same source revision'
            Diagnostic = ''
        })
    }
    foreach ($invocation in @($default, $xlir)) {
        if ($invocation.ExecutionFailure) {
            $executionFailures.Add([pscustomobject]@{
                Source = $source
                Backend = $invocation.Backend
                Failure = $invocation.ExecutionFailure
                Diagnostic = $invocation.Output
            })
        }
    }

    $defaultMatches = $default.ExecutionFailure -eq '' -and $xlir.ExecutionFailure -eq '' -and
        $default.ExitCode -eq $xlir.ExitCode
    if ($defaultMatches -and $default.ExitCode -eq 0) {
        $defaultMatches = $default.ArtifactSha256 -eq $xlir.ArtifactSha256
    } elseif ($defaultMatches -and $default.ExitCode -eq 1) {
        $defaultMatches = (Get-AdmissionDiagnostic $default) -eq (Get-AdmissionDiagnostic $xlir)
    }
    if (!$defaultMatches) {
        $defaultMismatches.Add([pscustomobject]@{
            Source = $source
            DefaultExitCode = $default.ExitCode
            XlirExitCode = $xlir.ExitCode
            DefaultSha256 = $default.ArtifactSha256
            XlirSha256 = $xlir.ArtifactSha256
            DefaultDiagnostic = $default.Output
            XlirDiagnostic = $xlir.Output
        })
    }
    $results.Add([pscustomobject]@{
        Source = $source
        SourceSha256 = $default.SourceSha256
        SourceHashesMatch = $sourceHashesMatch
        DefaultExitCode = $default.ExitCode
        XlirExitCode = $xlir.ExitCode
        DefaultMatchesXlir = $defaultMatches
        DefaultSha256 = $default.ArtifactSha256
        XlirSha256 = $xlir.ArtifactSha256
        DefaultMilliseconds = $default.DurationMilliseconds
        XlirMilliseconds = $xlir.DurationMilliseconds
    })
    if (!$default.ExecutionFailure -and $default.ExitCode -eq 0) { ++$defaultSuccess }
    if (!$xlir.ExecutionFailure -and $xlir.ExitCode -eq 0) { ++$xlirSuccess }
    if (($index % 50) -eq 0) {
        Write-Output "XLIR current-source default admission progress: $index / $($sources.Count)"
    }
    }
}

$inputsUnchanged = $true
$inputFailure = ''
try { Assert-XlirPublicCgenUnchanged -Session $session }
catch { $inputsUnchanged = $false; $inputFailure = $_.Exception.Message }
if ($inputsUnchanged) {
    try {
        Assert-XlirPublicCgenDirectoryInventoryUnchanged -Directory $suiteDirectory `
            -Filter '*.bat' -Snapshot $suiteSnapshot
        Assert-XlirPublicCgenSnapshotUnchanged -Snapshot $sourceSnapshot
        Assert-XlirPublicCgenSnapshotUnchanged -Snapshot $ancestryInputs
    } catch {
        $inputsUnchanged = $false
        $inputFailure = $_.Exception.Message
    }
}

$summary = 'XLIR current-source default admission: sources={0}, default-ok={1}, xlir-ok={2}, default-mismatches={3}' -f
    $sources.Count, $defaultSuccess, $xlirSuccess, $defaultMismatches.Count
$report = [ordered]@{
    SchemaVersion = 5
    RecordedAt = [DateTime]::UtcNow.ToString('o')
    Commands = @('cgen', 'run', 'build')
    SourceCount = $sources.Count
    DefaultSuccess = $defaultSuccess
    XlirSuccess = $xlirSuccess
    DefaultMismatchCount = $defaultMismatches.Count
    ExecutionFailureCount = $executionFailures.Count
    CompilerTimeoutSeconds = $CompilerTimeoutSeconds
    InputsUnchanged = $inputsUnchanged
    InputFailure = $inputFailure
    CompilerManifest = $session.ManifestPath
    CompilerManifestSha256 = $session.ManifestSha256
    Driver = $session.Driver
    DriverSha256 = $session.DriverSha256
    CompilerScope = $session.Manifest.Scope
    PrioritySources = $prioritySources
    CleanupAncestryInputs = $ancestryInputs
    CleanupAncestry = $ancestryResults
    SuiteInputs = @($suiteSnapshot | ForEach-Object {
        [ordered]@{ Path = Get-XlirContainedRelativePath -Base $Root -Path $_.Path; Sha256 = $_.Sha256 }
    })
    SourceInputs = @($sourceSnapshot | ForEach-Object {
        [ordered]@{ Path = Get-XlirContainedRelativePath -Base $Root -Path $_.Path; Sha256 = $_.Sha256 }
    })
    Sources = @($sources | Sort-Object)
    SuiteOwnedGeneratedSources = @($generatedSources | Sort-Object)
    DefaultMismatches = @($defaultMismatches.ToArray())
    ExecutionFailures = @($executionFailures.ToArray())
    Results = @($results.ToArray())
}
[IO.Directory]::CreateDirectory((Split-Path -Parent $ReportPath)) | Out-Null
[IO.File]::WriteAllText($ReportPath, ($report | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
Write-Output $summary
$defaultMismatches | Select-Object -First 10 | ForEach-Object {
    Write-Output ("  default mismatch: {0} [default={1}, xlir={2}]" -f
        $_.Source, $_.DefaultExitCode, $_.XlirExitCode)
}
$executionFailures | Select-Object -First 10 | ForEach-Object {
    Write-Output ("  execution failure: {0} [{1}, {2}]" -f $_.Failure, $_.Backend, $_.Source)
}

if (!$inputsUnchanged) { throw "XLIR default-admission evidence was invalidated: $inputFailure" }
if ($executionFailures.Count) { throw "XLIR default admission encountered $($executionFailures.Count) compiler execution failures" }
if ($defaultMismatches.Count) { throw "Default backend diverged from explicit XLIR for $($defaultMismatches.Count) sources" }
if ($sources.Count -lt $MinimumSourceCount) { throw "XLIR default admission source coverage regressed below $MinimumSourceCount" }
if ($defaultSuccess -lt $MinimumDefaultSuccess) { throw "XLIR default admission default success count regressed below $MinimumDefaultSuccess" }
if ($xlirSuccess -lt $MinimumXlirSuccess) { throw "XLIR default admission XLIR success count regressed below $MinimumXlirSuccess" }

exit 0
