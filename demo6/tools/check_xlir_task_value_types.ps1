param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    # Compile this verifier/lowering contract from current sources. The DLL is
    # only its unchanged runtime dependency; do not test a stale compiler DLL.
    $prefix = 'build/test/xlir_task_value_types'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    if (!(Test-Path -LiteralPath build/test/xl.dll) -or
        (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
        Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
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
    & gcc @strict test/cases/xlir_task_value_types_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Task value types source contract build failed' }
    & "$prefix.contract.exe" --admission
    if ($LASTEXITCODE -ne 0) { throw 'Task language admission contract failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Task value types module mutation or CFG contract failed' }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne [IO.File]::ReadAllText("$prefix.repeat.h") -or
        [IO.File]::ReadAllText("$prefix.json") -cne [IO.File]::ReadAllText("$prefix.repeat.json")) {
        throw 'Task value types C/header determinism failed'
    }
    & gcc @strict test/cases/xlir_task_value_types_runtime.c test/cases/xlir_task_value_types_host.c release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Task value types generated C strict build failed' }
    # This deterministic substrate test must pass before a lucky scheduling of
    # the task stress test can be accepted as a successful lifetime gate.
    & gcc @strict test/cases/xlir_task_value_types_release_runtime.c release/xl.dll -o "$prefix.release.exe"
    if ($LASTEXITCODE -ne 0) { throw 'XRT nonfinal reference release contract build failed' }
    & "$prefix.release.exe"
    if ($LASTEXITCODE -ne 0) { throw 'XRT must release independent nonfinal owners during value lookup' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Task value types runtime ownership or value semantics failed' }
    & release/xl.exe run test/cases/xlir_task_value_types_runtime.c test/cases/xlir_task_value_types_host.c --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Task value types generated C-host JIT failed' }
    & release/xl.exe build test/cases/xlir_task_value_types_runtime.c test/cases/xlir_task_value_types_host.c src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Task value types generated C-host AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Task value types generated C-host AOT runtime failed' }
    if ([IO.File]::ReadAllText("$prefix.json.txt") -cne [IO.File]::ReadAllText("$prefix.repeat.json.txt")) {
        throw 'Task value types text IR determinism failed'
    }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    if (@($ir.callbackBoundaries).Count -ne 16 -or @($ir.callbackCaptures).Count -ne 31 -or
        @($ir.callbackBoundaries | Where-Object { $_.transportAbi -eq 1 }).Count -ne 16 -or
        @($ir.instructions | Where-Object { $_.callbackBoundary -ne 0 }).Count -ne 16) {
        throw 'JSON dump lost the exact callback activation and capture program'
    }
} finally { Pop-Location }
Write-Output 'XLIR task value types passed: nominal and dynamic values, typed collections, callables, nested Futures, resource borrowing, exact type binding, verifier rejection, memory balance, strict C and JIT/AOT.'
exit 0
