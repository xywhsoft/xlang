param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_async_dispatch'
    & $SourceDriver test/cases/xlir_async_dispatch.xl "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Dispatch source generation failed' }
    & $SourceDriver test/cases/xlir_async_dispatch.xl "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Dispatch repeat generation failed' }
    foreach ($extension in @('c','h','json')) {
        if ((Get-FileHash "$prefix.$extension").Hash -ne (Get-FileHash "$prefix.repeat.$extension").Hash) {
            throw "Dispatch nondeterministic $extension"
        }
    }
    $ir = Get-Content "$prefix.json" -Raw | ConvertFrom-Json
    $awaits = @($ir.instructions | Where-Object kindName -eq 'await')
    if ($awaits.Count -ne 3 -or @($awaits | Where-Object awaitOutcomeAbi -ne 1).Count) { throw 'Dispatch await status carrier missing' }
    if ($ir.moduleContinuationAbi -ne 1 -or $ir.asyncMachines.Count -ne 1 -or
        $ir.asyncMachines[0].executionAbi -ne 1 -or $ir.asyncMachines[0].suspensionCount -ne 2) { throw 'Dispatch IR contract missing' }
    $binding = "#define XLIR_DISPATCH_CHAIN xlir_async_start_$($ir.asyncMachines[0].targetSymbol)`n"
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix.binding.h"), $binding, [Text.UTF8Encoding]::new($false))
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    & gcc @flags -DxrtTakeError=dispatch_take_error -DXLIR_CONTINUATION_CAPACITY=2 -include "$prefix.c" -include "$prefix.binding.h" -c test/cases/xlir_async_dispatch_host.c -o "$prefix.host.o"
    if ($LASTEXITCODE -ne 0) { throw 'Generated dispatch strict C compile failed' }
    & gcc @flags -DxrtFutureWatchAdd=dispatch_watch_add -c src/runtime/xl6_continuation_future.c -o "$prefix.transport.o"
    if ($LASTEXITCODE -ne 0) { throw 'Dispatch Watch boundary build failed' }
    & gcc @flags "$prefix.host.o" "$prefix.transport.o" test/cases/xlir_async_dispatch_runtime.c src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Dispatch source runtime link failed' }
    $failed = @()
    foreach ($mode in 0..6) {
        & "$prefix.exe" $mode
        if ($LASTEXITCODE -ne 0) { $failed += "capacity=2 mode=$mode" }
    }
    & gcc @flags -DxrtTakeError=dispatch_take_error -DXLIR_CONTINUATION_CAPACITY=1 -include "$prefix.c" -include "$prefix.binding.h" -c test/cases/xlir_async_dispatch_host.c -o "$prefix.single.o"
    if ($LASTEXITCODE -ne 0) { throw 'Single-credit dispatch strict C compile failed' }
    & gcc @flags "$prefix.single.o" "$prefix.transport.o" test/cases/xlir_async_dispatch_runtime.c src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c release/xl.dll -o "$prefix.single.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Single-credit dispatch link failed' }
    foreach ($mode in @(0,1,2,3,4)) {
        & "$prefix.single.exe" $mode
        if ($LASTEXITCODE -ne 0) { $failed += "capacity=1 mode=$mode" }
    }
    if ($failed.Count) { throw "All dispatch lanes collected; failures: $($failed -join ', ')" }
} finally { Pop-Location }
Write-Output 'Generated async dispatcher passed; source release integration and escaped-code coverage remain separate admission work.'
