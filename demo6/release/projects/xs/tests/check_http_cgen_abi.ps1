$ErrorActionPreference = "Stop"

$release = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$xl = Join-Path $release "xl.exe"
$outDir = Join-Path $release "build\test\xs_http_cgen_abi"
$plainOut = Join-Path $outDir "plain.c"
$tlsOut = Join-Path $outDir "tls.c"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function Invoke-Cgen([string]$source, [string]$output)
{
    & $xl cgen $source --emit $output
    if ( $LASTEXITCODE -ne 0 ) {
        throw "cgen failed: $source"
    }
}

Invoke-Cgen "examples\stdlib_http_server.xl" $plainOut
Invoke-Cgen "projects\xs\tests\xrt_owned_tls.xl" $tlsOut

$plain = Get-Content -LiteralPath $plainOut -Raw -Encoding UTF8
$tls = Get-Content -LiteralPath $tlsOut -Raw -Encoding UTF8

if ( !$plain.Contains("xrtWebServerCreateHostFullEx(") ) {
    throw "plain HTTP cgen does not use the stable scalar XRT constructor"
}
if ( !$tls.Contains("xrtWebServerCreateHostTlsFullEx(") ) {
    throw "TLS HTTP cgen does not use the stable scalar XRT constructor"
}

$generated = $plain + "`n" + $tls
foreach ( $forbidden in @("typedef struct xtlsconfig", "typedef struct xwebconfig", "typedef struct xrt_net_addr") ) {
    if ( $generated.Contains($forbidden) ) {
        throw "HTTP cgen duplicates an XRT-owned ABI layout: $forbidden"
    }
}

Remove-Item -LiteralPath $plainOut, $tlsOut -Force
Write-Host "xs2 HTTP cgen ABI guard ok"
