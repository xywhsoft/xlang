param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_async_module_lifetime'
    & $SourceDriver test/cases/xlir_async_module_lifetime.xl "$prefix.generated.c" "$prefix.generated.h" "$prefix.generated.json" --script
    if ($LASTEXITCODE -ne 0) { throw 'Joint lifecycle generation failed' }
    & $SourceDriver test/cases/xlir_async_module_lifetime.xl "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json" --script
    if ($LASTEXITCODE -ne 0) { throw 'Joint lifecycle repeat generation failed' }
    foreach ($ext in @('c','h','json')) {
        if ((Get-FileHash "$prefix.generated.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash) { throw "Joint nondeterministic $ext" }
    }
    $ir = Get-Content "$prefix.generated.json" -Raw | ConvertFrom-Json
    # Fail safely with an older driver, before the host deliberately unmaps
    # the producer while keeping its payload alive.
    $generated = [IO.File]::ReadAllText("$prefix.generated.c")
    if ($generated.Contains('xlir_async_value_free') -or
        !$generated.Contains('xl6PayloadResolveOwned(destination, value, XL6_FUTURE_PAYLOAD_VALUE)')) {
        throw 'Future payload destructor is not resident; unsafe unload test not executed'
    }
    if ($ir.asyncMachines.Count -ne 1) { throw 'Joint fixture must have one verified machine' }
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix.binding.h"), "#define JOINT_RUN xlir_async_start_$($ir.asyncMachines[0].targetSymbol)`n", [Text.UTF8Encoding]::new($false))
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-parameter','-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers','-I.','-Ibuild/test','-Irelease','-Ilib/tcc')
    & gcc @flags -O2 -fwhole-program -c test/cases/xlir_async_module_lifetime_host.c -o "$prefix.host.o"
    if ($LASTEXITCODE -ne 0) { throw 'Joint source API host compilation failed' }
    & gcc @flags -O2 -DXL_BUILD_DLL "$prefix.host.o" src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c src/runtime/xl6_continuation_future.c src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_task_pool.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c release/xl.dll "-Wl,--out-implib,$prefix.host.a" -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Joint resident runtime host link failed' }
    & gcc @flags -O2 -shared test/cases/xlir_async_module_lifetime_artifact.c "$prefix.host.a" release/xl.dll '-Wl,--export-all-symbols' -o "$prefix.generated.dll"
    if ($LASTEXITCODE -ne 0) { throw 'Joint generated unloadable DLL link failed' }
    $failed = @()
    foreach ($mode in 0..5) {
        & "$prefix.exe" $mode
        if ($LASTEXITCODE -ne 0) { $failed += $mode }
    }
    if ($failed.Count) { throw "All joint lifecycle modes collected; failed: $($failed -join ', ')" }
} finally { Pop-Location }
Write-Output 'Generated DLL/source API/resident dispatcher lifecycle passed; DLL unmapping is real, TCC relocation and escaped closure/native payload coverage remain separate.'
