$ErrorActionPreference = "Stop"

$project = $PSScriptRoot
$release = Resolve-Path (Join-Path $project "..\..")
$xl = Join-Path $release "xl.exe"
$outDir = Join-Path $project "run"
$outLog = Join-Path $outDir "xs_smoke.out.log"
$errLog = Join-Path $outDir "xs_smoke.err.log"
$customOut = Join-Path $outDir "custom.out"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Remove-Item -LiteralPath $outLog,$errLog,$customOut -Force -ErrorAction SilentlyContinue

function Wait-HttpReady {
    param(
        [int]$Port,
        [System.Diagnostics.Process]$ServerProcess
    )

    for ($i = 0; $i -lt 80; $i++) {
        if ($ServerProcess.HasExited) {
            $stderr = if (Test-Path -LiteralPath $errLog) {
                [System.IO.File]::ReadAllText($errLog, [System.Text.Encoding]::UTF8).Trim()
            } else {
                ""
            }
            throw "xs process exited before HTTP readiness: code=$($ServerProcess.ExitCode), stderr=$stderr"
        }
        try {
            $resp = Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:$Port/" -TimeoutSec 1
            if ($resp.StatusCode -eq 200) {
                return
            }
        } catch {
            Start-Sleep -Milliseconds 100
        }
    }
    $stderr = if (Test-Path -LiteralPath $errLog) {
        [System.IO.File]::ReadAllText($errLog, [System.Text.Encoding]::UTF8).Trim()
    } else {
        ""
    }
    throw "HTTP service did not become ready: stderr=$stderr"
}

$proc = Start-Process -FilePath $xl `
    -ArgumentList @("run", "projects\xs\xs.xl", "--", "--duration", "8000", "--quiet") `
    -WorkingDirectory $release `
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
        throw "xs smoke client failed with code $($client.ExitCode)"
    }

    if (!$proc.WaitForExit(12000)) {
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
