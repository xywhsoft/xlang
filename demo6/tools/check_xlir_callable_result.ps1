param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    # Compile this verifier/lowering contract from current sources. The DLL is
    # only its unchanged runtime dependency; do not test a stale compiler DLL.
    $prefix = 'build/test/xlir_callable_result'
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
    & gcc @strict test/cases/xlir_callable_result_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Callable result source contract build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Callable result lowering and ownership contract failed' }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Callable result repeated generation failed' }
    foreach ($extension in @('c', 'h', 'json', 'h.bind.h')) {
        if ([IO.File]::ReadAllText("$prefix.$extension") -cne [IO.File]::ReadAllText("$prefix.repeat.$extension")) {
            throw "Callable result $extension determinism failed"
        }
    }
    & gcc @strict test/cases/xlir_callable_result_runtime.c test/cases/xlir_callable_result_host.c release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Callable result strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Callable result fault rollback, invocation or memory balance failed' }
    & release/xl.exe run test/cases/xlir_callable_result_runtime.c test/cases/xlir_callable_result_host.c --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Callable result generated C-host JIT failed' }
    & release/xl.exe build test/cases/xlir_callable_result_runtime.c test/cases/xlir_callable_result_host.c src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Callable result generated C-host AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Callable result generated C-host AOT run failed' }
} finally { Pop-Location }
Write-Output 'XLIR callable result passed: verified result programs, mutation rejection, product/closure and overflow rollback, primary errors, atomic commit, memory balance, strict C and generated C-host JIT/AOT.'
exit 0
