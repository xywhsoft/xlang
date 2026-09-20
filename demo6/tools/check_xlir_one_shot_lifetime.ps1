param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $api = [IO.File]::ReadAllText('src/api/xl_api.c')
    $entry = [regex]::Match($api, '(?ms)^static int xlCompileAndRunUnit\(.*?^\}').Value
    foreach ($required in @('xlCompileUnitToModuleWithC(ctx, unit, &options, NULL, XLC_BACKEND_XLIR)',
        'return xlRunAndRetireModuleUnlocked(ctx, module, arg)')) {
        if (!$entry.Contains($required)) { throw "One-shot bypasses shared module path: $required" }
    }
    if ($entry -match 'tcc_delete\s*\(') { throw 'One-shot raw delete returned' }
    $prefix = 'build/test/xlir_one_shot_lifetime'
    foreach ($pass in @('generated','repeat')) {
        & $SourceDriver test/cases/xlir_one_shot_lifetime.xl "$prefix.$pass.c" "$prefix.$pass.h" "$prefix.$pass.json" --script
        if ($LASTEXITCODE -ne 0) { throw 'One-shot source generation failed' }
    }
    foreach ($ext in @('c','h','json')) {
        if ((Get-FileHash "$prefix.generated.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { throw "One-shot nondeterministic $ext" }
    }
    $ir = Get-Content "$prefix.generated.json" -Raw | ConvertFrom-Json
    if ($ir.asyncMachines.Count -ne 1) { throw 'One-shot fixture lost canonical machine' }
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix.binding.h"), "#define JOINT_RUN xlir_async_start_$($ir.asyncMachines[0].targetSymbol)`n", [Text.UTF8Encoding]::new($false))
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-parameter','-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers','-I.','-Ibuild/test','-Irelease','-Ilib/tcc')
    & gcc @flags -O2 -fwhole-program -c test/cases/xlir_one_shot_lifetime_host.c -o "$prefix.host.o"
    if ($LASTEXITCODE -ne 0) { throw 'One-shot source API host build failed' }
    & gcc @flags -O2 -DXL_BUILD_DLL "$prefix.host.o" src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c src/runtime/xl6_continuation_future.c src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c release/xl.dll "-Wl,--out-implib,$prefix.host.a" -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'One-shot resident runtime link failed' }
    & gcc @flags -O2 -shared test/cases/xlir_one_shot_lifetime_artifact.c "$prefix.host.a" release/xl.dll '-Wl,--export-all-symbols' -o "$prefix.generated.dll"
    if ($LASTEXITCODE -ne 0) { throw 'One-shot generated DLL link failed' }
    $failed = @()
    foreach ($mode in 0..6) { & "$prefix.exe" $mode; if ($LASTEXITCODE -ne 0) { $failed += $mode } }
    if ($failed.Count) { throw "All one-shot modes collected; failed: $($failed -join ', ')" }
} finally { Pop-Location }
Write-Output 'One-shot shared source execution/retirement passed from relocated state; actual TCC compilation/unmapping and escaping code-bearing objects remain separate.'
