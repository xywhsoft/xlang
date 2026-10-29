$ErrorActionPreference = "Stop"

$project = $PSScriptRoot
$release = Resolve-Path (Join-Path $project "..\..")
$xl = Join-Path $release "xl.exe"
$xs = Join-Path $project "xs.xl"
$outDir = Join-Path $project "run"
$outLog = Join-Path $outDir "xs_smoke.out.log"
$errLog = Join-Path $outDir "xs_smoke.err.log"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Remove-Item -LiteralPath $outLog,$errLog -Force -ErrorAction SilentlyContinue

function Read-SharedText {
    param([string]$Path)

    if (!(Test-Path -LiteralPath $Path)) {
        return ""
    }
    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite)
    try {
        $reader = [System.IO.StreamReader]::new($stream, [System.Text.Encoding]::UTF8)
        try {
            return $reader.ReadToEnd().Trim()
        } finally {
            $reader.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Wait-HttpReady {
    param(
        [int]$Port,
        [System.Diagnostics.Process]$ServerProcess
    )

    $lastError = ""
    for ($i = 0; $i -lt 80; $i++) {
        if ($ServerProcess.HasExited) {
            $stderr = Read-SharedText $errLog
            throw "xs process exited before HTTP readiness: code=$($ServerProcess.ExitCode), stderr=$stderr"
        }
        try {
            $resp = Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:$Port/" -TimeoutSec 1
            if ($resp.StatusCode -eq 200) {
                return
            }
        } catch {
            $lastError = $_.Exception.Message
            Start-Sleep -Milliseconds 100
        }
    }
    $stderr = Read-SharedText $errLog
    throw "HTTP service did not become ready: lastError=$lastError, stderr=$stderr"
}

$proc = Start-Process -FilePath $xl `
    -ArgumentList @("run", $xs, "--", "--duration", "12000") `
    -WorkingDirectory $project `
    -RedirectStandardOutput $outLog `
    -RedirectStandardError $errLog `
    -PassThru

try {
    Wait-HttpReady -Port 18080 -ServerProcess $proc

    $client = Start-Process -FilePath $xl `
        -ArgumentList @("run", "projects\xs\smoke_client.xl") `
        -WorkingDirectory $release `
        -NoNewWindow `
        -Wait `
        -PassThru
    if ($client.ExitCode -ne 0) {
        if (!$proc.HasExited) {
            Stop-Process -Id $proc.Id -Force
            $proc.WaitForExit()
        }
        $serverOut = Read-SharedText $outLog
        $serverErr = Read-SharedText $errLog
        throw "xs smoke client failed with code $($client.ExitCode), stdout=$serverOut, stderr=$serverErr"
    }

    # Cover the run window and teardown of isolated handler contexts.
    if (!$proc.WaitForExit(30000)) {
        throw "xs process did not exit after duration"
    }
    $proc.Refresh()
    $serverExitCode = $proc.ExitCode
    if ($null -eq $serverExitCode) {
        $serverExitCode = 0
    }
    if ($serverExitCode -ne 0) {
        throw "xs process exited with code $serverExitCode"
    }

    Write-Host "xs smoke ok"
} finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
}
