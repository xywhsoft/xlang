param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_async_module_lifetime'
    foreach ($pass in @('generated','repeat')) {
        & $SourceDriver test/cases/xlir_async_module_lifetime.xl "$prefix.$pass.c" "$prefix.$pass.h" "$prefix.$pass.json" --script
        if ($LASTEXITCODE -ne 0) { throw 'TCC async source generation failed' }
    }
    foreach ($ext in @('c','h','json')) {
        if ((Get-FileHash "$prefix.generated.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { throw "TCC async nondeterministic $ext" }
    }
    $ir = Get-Content "$prefix.generated.json" -Raw | ConvertFrom-Json
    if ($ir.asyncMachines.Count -ne 1) { throw 'TCC async canonical machine missing' }
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix.binding.h"), "#define JOINT_RUN xlir_async_start_$($ir.asyncMachines[0].targetSymbol)`n", [Text.UTF8Encoding]::new($false))
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers','-I.','-Ibuild/test','-Irelease','-Ilib/tcc')
    & gcc @flags -O2 -DJOINT_SOURCE_XRT -fwhole-program -c test/cases/xlir_async_module_tcc_host.c -o build/test/xlir_async_module_tcc.host.o
    if ($LASTEXITCODE -ne 0) { throw 'TCC async source host build failed' }
    & gcc @flags -O2 build/test/xlir_async_module_tcc.host.o src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c src/runtime/xl6_continuation_future.c src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_task_frame.c src/runtime/xl6_task_pool.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c release/xl.dll -o build/test/xlir_async_module_tcc.exe
    if ($LASTEXITCODE -ne 0) { throw 'TCC async resident link failed' }
    $failed = @()
    foreach ($mode in 0..5) { & build/test/xlir_async_module_tcc.exe $mode; if ($LASTEXITCODE -ne 0) { $failed += $mode } }
    if ($failed.Count) { throw "All actual TCC async modes collected; failed: $($failed -join ', ')" }
} finally { Pop-Location }
Write-Output 'Actual TCC async lifetime passed, including retained payload after MEM_FREE; released TCC/environment with current source module/dispatcher, not same-source release admission.'
