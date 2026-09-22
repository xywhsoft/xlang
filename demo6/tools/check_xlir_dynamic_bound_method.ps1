param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    # Compile this verifier/lowering contract from current sources. The DLL is
    # only its unchanged runtime dependency; do not test a stale compiler DLL.
    $prefix = 'build/test/xlir_dynamic_bound_method'
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
    & gcc @strict test/cases/xlir_dynamic_bound_method_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Dynamic bound method source contract build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.bind.h"
    if ($LASTEXITCODE -ne 0) { throw 'Dynamic bound method module mutation or CFG contract failed' }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.bind.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne [IO.File]::ReadAllText("$prefix.repeat.h") -or
        [IO.File]::ReadAllText("$prefix.bind.h") -cne [IO.File]::ReadAllText("$prefix.repeat.bind.h")) {
        throw 'Dynamic bound method C/header determinism failed'
    }
    foreach ($case in @('cgen_dynamic_method_bound_lifetime','cgen_dynamic_method_inheritance','cgen_dynamic_method_overload','cgen_generic_dynamic_method','cgen_dynamic_method_arguments')) {
        & "$prefix.contract.exe" "test/cases/$case.xl" "$prefix.$case.c"
        if ($LASTEXITCODE -ne 0) { throw "Dynamic bound method compatibility source failed: $case" }
        & gcc @strict -DXLIR_GENERATED_MODULE_USER_ENTRY -DXLIR_GENERATED_MODULE_PREINCLUDED -include "$prefix.$case.c" -c test/cases/xlir_generated_module_host.c -o "$prefix.$case.o"
        if ($LASTEXITCODE -ne 0) { throw "Dynamic bound method compatibility C failed: $case" }
        & gcc @strict "$prefix.$case.o" test/cases/cgen_class_host.c release/xl.dll -o "$prefix.$case.exe"
        if ($LASTEXITCODE -ne 0) { throw "Dynamic bound method compatibility link failed: $case" }
        & "$prefix.$case.exe"
        if ($LASTEXITCODE -ne 0) { throw "Dynamic bound method compatibility runtime failed: $case" }
    }
    & gcc @strict test/cases/xlir_dynamic_bound_method_runtime.c test/cases/xlir_dynamic_bound_method_host.c release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Dynamic bound method generated C strict build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Dynamic bound method accessor transaction/runtime semantics failed' }
    & release/xl.exe run test/cases/xlir_dynamic_bound_method_runtime.c test/cases/xlir_dynamic_bound_method_host.c --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Dynamic bound method generated C-host JIT failed' }
    & release/xl.exe build test/cases/xlir_dynamic_bound_method_runtime.c test/cases/xlir_dynamic_bound_method_host.c src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Dynamic bound method generated C-host AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Dynamic bound method generated C-host AOT runtime failed' }
} finally { Pop-Location }
Write-Output 'XLIR dynamic bound method passed: retained receiver environment ABI, inherited overload selection, borrowed-frame argument transfer, checked outcomes, strict C, JIT/AOT, mutation rejection and allocation baseline restoration.'
exit 0
