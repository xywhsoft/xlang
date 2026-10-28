param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$directory = Join-Path $Root ("build/test/websocket-managed-" + [Guid]::NewGuid().ToString("N"))
& python (Join-Path $Root "tools/check_stdlib_websocket.py") --root $Root --out $directory `
    --case stdlib_websocket_stream_v2 --case stdlib_websocket_tls_v2 `
    --case stdlib_websocket_cycle_v2 --case stdlib_websocket_callback_error_v2
if ($LASTEXITCODE -ne 0) { throw "WebSocket exact resident ABI/product gate failed: $directory" }
