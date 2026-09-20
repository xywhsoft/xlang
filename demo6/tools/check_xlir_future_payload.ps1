param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_future_payload'
    & gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -O2 -fwhole-program -I. test/cases/xlir_future_payload_trim.c -o "$prefix.trim.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Payload private-source trim build failed' }
    & "$prefix.trim.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Payload private-source trim dependencies failed' }
    $inputs = @('test/cases/xlir_future_payload_contract.c','src/runtime/xl6_future_payload.c')
    & gcc -std=c11 -Wall -Wextra -Werror -O2 -I. @inputs release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Future payload resident runtime build failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Future payload ownership failed' }
    & release/xl.exe run @inputs --inc-path . -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Future payload C-host JIT failed' }
    & release/xl.exe build @inputs --inc-path . -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Future payload C-host AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Future payload C-host AOT failed' }
} finally { Pop-Location }
Write-Output 'Resident box ownership passed; contained closure/class/native code lifetimes require independent verification.'
