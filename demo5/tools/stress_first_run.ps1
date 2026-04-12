param(
    [int]$Iterations = 5,
    [string]$PrimarySelector = "lower.async_generator_loop_control_hybrid_groups_json",
    [string]$SecondarySelector = "cgen.async_generator_try_catch_basic",
    [string]$ReportPath = "",
    [switch]$FailOnAnomaly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$releaseTmp = Join-Path $projectRoot "release\tmp"

if (-not (Test-Path $releaseTmp)) {
    New-Item -ItemType Directory -Path $releaseTmp | Out-Null
}

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $ReportPath = Join-Path $releaseTmp "first_run_stress_report.txt"
}

function Invoke-Selector {
    param(
        [string]$ExePath,
        [string[]]$Arguments
    )

    $output = & $ExePath @Arguments 2>&1 | Out-String
    $pass = $output -match "\[PASS\]"
    $noMatch = $output -match "no tests matched selector"
    $buildStampMatch = [regex]::Match($output, "build_stamp=(.+)")
    $testCountMatch = [regex]::Match($output, "test_count=(\d+)")

    [pscustomobject]@{
        Exe        = [System.IO.Path]::GetFileName($ExePath)
        Args       = ($Arguments -join " ")
        Pass       = $pass
        NoMatch    = $noMatch
        BuildStamp = $(if ($buildStampMatch.Success) { $buildStampMatch.Groups[1].Value.Trim() } else { "" })
        TestCount  = $(if ($testCountMatch.Success) { $testCountMatch.Groups[1].Value.Trim() } else { "" })
        Output     = $output.TrimEnd()
    }
}

function Get-BinarySnapshot {
    param(
        [string]$Path
    )

    $item = Get-Item $Path
    [pscustomobject]@{
        Name          = $item.Name
        Length        = $item.Length
        LastWriteTime = $item.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss.fff")
    }
}

$allResults = @()
$iterationDiagnostics = @()

for ($i = 1; $i -le $Iterations; $i++) {
    $buildLogPath = Join-Path $releaseTmp ("first_run_build_{0:00}.log" -f $i)
    & (Join-Path $projectRoot "build_gcc.bat") *> $buildLogPath
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed at iteration $i. See $buildLogPath"
    }

    $xl5Path = Join-Path $projectRoot "release\bin\xl5.exe"
    $xl5TestPath = Join-Path $projectRoot "release\bin\xl5_test.exe"
    $xl5Snapshot = Get-BinarySnapshot -Path $xl5Path
    $xl5TestSnapshot = Get-BinarySnapshot -Path $xl5TestPath

    $runResults = @(
        Invoke-Selector -ExePath $xl5Path -Arguments @("test", $PrimarySelector)
        Invoke-Selector -ExePath $xl5TestPath -Arguments @($PrimarySelector)
        Invoke-Selector -ExePath $xl5Path -Arguments @("test", $SecondarySelector)
        Invoke-Selector -ExePath $xl5TestPath -Arguments @($SecondarySelector)
    )

    $badResults = @($runResults | Where-Object { (-not $_.Pass) -or $_.NoMatch -or [string]::IsNullOrWhiteSpace($_.BuildStamp) -or [string]::IsNullOrWhiteSpace($_.TestCount) })
    $uniqueStamps = @($runResults | Select-Object -ExpandProperty BuildStamp -Unique)
    $uniqueCounts = @($runResults | Select-Object -ExpandProperty TestCount -Unique)
    $hasStampMismatch = ($uniqueStamps.Count -ne 1)
    $hasCountMismatch = ($uniqueCounts.Count -ne 1)

    $iterationDiagnostics += [pscustomobject]@{
        Iteration         = $i
        Xl5Length         = $xl5Snapshot.Length
        Xl5LastWriteTime  = $xl5Snapshot.LastWriteTime
        Xl5TestLength     = $xl5TestSnapshot.Length
        Xl5TestLastWrite  = $xl5TestSnapshot.LastWriteTime
        BuildStamp        = (($uniqueStamps | Where-Object { $_ -ne "" }) -join " | ")
        TestCount         = (($uniqueCounts | Where-Object { $_ -ne "" }) -join " | ")
        StampMismatch     = $hasStampMismatch
        CountMismatch     = $hasCountMismatch
        AnomalyCount      = $badResults.Count
        BuildLogPath      = $buildLogPath
    }

    foreach ($result in $runResults) {
        $allResults += [pscustomobject]@{
            Iteration  = $i
            Exe        = $result.Exe
            Args       = $result.Args
            Pass       = $result.Pass
            NoMatch    = $result.NoMatch
            BuildStamp = $result.BuildStamp
            TestCount  = $result.TestCount
            Output     = $result.Output
        }
    }

    if ($FailOnAnomaly -and (($badResults.Count -ne 0) -or $hasStampMismatch -or $hasCountMismatch)) {
        break
    }
}

$summary = $allResults |
    Select-Object Iteration, Exe, Args, Pass, NoMatch, BuildStamp, TestCount |
    Format-Table -AutoSize |
    Out-String

$iterationSummary = $iterationDiagnostics |
    Select-Object Iteration, Xl5Length, Xl5LastWriteTime, Xl5TestLength, Xl5TestLastWrite, BuildStamp, TestCount, StampMismatch, CountMismatch, AnomalyCount |
    Format-Table -AutoSize |
    Out-String

$details = $allResults |
    ForEach-Object {
        @(
            ("[{0}] {1} {2}" -f $_.Iteration, $_.Exe, $_.Args)
            ("pass={0} no_match={1} build_stamp={2} test_count={3}" -f $_.Pass, $_.NoMatch, $_.BuildStamp, $_.TestCount)
            $_.Output
            ""
        ) -join [Environment]::NewLine
    } |
    Out-String

$report = @(
    ("stress_first_run report")
    ("project_root={0}" -f $projectRoot)
    ("iterations={0}" -f $Iterations)
    ("primary_selector={0}" -f $PrimarySelector)
    ("secondary_selector={0}" -f $SecondarySelector)
    ("fail_on_anomaly={0}" -f $FailOnAnomaly.IsPresent)
    ""
    "[iteration_summary]"
    $iterationSummary.TrimEnd()
    ""
    "[summary]"
    $summary.TrimEnd()
    ""
    "[details]"
    $details.TrimEnd()
) -join [Environment]::NewLine

Set-Content -Path $ReportPath -Value $report

if ($FailOnAnomaly) {
    $hasAnyAnomaly = @($iterationDiagnostics | Where-Object { $_.StampMismatch -or $_.CountMismatch -or ($_.AnomalyCount -ne 0) }).Count -ne 0
    if ($hasAnyAnomaly) {
        Write-Error "Detected first-run anomaly. See $ReportPath"
        exit 1
    }
}

$summary
