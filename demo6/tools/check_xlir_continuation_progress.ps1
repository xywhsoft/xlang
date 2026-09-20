param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_continuation_progress'
    & gcc -std=c11 -Wall -Wextra -Werror -O2 -I. test/cases/xlir_continuation_progress_contract.c src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Guaranteed-progress strict build failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Guaranteed root admission/progress/retirement contract failed' }
} finally { Pop-Location }
Write-Output 'Guaranteed root admission passed; this runtime strategy is not yet selected by production async IR/Cgen.'
