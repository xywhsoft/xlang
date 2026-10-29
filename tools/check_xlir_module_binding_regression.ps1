param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    # Focused current-source lane. The older comprehensive module gates also
    # use release compiler/DLL paths; this lane makes no claim about those.
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function',
        '-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $failures = @()
    foreach ($name in @('xlir_module_finalization','xlir_module_native_finalization',
        'xlir_module_scalar_finalization','xlir_module_cleanup_outcome')) {
        try {
            $prefix = "build/test/$name"
            foreach ($pass in @('', '.repeat')) {
                & $SourceDriver "test/cases/$name.xl" "$prefix$pass.c" "$prefix$pass.h" "$prefix$pass.json" --script
                if ($LASTEXITCODE -ne 0) { throw 'Current-source lifecycle generation failed' }
            }
            foreach ($ext in @('c','h','json')) {
                if ((Get-FileHash "$prefix.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { throw "Nondeterministic $ext" }
            }
            $inputs = @("test/cases/${name}_host.c")
            if ($name -eq 'xlir_module_cleanup_outcome') { $inputs += "test/cases/${name}_runtime.c" }
            & gcc @flags @inputs release/xl.dll -o "$prefix.binding-regression.exe"
            if ($LASTEXITCODE -ne 0) { throw 'Strict lifecycle host build failed' }
            & "$prefix.binding-regression.exe"
            if ($LASTEXITCODE -ne 0) { throw 'Strict lifecycle host runtime failed' }
            Write-Output "$name current-source binding regression passed"
        } catch { $failures += "$name : $_"; Write-Output $failures[-1] }
    }
    if ($failures.Count) { throw ($failures -join "`n") }
} finally { Pop-Location }
Write-Output 'Four current-source lifecycle fixtures passed deterministic C/H/JSON and strict runtime; comprehensive release/JIT admission is separate.'
