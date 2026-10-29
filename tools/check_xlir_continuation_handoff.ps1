param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_continuation_handoff'
    & gcc -std=c11 -Wall -Wextra -Werror -O2 -I. -DxrtFutureWatchAdd=handoff_watch_add `
        -c src/runtime/xl6_continuation_future.c -o "$prefix.transport.o"
    if ($LASTEXITCODE -ne 0) { throw 'Continuation Watch transport build failed' }
    & gcc -std=c11 -Wall -Wextra -Werror -O2 -I. test/cases/xlir_continuation_handoff_contract.c `
        src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c "$prefix.transport.o" release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Continuation handoff strict build failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Continuation successor capacity/lifetime contract failed' }
} finally { Pop-Location }
Write-Output 'Successor runtime handoff passed; generated-language dispatch is not yet admitted.'
