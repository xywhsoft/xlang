param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    # Compile this verifier/lowering contract from current sources. The DLL is
    # only its unchanged runtime dependency; do not test a stale compiler DLL.
    $prefix = 'build/test/xlir_callback_boundary'
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
    & gcc @strict test/cases/xlir_callback_boundary_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Callback boundary source contract build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Callback boundary module mutation or CFG contract failed' }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne [IO.File]::ReadAllText("$prefix.repeat.h") -or
        [IO.File]::ReadAllText("$prefix.json") -cne [IO.File]::ReadAllText("$prefix.repeat.json")) {
        throw 'Callback boundary C/header determinism failed'
    }
    & "$prefix.contract.exe" "$prefix.future_only.c" "$prefix.future_only.h" "$prefix.future_only.json" --future-only
    if ($LASTEXITCODE -ne 0) { throw 'Future-only callback source contract failed' }
    & gcc @strict test/cases/xlir_callback_boundary_future_only_runtime.c test/cases/xlir_callback_boundary_future_only_host.c release/xl.dll -o "$prefix.future_only.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Future-only callback standalone strict C failed' }
    & "$prefix.future_only.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Future-only callback standalone runtime failed' }
    & gcc @strict test/cases/xlir_callback_boundary_runtime.c test/cases/xlir_callback_boundary_host.c release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Callback boundary generated C strict build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Callback boundary accessor transaction/runtime semantics failed' }
    & release/xl.exe run test/cases/xlir_callback_boundary_runtime.c test/cases/xlir_callback_boundary_host.c --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Callback boundary generated C-host JIT failed' }
    & release/xl.exe build test/cases/xlir_callback_boundary_runtime.c test/cases/xlir_callback_boundary_host.c src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Callback boundary generated C-host AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Callback boundary generated C-host AOT runtime failed' }
    if ([IO.File]::ReadAllText("$prefix.h.bind.h") -cne [IO.File]::ReadAllText("$prefix.repeat.h.bind.h")) {
        throw 'Callback boundary IR binding determinism failed'
    }
    if ([IO.File]::ReadAllText("$prefix.json.txt") -cne [IO.File]::ReadAllText("$prefix.repeat.json.txt")) {
        throw 'Callback boundary text IR determinism failed'
    }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    if (@($ir.callbackBoundaries).Count -ne 5 -or @($ir.callbackCaptures).Count -ne 12 -or
        @($ir.callbackBoundaries | Where-Object { $_.transportAbi -eq 2 }).Count -ne 4 -or
        @($ir.instructions | Where-Object { $_.callbackBoundary -ne 0 }).Count -ne 5) {
        throw 'JSON dump lost the exact callback activation and capture program'
    }
} finally { Pop-Location }
Write-Output 'XLIR callback boundary passed: verified captures, cleanup-before-publication, cancellation barrier, primary errors, owned result rollback/forwarding, allocation faults, memory balance, strict C and JIT/AOT.'
exit 0
