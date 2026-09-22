param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    # Compile this verifier/lowering contract from current sources. The DLL is
    # only its unchanged runtime dependency; do not test a stale compiler DLL.
    $prefix = 'build/test/xlir_evaluation_cleanup'
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
    & gcc @strict test/cases/xlir_evaluation_cleanup_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Evaluation cleanup source contract build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Evaluation cleanup module mutation or CFG contract failed' }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne [IO.File]::ReadAllText("$prefix.repeat.h") -or
        [IO.File]::ReadAllText("$prefix.json") -cne [IO.File]::ReadAllText("$prefix.repeat.json")) {
        throw 'Evaluation cleanup C/header determinism failed'
    }
    & gcc @strict test/cases/xlir_evaluation_cleanup_runtime.c test/cases/xlir_evaluation_cleanup_host.c release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Evaluation cleanup generated C strict build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Evaluation cleanup accessor transaction/runtime semantics failed' }
    & release/xl.exe run test/cases/xlir_evaluation_cleanup_runtime.c test/cases/xlir_evaluation_cleanup_host.c --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Evaluation cleanup generated C-host JIT failed' }
    $artifactRuntimeSources = @('src/runtime/xl6_slot.c',
        'src/runtime/xl6_code_lease.c', 'src/runtime/xl6_code_finalizer.c')
    & release/xl.exe build test/cases/xlir_evaluation_cleanup_runtime.c test/cases/xlir_evaluation_cleanup_host.c @artifactRuntimeSources src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Evaluation cleanup generated C-host AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Evaluation cleanup generated C-host AOT runtime failed' }

    $tasks = 'build/test/xlir_evaluation_cleanup_tasks'
    $taskFixture = 'test/cases/xlir_evaluation_cleanup_tasks.xl'
    & "$prefix.contract.exe" "$tasks.c" "$tasks.h" "$tasks.json" $taskFixture
    if ($LASTEXITCODE -ne 0) { throw 'Task operand source contract failed' }
    & "$prefix.contract.exe" "$tasks.repeat.c" "$tasks.repeat.h" "$tasks.repeat.json" $taskFixture
    if ($LASTEXITCODE -ne 0) { throw 'Task operand repeated source contract failed' }
    foreach ($extension in @('c','h','json')) {
        if ([IO.File]::ReadAllText("$tasks.$extension") -cne [IO.File]::ReadAllText("$tasks.repeat.$extension")) {
            throw "Task operand $extension output is nondeterministic"
        }
    }
    & gcc @strict test/cases/xlir_evaluation_cleanup_tasks_runtime.c test/cases/xlir_evaluation_cleanup_tasks_host.c release/xl.dll -o "$tasks.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Task operand strict C build failed' }
    & "$tasks.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Task operand cleanup or module lifetime failed' }
    & release/xl.exe run test/cases/xlir_evaluation_cleanup_tasks_runtime.c test/cases/xlir_evaluation_cleanup_tasks_host.c --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Task operand C-host JIT failed' }
    $taskArtifactRuntimeSources = $artifactRuntimeSources + @(
        'src/runtime/xl6_cell.c', 'src/runtime/xl6_closure.c',
        'src/runtime/xl6_capture.c', 'src/runtime/xl6_value_iterator.c',
        'src/runtime/xl6_generator.c', 'src/runtime/xl6_future_callback.c',
        'src/runtime/xl6_continuation_queue.c',
        'src/runtime/xl6_continuation_frame.c',
        'src/runtime/xl6_future_wait.c', 'src/runtime/xl6_future_payload.c',
        'src/runtime/xl6_task.c')
    & release/xl.exe build test/cases/xlir_evaluation_cleanup_tasks_runtime.c test/cases/xlir_evaluation_cleanup_tasks_host.c @taskArtifactRuntimeSources src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$tasks.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Task operand C-host AOT build failed' }
    & "$tasks.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Task operand C-host AOT runtime failed' }
} finally { Pop-Location }
Write-Output 'XLIR evaluation cleanup passed: explicit transaction identity, pending-result commit/rollback, cleanup errors and pre-loop failure routes, strict C and JIT/AOT.'
exit 0
