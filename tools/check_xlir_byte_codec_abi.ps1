param([string]$Root = "", [string]$Out = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
if ($Out -eq "") {
    $Out = Join-Path $Root ("build/test/codec-bytes-resident-" + [Guid]::NewGuid().ToString("N"))
}
& python (Join-Path $PSScriptRoot "check_stdlib_codec_template.py") --root $Root --out $Out --group codec-bytes
if ($LASTEXITCODE -ne 0) { throw "Resident codec-bytes actual-product acceptance failed: $Out" }
Write-Output "Resident codec-bytes: checked ABI, binary results, deterministic C, actual JIT/module/relocated AOT and family pruning passed. Report: $Out/results.json"
exit 0
