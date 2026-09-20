param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    # Compile this verifier/lowering contract from current sources. The DLL is
    # only its unchanged runtime dependency; do not test a stale compiler DLL.
    $prefix = 'build/test/xlir_index_conditional_assign'
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
    & gcc @strict test/cases/xlir_index_conditional_assign_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index source contract build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.bind.h"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index module mutation or CFG contract failed' }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.bind.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne [IO.File]::ReadAllText("$prefix.repeat.h") -or
        [IO.File]::ReadAllText("$prefix.bind.h") -cne [IO.File]::ReadAllText("$prefix.repeat.bind.h")) {
        throw 'Conditional index C/header determinism failed'
    }
    & gcc @strict test/cases/xlir_index_conditional_assign_runtime.c test/cases/xlir_index_conditional_assign_host.c release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index generated C strict build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index storage snapshot/runtime semantics failed' }
    & release/xl.exe run test/cases/xlir_index_conditional_assign_runtime.c test/cases/xlir_index_conditional_assign_host.c --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index generated C-host JIT failed' }
    & release/xl.exe build test/cases/xlir_index_conditional_assign_runtime.c test/cases/xlir_index_conditional_assign_host.c src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index generated C-host AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index generated C-host AOT runtime failed' }
    & gcc @strict test/cases/xlir_nullable_index_assign_if_null_host.c release/xl.dll -o "$prefix.baseline.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index baseline strict C build failed' }
    & "$prefix.baseline.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index baseline generated-C runtime failed' }
    & gcc @strict -DXLIR_GENERATED_MODULE_PREINCLUDED -DXLIR_GENERATED_MODULE_USER_ENTRY -include build/test/xlir_nullable_index_assign_if_null_may_raise_generated.c test/cases/xlir_generated_module_host.c release/xl.dll -o "$prefix.raise.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index raising RHS strict C build failed' }
    & "$prefix.raise.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index raising RHS generated-C runtime failed' }
    & release/xl.exe cgen test/cases/xlir_nullable_index_assign_if_null_may_raise.xl --backend legacy --emit "$prefix.raise.legacy.c"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index raising RHS legacy generation failed' }
    & gcc @strict "$prefix.raise.legacy.c" release/xl.dll -o "$prefix.raise.legacy.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index raising RHS legacy strict C build failed' }
    & "$prefix.raise.legacy.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional index raising RHS legacy differential failed' }
} finally { Pop-Location }
Write-Output 'XLIR conditional index passed: lazy typed/dynamic address transactions, raising RHS, ownership/merge rejection, genuine suspension/short-circuit, deterministic C/header, strict C, JIT/AOT, raising RHS legacy differential and module finalization.'
exit 0
