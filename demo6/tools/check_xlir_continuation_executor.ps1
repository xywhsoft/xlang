param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_continuation_executor'
    & gcc -std=c11 -Wall -Wextra -Werror -O2 -I. test/cases/xlir_continuation_executor_contract.c src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Continuation executor strict build failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Continuation executor isolation/progress/lifecycle failed' }
} finally { Pop-Location }
Write-Output 'Continuation executor service passed; verified language dispatcher integration remains separate.'
