param([string]$Root = '', [string]$SourceDriver = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if ($SourceDriver -eq '') {
        $SourceDriver = 'build/test/xlir_resume_isolation_source_driver.exe'
        & tools/build_xlir_validation_driver.ps1 -Output $SourceDriver
        if ($LASTEXITCODE -ne 0) { throw 'Resume isolation current-source driver build failed' }
    }
    $prefix = 'build/test/xlir_async_resume_isolation'
    foreach ($pass in @('', '.repeat')) {
        & $SourceDriver test/cases/xlir_async_resume_isolation.xl "$prefix$pass.c" "$prefix$pass.h" "$prefix$pass.json"
        if ($LASTEXITCODE -ne 0) { throw 'Resume isolation verified generation failed' }
    }
    foreach ($extension in @('c','h','json')) {
        if ((Get-FileHash "$prefix.$extension").Hash -ne (Get-FileHash "$prefix.repeat.$extension").Hash) { throw "Nondeterministic $extension" }
    }
    $ir = Get-Content "$prefix.json" -Raw | ConvertFrom-Json
    $machines = @($ir.asyncMachines)
    if ($machines.Count -ne 1 -or $machines[0].suspensionCount -ne 1) { throw 'Fixture lost its unique verified async machine' }
    $binding = "#define XLIR_ISOLATED_RESUME xlir_async_start_$($machines[0].targetSymbol)`n"
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix.binding.h"), $binding, [Text.UTF8Encoding]::new($false))
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    & gcc @flags -include "$prefix.c" -include "$prefix.binding.h" -c test/cases/xlir_async_resume_isolation_host.c -o "$prefix.host.o"
    if ($LASTEXITCODE -ne 0) { throw 'Resume isolation generated host compile failed' }
    & gcc @flags "$prefix.host.o" src/runtime/xl6_code_lease.c src/runtime/xl6_continuation_queue.c src/runtime/xl6_continuation_executor.c src/runtime/xl6_continuation_future.c src/runtime/xl6_closure.c src/runtime/xl6_cell.c src/runtime/xl6_slot.c src/runtime/xl6_frame_storage.c src/runtime/xl6_continuation_frame.c src/runtime/xl6_future_wait.c src/runtime/xl6_task.c src/runtime/xl6_task_frame.c src/runtime/xl6_task_pool.c src/runtime/xl6_capture.c src/runtime/xl6_value_iterator.c src/runtime/xl6_future_callback.c src/runtime/xl6_generator.c src/runtime/xl6_future_payload.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Resume isolation strict C link failed' }
    $output = @(& "$prefix.exe" 2>&1)
    $code = $LASTEXITCODE
    $report = [pscustomobject]@{DriverSha256=(Get-FileHash $SourceDriver).Hash;Exit=$code;Output=@($output|ForEach-Object{[string]$_});Scope='400 cases: ready/pending, success/failure/cancel/close, exact outcome, finally count, producer diagnostic and execution isolation'}
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix.results.json"), ($report | ConvertTo-Json -Depth 4), [Text.UTF8Encoding]::new($false))
    $output | Write-Output
    if ($code -ne 0) { throw 'Resume isolation gate failed; all runtime cases collected' }
} finally { Pop-Location }
Write-Output 'Async resume isolation passed.'
