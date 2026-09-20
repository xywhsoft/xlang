param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    # Reuse the code-owner validation driver for typed/lowered result-demand mutations.
    & $SourceDriver --dynamic-results
    if ($LASTEXITCODE -ne 0) { throw 'Dynamic result-demand IR mutation contract failed' }
    $prefix = 'build/test/xlir_xcall_expression'
    foreach ($pass in @('generated','repeat')) {
        & $SourceDriver test/cases/xlir_xcall_expression.xl "$prefix.$pass.c" "$prefix.$pass.h" "$prefix.$pass.json" --script
        if ($LASTEXITCODE -ne 0) { throw 'XCall expression lowering failed' }
    }
    foreach ($ext in @('c','h','json')) {
        if ((Get-FileHash "$prefix.generated.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { throw "XCall expression nondeterministic $ext" }
    }
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    & gcc @flags test/cases/xlir_xcall_expression_host.c test/cases/xlir_xcall_expression_memory.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'XCall expression strict host build failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'XCall expression ownership/evaluation failed' }
} finally { Pop-Location }
