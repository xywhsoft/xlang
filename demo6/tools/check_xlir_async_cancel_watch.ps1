param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if (!(Test-Path -LiteralPath test/cases/xlir_continuation_owner_test.h -PathType Leaf)) {
        throw 'Continuation owner test helpers are missing'
    }
    $prefix = 'build/test/xlir_async_cancel_watch'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    if (!(Test-Path -LiteralPath build/test/xl.dll) -or
        (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
        Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-Wno-pointer-sign','-Wno-format-truncation','-I.','-Ibuild/test')
    & gcc @strict test/cases/xlir_async_cancel_watch_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Async cancel watch source contract build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Async cancel watch mutation/verification contract failed' }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Async cancel watch repeated verification failed' }
    foreach ($suffix in @('c','h','json','h.bind.h')) {
        if ([IO.File]::ReadAllText("$prefix.$suffix") -cne [IO.File]::ReadAllText("$prefix.repeat.$suffix")) {
            throw "Async cancel watch nondeterministic $suffix"
        }
    }
    $hostSource = 'test/cases/xlir_async_cancel_watch_host.c'
    $runtimeSources = @('test/cases/xlir_async_source_sync.c','test/cases/xlir_async_source_transport.c',
        'src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c')
    & gcc @strict test/cases/xlir_async_cancel_watch_runtime.c $hostSource @runtimeSources release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Async cancel watch strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Async cancel watch runtime/memory failed' }
    & release/xl.exe run test/cases/xlir_async_cancel_watch_runtime.c $hostSource @runtimeSources --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Async cancel watch C-host JIT failed' }
    & release/xl.exe build test/cases/xlir_async_cancel_watch_runtime.c $hostSource @runtimeSources src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Async cancel watch C-host AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Async cancel watch C-host AOT runtime failed' }
} finally { Pop-Location }
Write-Output 'XLIR async cancel watch passed: canonical Watch transport, cooperative cancellation through verified resume CFG, exactly-once finally/retirement, all outcomes, isolated producer, strict C memory, C-host JIT/AOT and deterministic output.'
exit 0
