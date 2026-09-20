param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-O2','-I.')
    & gcc @flags -shared test/cases/xlir_code_lease_drop.c -o build/test/xlir_code_lease_drop.dll
    if ($LASTEXITCODE -ne 0) { throw 'Lease unloadable callback DLL build failed' }
    & gcc @flags test/cases/xlir_code_lease_contract.c src/runtime/xl6_code_lease.c release/xl.dll -o build/test/xlir_code_lease.exe
    if ($LASTEXITCODE -ne 0) { throw 'Resident code lease build failed' }
    & build/test/xlir_code_lease.exe
    if ($LASTEXITCODE -ne 0) { throw 'Shared code lease lifecycle failed' }
    & release/xl.exe run test/cases/xlir_code_lease_contract.c src/runtime/xl6_code_lease.c --inc-path . -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Code lease C-host JIT smoke failed' }
    & release/xl.exe build test/cases/xlir_code_lease_contract.c src/runtime/xl6_code_lease.c --inc-path . -o build/test/xlir_code_lease.aot.exe
    if ($LASTEXITCODE -ne 0) { throw 'Code lease C-host AOT build failed' }
    & build/test/xlir_code_lease.aot.exe --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Code lease C-host AOT smoke failed' }
} finally { Pop-Location }
Write-Output 'Resident shared residency/drop adapter passed; production IR selection and module self-ownership cycles remain unadmitted.'
