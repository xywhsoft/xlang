param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_continuation_queue'
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-O2','-I.')
    & gcc @flags test/cases/xlir_continuation_queue_contract.c src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Continuation queue strict build failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Continuation queue lifecycle/ownership failed' }
} finally { Pop-Location }
Write-Output 'Continuation queue transport passed; language dispatcher integration is not yet admitted.'
