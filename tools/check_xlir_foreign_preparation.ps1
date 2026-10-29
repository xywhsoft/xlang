param([string]$Root = '', [string]$SourceDriver = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if ($SourceDriver -eq '') {
        $SourceDriver = 'build/test/xlir_foreign_preparation_driver.exe'
        & tools/build_xlir_validation_driver.ps1 -Output $SourceDriver
        if ($LASTEXITCODE -ne 0) { throw 'Preparation source driver build failed' }
    }
    $prefix = 'build/test/xlir_foreign_preparation'
    foreach ($pass in @('', '.repeat')) {
        & $SourceDriver test/cases/xlir_foreign_prepare_ownership.xl "$prefix$pass.c" "$prefix$pass.h" "$prefix$pass.json"
        if ($LASTEXITCODE -ne 0) { throw 'Preparation generation failed' }
    }
    foreach ($extension in @('c','h','json')) {
        if ((Get-FileHash "$prefix.$extension").Hash -ne (Get-FileHash "$prefix.repeat.$extension").Hash) { throw "Nondeterministic $extension" }
    }
    $ir = Get-Content "$prefix.json" -Raw | ConvertFrom-Json
    $entry = @($ir.functions | Where-Object kindName -eq 'user')
    $foreign = @($ir.callTargets | Where-Object cName -eq 'prepare_consume')
    if ($entry.Count -ne 1 -or $foreign.Count -ne 1 -or
        $foreign[0].preparationFailureAbiName -ne 'retire-owned-reverse') { throw 'Preparation target contract missing' }
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-label','-Wno-missing-field-initializers','-I.')
    & gcc @flags '-Dxl6ForeignCallBegin=prepare_begin' '-DxrtStrDup=prepare_dup' '-DxrtFree=prepare_free' `
        "-DXLIR_PREPARE_CASE=xlir_fn_$($entry[0].symbol)" -include "$prefix.c" `
        -c test/cases/xlir_foreign_prepare_ownership_host.c -o "$prefix.host.o"
    if ($LASTEXITCODE -ne 0) { throw 'Preparation strict host build failed' }
    & gcc -std=c11 -Wall -Wextra -Werror -I. "$prefix.host.o" test/cases/xlir_foreign_prepare_memory.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Preparation strict link failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Preparation ownership/first-outcome runtime failed' }
} finally { Pop-Location }
Write-Output 'Foreign preparation passed: public ABI fault injection (not allocator failure), two reverse-owned inputs, untouched borrow, first-error and false/NULL preservation, deterministic C/header/IR.'
