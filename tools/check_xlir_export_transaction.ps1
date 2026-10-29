param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_export_transaction.xl'
    $prefix = 'build/test/xlir_export_transaction'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Export transaction lowering failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    if ($ir.exports.Count -ne 13) { throw 'Export transaction fixture lost return/callback coverage' }
    foreach ($entry in $ir.exports) {
        $body = @($ir.callTargets | Where-Object symbol -eq $entry.symbol)
        if ($entry.errorAbi -ne 1 -or $body.Count -ne 1 -or $entry.bodyErrorAbi -ne $body[0].errorAbi) {
            throw 'Export entry transaction and private body effects must be independently explicit'
        }
    }
    $product = @($ir.exports | Where-Object cName -eq 'xl6_export_tx_product')[0]
    $accepted = @($ir.exports | Where-Object cName -eq 'xl6_export_tx_accept_product')[0]
    # Auto-linked language exports carry managed ownership; quoted raw-C
    # exports deliberately reject those return types in semantic analysis.
    $aliases = @($ir.exports | Where-Object cName -Like 'xl6_export_tx_*' | ForEach-Object {
        '#define ' + $_.cName.Substring('xl6_export_'.Length) + ' ' + $_.cName
    }) -join "`n"
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix`_bind.h"),
        "#define TX_PRODUCT xlir_product_t$($product.returnType)`n#define TX_ACCEPT_PRODUCT xlir_product_t$($accepted.returnType)`n$aliases`n",
        [Text.UTF8Encoding]::new($false))
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Export transaction C/header emission failed' }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ($LASTEXITCODE -ne 0 -or ![System.Linq.Enumerable]::SequenceEqual(
        [byte[]][IO.File]::ReadAllBytes("$prefix.c"), [byte[]][IO.File]::ReadAllBytes("$prefix.repeat.c"))) {
        throw 'Export transaction C is not deterministic'
    }
    $strict = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-O0',
        '-Wno-unused-function', '-Wno-unused-label', '-Wno-unused-variable',
        '-Wno-unused-parameter', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers', '-Wno-missing-braces', '-I.', '-Ibuild/test')
    $runtime = 'test/cases/xlir_export_transaction_runtime.c'
    $hostSource = 'test/cases/xlir_export_transaction_host.c'
    $publicHost = 'test/cases/xlir_export_transaction_header_host.c'
    & gcc -x c++ -std=c++11 -Wall -Wextra -Werror -Ibuild/test -fsyntax-only $publicHost
    if ($LASTEXITCODE -ne 0) { throw 'Public export transaction header is not C++ compatible' }
    & gcc @strict $runtime $hostSource $publicHost release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Export transaction strict separate C compilation failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Export transaction ownership/error runtime failed' }
    & release/xl.exe run $runtime $hostSource $publicHost --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Export transaction C-host JIT failed' }
    & release/xl.exe build $runtime $hostSource $publicHost src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Export transaction standalone AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Export transaction standalone AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c', 'src/type/xl6_type_model.c',
        'src/ir/xl6_ir.c', 'src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c', 'src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c', 'src/common/xl6_output.c', 'src/common/xl6_console.c',
        'src/lexer/xl6_token.c', 'src/lexer/xl6_lexer_api.c', 'src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c', 'src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c', 'src/sem/xl6_protocol.c', 'src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c', 'src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c', 'src/stdlib/xl6_stdlib_data.c')
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_export_transaction_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Export transaction verifier contract build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Export transaction verifier/Cgen mutation contract failed' }
} finally { Pop-Location }
Write-Output 'XLIR export transaction passed: diagnostic-independent public errors, exact body carriers, callback reentry/catch, partial parameter rollback, pending managed results, deterministic C, strict memory accounting, public header and JIT/AOT.'
exit 0
