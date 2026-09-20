param([string]$Root = '', [string]$SourceDriver = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/cgen_native_class_bind_point.xl'
    $prefix = 'build/test/xlir_native_take_reentry'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    if ($SourceDriver -eq '') {
        $SourceDriver = 'build/test/xlir_native_take_source_driver.exe'
        & tools/build_xlir_validation_driver.ps1 -Output $SourceDriver
        if ($LASTEXITCODE -ne 0) { throw 'Native TAKE current-source driver build failed' }
    }
    & $SourceDriver $source build/test/xlir_native_handle_bind_generated.c "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Native TAKE current-source verified generation failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $origins = @($ir.nativeHandleBinds.origin)
    $takes = @($ir.instructions | Where-Object {
        $_.runtimeOpName -eq 'xvalue-native-handle-take' -and $_.origin -in $origins
    })
    $destroy = @($takes.targetSymbol | Select-Object -Unique)
    if ($takes.Count -ne 3 -or $destroy.Count -ne 1 -or $destroy[0] -eq 0 -or
        @($ir.callTargets | Where-Object {
            $_.symbol -eq $destroy[0] -and $_.cName -eq 'borrowed_box_close'
        }).Count -ne 1) { throw 'Native TAKE reentry lost its verified destructor target' }
    # Generated test binding follows the canonical symbol, not a hardcoded ID.
    [IO.File]::WriteAllText((Join-Path (Get-Location) 'build/test/xlir_native_take_helper.h'),
        "#define XLIR_NATIVE_TAKE_HELPER xlir_native_handle_take_s$($destroy[0])`n#define XLIR_NATIVE_DROP_HELPER xlir_native_handle_drop_s$($destroy[0])`n",
        [Text.UTF8Encoding]::new($false))
    & $SourceDriver $source "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0 -or ![System.Linq.Enumerable]::SequenceEqual(
        [byte[]][IO.File]::ReadAllBytes('build/test/xlir_native_handle_bind_generated.c'),
        [byte[]][IO.File]::ReadAllBytes("$prefix.repeat.c"))) { throw 'Native TAKE C is not deterministic' }
    $strict = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-O0',
        '-Wno-unused-function', '-Wno-unused-label', '-Wno-unused-variable',
        '-Wno-unused-parameter', '-Wno-unused-const-variable', '-Wno-missing-field-initializers',
        '-I.', '-Ibuild/test')
    $runtime = 'test/cases/xlir_native_take_reentry_runtime.c'
    $hostSource = 'test/cases/xlir_native_take_reentry_host.c'
    $smokeSource = 'test/cases/xlir_native_take_reentry_smoke.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Native TAKE strict C compilation failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Native TAKE allocation-failure reentry ownership failed' }
    & release/xl.exe run $runtime $smokeSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Native TAKE C-host JIT reentry failed' }
    & release/xl.exe build $runtime $smokeSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Native TAKE standalone AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Native TAKE standalone AOT reentry failed' }
} finally { Pop-Location }
Write-Output 'XLIR native TAKE reentry passed: exact destructor target, deterministic C, allocation-failure detach, replacement owner preservation, strict allocation accounting and C-host JIT/AOT.'
exit 0
