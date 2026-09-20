param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_conditional_nullable_join.xl'
    $prefix = 'build/test/xlir_conditional_nullable_join'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional nullable join lowered IR failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $stringJoins = @($ir.nodes | Where-Object { $_.kindName -eq 'conditional' -and $_.storageType -in @(
        $ir.types | Where-Object { $_.kind -eq 8 -and $_.cAbi -eq 25 } | ForEach-Object id) } | ForEach-Object id)
    if ($stringJoins.Count -eq 0 -or @($ir.instructions | Where-Object {
        $_.origin -in $stringJoins -and $_.runtimeOpName -eq 'string-nullable-copy'
    }).Count -eq 0) { throw 'Nullable string branch lost its independent owning copy' }
    if (@($ir.instructions | Where-Object runtimeOpName -eq 'xvalue-project-nullable-xvalue-retain').Count -eq 0) {
        throw 'Nullable class flow from var lost its exact owning projection'
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional nullable join C/header failed' }
    $header = [IO.File]::ReadAllText("$prefix.h")
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne $header) { throw 'Conditional nullable join C/header is not deterministic' }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $runtime = 'test/cases/xlir_conditional_nullable_join_runtime.c'
    $hostSource = 'test/cases/xlir_conditional_nullable_join_host.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional nullable join strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional nullable join lifetime, value or lazy evaluation failed' }
    & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Conditional nullable join C-host JIT failed' }
    & release/xl.exe build $runtime $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional nullable join AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Conditional nullable join AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_conditional_nullable_join_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional nullable join semantic/verifier contract build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Conditional nullable join semantic/verifier contract failed' }
    & release/xl.exe cgen test/cases/perf_json_ast_lifecycle.xl --backend xlir --emit "$prefix.original.c"
    if ($LASTEXITCODE -ne 0) { throw 'Original narrowed integer conversion admission representative regressed' }
} finally { Pop-Location }
Write-Output 'XLIR conditional nullable joins passed: absence-preserving types, scoped branch narrowing, lazy definite assignment, explicit value ownership, physical null tests, exact var projections, deterministic C/header, strict allocation balance and C-host JIT/AOT.'
exit 0
