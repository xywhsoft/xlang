param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_cell_allocation_outcome'
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
    & gcc @strict test/cases/xlir_cell_allocation_outcome_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Cell allocation entry source contract build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Cell allocation entry mutation/verification failed' }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Cell allocation entry repeated verification failed' }
    foreach ($suffix in @('c','h','json','h.bind.h','c.suspension.c','c.suspension.h','c.suspension.json')) {
        if ([IO.File]::ReadAllText("$prefix.$suffix") -cne [IO.File]::ReadAllText("$prefix.repeat.$suffix")) {
            throw "Cell allocation entry nondeterministic $suffix"
        }
    }
    $hostSource = 'test/cases/xlir_cell_allocation_outcome_host.c'
    $runtimeSource = 'test/cases/xlir_cell_allocation_outcome_runtime.c'
    & gcc @strict $runtimeSource $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Cell allocation entry strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Cell allocation entry runtime/memory failed' }
    & release/xl.exe run $runtimeSource $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Cell allocation entry C-host JIT failed' }
    # C-host artifacts do not have a language IR runtime plan. Supply the real
    # resident implementations, rather than relying on the JIT's DLL symbols.
    $syncRuntimeSources = @('src/runtime/xl6_slot.c','src/runtime/xl6_code_lease.c',
        'src/runtime/xl6_cell.c','src/runtime/xl6_closure.c')
    & release/xl.exe build $runtimeSource $hostSource @syncRuntimeSources src/api/xl_foreign_error.c --inc-path . --inc-path release --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Cell allocation entry C-host AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Cell allocation entry C-host AOT runtime failed' }
    $suspensionHost = 'test/cases/xlir_cell_allocation_suspension_host.c'
    & gcc @strict $suspensionHost release/xl.dll -o "$prefix.suspension.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Cell suspension strict C build failed' }
    & "$prefix.suspension.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Cell suspension strict C runtime failed' }
    & release/xl.exe run $suspensionHost --inc-path . --inc-path build/test
    if ($LASTEXITCODE -ne 0) { throw 'Cell suspension C-host JIT failed' }
    $suspensionRuntimeSources = @($syncRuntimeSources) + @(
        'src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c',
        'src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_task_pool.c',
        'src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c',
        'src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c',
        'src/runtime/xl6_code_finalizer.c','src/runtime/xl6_continuation_queue.c',
        'src/runtime/xl6_continuation_executor.c','src/runtime/xl6_continuation_future.c',
        'src/runtime/xl6_future_payload.c','src/runtime/xl6_time_core.c')
    & release/xl.exe build $suspensionHost @suspensionRuntimeSources src/api/xl_foreign_error.c --inc-path . --inc-path release --inc-path build/test -o "$prefix.suspension.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Cell suspension C-host AOT build failed' }
    & "$prefix.suspension.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Cell suspension C-host AOT runtime failed' }
} finally { Pop-Location }
Write-Output 'XLIR cell allocation entry gate passed: scalar/string/var/nullable, parameter prefix, expression-pattern owners, catch/finally, exact outcomes, async/generator smoke and deterministic C/header/IR. Loop binding and concurrent capture admission require separate verification.'
exit 0
