$ErrorActionPreference = "Stop"

$projectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$releaseDir = Resolve-Path (Join-Path $projectDir "..\..")
$exe = Join-Path $projectDir "release\xs_static.exe"
$port = 18088
$duration = 6000

$args = @(
    "--port", "$port",
    "--duration", "$duration",
    "--quiet"
)

$proc = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $releaseDir -PassThru -WindowStyle Hidden

try {
    $ready = $false
    for ( $i = 0; $i -lt 40; $i++ ) {
        try {
            $health = Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:$port/health" -TimeoutSec 1
            if ( $health.Content -eq "ok" ) {
                $ready = $true
                break
            }
        } catch {
            Start-Sleep -Milliseconds 150
        }
    }

    if ( -not $ready ) {
        throw "xs_static did not become ready"
    }

    $info = Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:$port/api/info" -TimeoutSec 2
    if ( $info.Content -notmatch '"name":"xs_static"' ) {
        throw "api/info mismatch: $($info.Content)"
    }

    $static = Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:$port/readme.txt" -TimeoutSec 2
    if ( $static.Content -notmatch "xs_static static file root" ) {
        throw "static file mismatch: $($static.Content)"
    }

    $echo = Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:$port/api/echo" -Method Post -Body "ping" -TimeoutSec 2
    if ( $echo.Content -ne "ping" ) {
        throw "api/echo mismatch: $($echo.Content)"
    }

    Write-Host "xs_static smoke ok"
} finally {
    if ( $proc -and -not $proc.HasExited ) {
        Stop-Process -Id $proc.Id -Force
    }
}
