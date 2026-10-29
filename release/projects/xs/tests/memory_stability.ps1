param(
    [int64]$AllowedGrowthBytes = 201326592,
    [int]$TimeoutSeconds = 240
)

$ErrorActionPreference = 'Stop'
$releaseDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$xlPath = Join-Path $releaseDir 'xl.exe'
$testPath = Join-Path $releaseDir 'projects\xs\src\runtime_soak_test.xl'
$process = $null
$samples = New-Object System.Collections.Generic.List[long]
$watch = [System.Diagnostics.Stopwatch]::StartNew()

try {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $xlPath
    $info.WorkingDirectory = $releaseDir
    $info.Arguments = "run `"$testPath`""
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $info
    if (-not $process.Start()) { throw 'cannot start XS memory stability test' }

    while (-not $process.HasExited) {
        if ($watch.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
            $process.Kill()
            throw "XS memory stability test exceeded $TimeoutSeconds seconds"
        }
        Start-Sleep -Milliseconds 250
        if (-not $process.HasExited -and $watch.Elapsed.TotalSeconds -ge 2) {
            $process.Refresh()
            $samples.Add([long]$process.PrivateMemorySize64)
        }
    }

    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    if ($process.ExitCode -ne 0) {
        throw "XS memory stability workload failed with exit $($process.ExitCode)`n$stdout`n$stderr"
    }

    if ($samples.Count -ge 8) {
        $window = [Math]::Max(2, [Math]::Floor($samples.Count / 4))
        $early = ($samples | Select-Object -First $window | Measure-Object -Minimum).Minimum
        $late = ($samples | Select-Object -Last $window | Measure-Object -Minimum).Minimum
        $growth = [long]$late - [long]$early
        if ($growth -gt $AllowedGrowthBytes) {
            throw "XS retained memory grew by $growth bytes; allowed growth is $AllowedGrowthBytes"
        }
        Write-Host "xs2 memory stability ok: samples=$($samples.Count), retained-growth=$growth"
    } else {
        Write-Host "xs2 memory stability ok: workload completed before a trend window was available"
    }
} finally {
    if ($process -ne $null -and -not $process.HasExited) {
        $process.Kill()
        $process.WaitForExit()
    }
    if ($process -ne $null) { $process.Dispose() }
}
