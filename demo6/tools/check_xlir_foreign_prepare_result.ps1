param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_foreign_prepare_result'
    & $SourceDriver test/cases/xlir_foreign_prepare_result.xl "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Preparation result generation failed' }
    & $SourceDriver test/cases/xlir_foreign_prepare_result.xl "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Preparation result repeat generation failed' }
    foreach ($ext in @('c','h','json')) {
        if ((Get-FileHash "$prefix.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { throw "Preparation result nondeterministic $ext" }
    }
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-O2','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-parameter','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    & gcc @flags -Dxl6ForeignCallBegin=prepare_result_begin -include "$prefix.c" -c test/cases/xlir_foreign_prepare_result_host.c -o "$prefix.host.o"
    if ($LASTEXITCODE -ne 0) { throw 'Preparation result optimized C compile failed' }
    & gcc @flags "$prefix.host.o" test/cases/xlir_foreign_prepare_memory.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Preparation result link failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Preparation result repeated-attempt ownership failed' }
} finally { Pop-Location }
