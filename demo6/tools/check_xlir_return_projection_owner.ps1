param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_return_projection_owner.xl'
    $prefix = 'build/test/xlir_return_projection_owner'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Return projection lowered IR failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    if (@($ir.instructions | Where-Object kindName -eq 'product-make').Count -lt 12 -or
        @($ir.instructions | Where-Object { $_.kindName -eq 'cell-load' -and $_.resultOwnershipName -eq 'borrow' }).Count -lt 4 -or
        @($ir.instructions | Where-Object { $_.runtimeOpName -eq 'xvalue-retain' -and $_.resultOwnershipName -eq 'move' }).Count -lt 4 -or
        @($ir.instructions | Where-Object { $_.runtimeOpName -eq 'xvalue-dynamic-field-get-literal' -and $_.resultOwnershipName -ne 'borrow' }).Count -ne 0) {
        throw 'Return projection lost product, retained-result, borrowed-cell or dynamic-view ownership contracts'
    }
    & release/xl.exe ir $source --lowered --format text --emit "$prefix.ir"
    if ($LASTEXITCODE -ne 0 -or ![IO.File]::ReadAllText("$prefix.ir").Contains('result-ownership=borrow')) {
        throw 'Return projection text IR does not expose producer ownership'
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Return projection C/header failed' }
    $header = [IO.File]::ReadAllText("$prefix.h")
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne $header) { throw 'Return projection C/header is not deterministic' }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $runtime = 'test/cases/xlir_return_projection_owner_runtime.c'
    $hostSource = 'test/cases/xlir_return_projection_owner_host.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return projection strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return projection lost identity, ownership, cleanup order or failure outcome' }
    & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Return projection C-host JIT failed' }
    & release/xl.exe build $runtime $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return projection AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Return projection AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_return_projection_owner_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return projection verifier contract build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return projection verifier mutation failed' }
    # Preserve the original admission representative that exposed implicit
    # conditional/coalesce transfers, including its independent C ABI host.
    $regressionSource = 'test/cases/cgen_var_return_exprs.xl'
    & release/xl.exe cgen $regressionSource --backend xlir --emit build/test/xlir_var_return_exprs_generated.c
    if ($LASTEXITCODE -ne 0) { throw 'Original multi-value conditional/coalesce return regressed' }
    & gcc @strict test/cases/xlir_var_return_exprs_host.c test/cases/cgen_class_host.c release/xl.dll -o "$prefix.original.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Original return-expression C ABI host build failed' }
    & "$prefix.original.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Original return-expression C ABI host failed' }
} finally { Pop-Location }
Write-Output 'XLIR return projection owners passed: physical carrier versus narrowed value, repeated aliases, nullable strings, boxed scalars, partial evaluation, finally, mutable captures and path-dependent conditional/coalesce owners; shared SSA ownership, independent product inputs, real OOM, allocation balance, verifier rejection, deterministic C/header and C-host JIT/AOT.'
exit 0
