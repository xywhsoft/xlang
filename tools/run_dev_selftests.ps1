param(
    [string]$Project = '',
    [int]$TimeoutSeconds = 180,
    [switch]$KeepGoing
)

$ErrorActionPreference = 'Stop'

$demoRoot = Split-Path -Parent $PSScriptRoot
$releaseRoot = Join-Path $demoRoot 'release'
$devRoot = Join-Path $releaseRoot 'projects\dev'
$xl = Join-Path $releaseRoot 'xl.exe'
$outputRoot = Join-Path $demoRoot 'build\dev_project_selftests'

if (-not (Test-Path -LiteralPath $xl -PathType Leaf)) {
    throw "xl.exe not found: $xl"
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

$entryOverrides = @{
    xagent5000 = 'xflowsim.xl'
}

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
    $entryName = if ($entryOverrides.ContainsKey($name)) { $entryOverrides[$name] } else { "$name.xl" }
    $entry = Join-Path $directory.FullName $entryName
    if (-not (Test-Path -LiteralPath $entry -PathType Leaf)) {
        continue
    }

    $source = Get-Content -LiteralPath $entry -Raw -Encoding UTF8
    $testArgument = ''
    if ($source -match '(?i)(?:==|!=)\s*"selftest"|"selftest"\s*(?:==|!=)') {
        $testArgument = 'selftest'
    } elseif ($source -match '(?i)(?:==|!=)\s*"test"|"test"\s*(?:==|!=)') {
        $testArgument = 'test'
    }

    if ($testArgument -eq '') {
        continue
    }

    $stdoutPath = Join-Path $outputRoot "$name.stdout.log"
    $stderrPath = Join-Path $outputRoot "$name.stderr.log"
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo.FileName = $xl
    $process.StartInfo.Arguments = "run `"$entry`" -- $testArgument"
    $process.StartInfo.WorkingDirectory = $releaseRoot
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true

    $started = $process.Start()
    if (-not $started) {
        throw "failed to start self-test: $name"
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $completed = $process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $completed) {
        $process.Kill()
        $process.WaitForExit()
    }
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    [System.IO.File]::WriteAllText($stdoutPath, $stdout, [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText($stderrPath, $stderr, [System.Text.UTF8Encoding]::new($false))

    $status = if (-not $completed) { 'timeout' } elseif ($process.ExitCode -eq 0) { 'ok' } else { 'fail' }
    $exitCode = if ($completed) { $process.ExitCode } else { -1 }
    $results.Add([pscustomobject]@{
        Project = $name
        Argument = $testArgument
        Status = $status
        ExitCode = $exitCode
    })

    Write-Host ("{0,-14} {1,-8} exit={2}" -f $name, $status, $exitCode)
    if ($status -ne 'ok' -and -not $KeepGoing) {
        break
    }
}

$resultFileName = if ($Project -eq '') { 'results.csv' } else { "results.$Project.csv" }
$results | Export-Csv -LiteralPath (Join-Path $outputRoot $resultFileName) -NoTypeInformation -Encoding UTF8
$failed = @($results | Where-Object Status -NE 'ok')
Write-Host "selftests=$($results.Count) failed=$($failed.Count)"
if ($failed.Count -ne 0) {
    exit 1
}
