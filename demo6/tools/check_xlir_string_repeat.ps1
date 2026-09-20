param([string]$Root = '', [string]$SourceDriver = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if ($SourceDriver -eq '') {
        $SourceDriver = 'build/test/xlir_string_repeat_source_driver.exe'
        & tools/build_xlir_validation_driver.ps1 -Output $SourceDriver
        if ($LASTEXITCODE -ne 0) { throw 'String repeat source driver build failed' }
    }
    $prefix = 'build/test/xlir_string_repeat'
    $failures = @()
    foreach ($negative in @('bad_parameter','bad_result')) {
        $output = @(& $SourceDriver "test/cases/xlir_string_repeat_$negative.xl" "$prefix.$negative.c" "$prefix.$negative.h" "$prefix.$negative.json" 2>&1)
        if ($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'string runtime bridge has no exact parameter/result ABI') {
            $failures += "Incorrect rejection: $negative $($output -join ' ')"
        }
    }
    try {
        foreach ($pass in @('', '.repeat')) {
            & $SourceDriver test/cases/xlir_string_repeat_abi.xl "$prefix$pass.c" "$prefix$pass.h" "$prefix$pass.json"
            if ($LASTEXITCODE -ne 0) { throw "String repeat generation failed: $pass" }
        }
        foreach ($extension in @('c','h','json')) {
            if ((Get-FileHash "$prefix.$extension").Hash -ne (Get-FileHash "$prefix.repeat.$extension").Hash) { throw "Nondeterministic $extension" }
        }
        $ir = Get-Content "$prefix.json" -Raw | ConvertFrom-Json
        $targets = @($ir.callTargets | Where-Object cName -eq 'xl6_xrt_string_repeat')
        if ($targets.Count -ne 1 -or $targets[0].runtimeBridgeName -ne 'string-repeat') { throw 'Missing exact repeat bridge identity' }
        $c = [IO.File]::ReadAllText("$prefix.c")
        if ($c.Contains('xl6_xrt_string_replace(') -or $c.Contains('xl6_xrt_string_split(')) { throw 'Repeat-only program pulls unrelated string bridges' }
        $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
            '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
        & gcc @flags -include "$prefix.c" test/cases/xlir_string_repeat_host.c release/xl.dll -o "$prefix.exe"
        if ($LASTEXITCODE -ne 0) { throw 'String repeat strict C build failed' }
        & "$prefix.exe"
        if ($LASTEXITCODE -ne 0) { throw 'String repeat language values/allocation outcomes failed' }
    } catch { $failures += $_.Exception.Message }
    if ($failures.Count) { throw ($failures -join "`n") }
} finally { Pop-Location }
Write-Output 'String repeat passed: explicit IR ABI, two rejected signature mutations, deterministic C/header/IR, minimal component selection, language boundaries and 300 allocation failures.'
