param([string]$Root = "", [switch]$Check)
$ErrorActionPreference = "Stop"
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$arguments = @((Join-Path $PSScriptRoot "gen_xrt_symbol_table.py"), "--root", $Root)
if ($Check) { $arguments += "--check" }
& python @arguments
exit $LASTEXITCODE
