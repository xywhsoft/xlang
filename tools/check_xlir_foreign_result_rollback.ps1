param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_foreign_result_rollback.xl'
    $prefix = 'build/test/xlir_foreign_result_rollback'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Foreign result rollback lowering failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $async = @($ir.callTargets | Where-Object kind -eq 4)
    $escape = @($ir.exports | Where-Object cName -eq 'rollback_escape')
    if ($escape.Count -ne 1) { throw 'Rollback fixture requires one exact unhandled language target' }
    if ($async.Count -ne 1) { throw 'Rollback fixture requires one exact async launch target' }
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix`_bind.h"),
        "#define ROLLBACK_ASYNC_START xlir_async_start_$($async[0].symbol)`n#define ROLLBACK_ESCAPE xlir_fn_$($escape[0].symbol)`n",
        [Text.UTF8Encoding]::new($false))
    $owned = @($ir.callTargets | Where-Object cName -eq 'rollback_owned_open')
    $borrowed = @($ir.callTargets | Where-Object cName -eq 'rollback_borrowed_open')
    $destroy = @($ir.callTargets | Where-Object cName -eq 'rollback_owned_close')
    $rollbacks = @($ir.instructions | Where-Object {
        $_.kindName -eq 'foreign-result-rollback' -and $_.targetSymbol -eq $destroy[0].symbol
    })
    if ($owned.Count -ne 1 -or $borrowed.Count -ne 1 -or $destroy.Count -ne 1 -or $rollbacks.Count -ne 4) {
        throw 'Foreign result fixture lost sync/async/generator ownership or its borrowed constructor path'
    }
    foreach ($rollback in $rollbacks) {
        $call = @($ir.instructions | Where-Object result -eq $rollback.operands[0])
        if ($call.Count -ne 1 -or $call[0].kindName -ne 'call' -or
            $call[0].targetSymbol -ne $owned[0].symbol -or $rollback.targetSymbol -ne $destroy[0].symbol) {
            throw 'Foreign result rollback lost exact factory/destructor dataflow'
        }
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c"
    if ($LASTEXITCODE -ne 0) { throw 'Foreign result rollback Cgen failed' }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ($LASTEXITCODE -ne 0 -or ![System.Linq.Enumerable]::SequenceEqual(
        [byte[]][IO.File]::ReadAllBytes("$prefix.c"), [byte[]][IO.File]::ReadAllBytes("$prefix.repeat.c"))) {
        throw 'Foreign result rollback C is not deterministic'
    }
    $strict = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-O0',
        '-Wno-unused-function', '-Wno-unused-label', '-Wno-unused-variable',
        '-Wno-unused-parameter', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers', '-I.', '-Ibuild/test')
    $runtime = 'test/cases/xlir_foreign_result_rollback_runtime.c'
    $hostSource = 'test/cases/xlir_foreign_result_rollback_host.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Foreign result rollback strict C build failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Foreign result rollback runtime ownership/error isolation failed' }
    & "$prefix.exe" --export-probe
    if ($LASTEXITCODE -ne 0) { throw 'Export must preserve the rolled-back NULL-diagnostic failure bit' }
    & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Foreign result rollback C-host JIT failed' }
    & release/xl.exe build $runtime $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Foreign result rollback standalone AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Foreign result rollback standalone AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c', 'src/type/xl6_type_model.c',
        'src/ir/xl6_ir.c', 'src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c', 'src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c', 'src/common/xl6_output.c', 'src/common/xl6_console.c',
        'src/lexer/xl6_token.c', 'src/lexer/xl6_lexer_api.c', 'src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c', 'src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c', 'src/sem/xl6_protocol.c', 'src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c', 'src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c', 'src/stdlib/xl6_stdlib_data.c')
    # The existing semantic diagnostic formatter has truncation warnings;
    # this exception applies only to the compiler harness, never emitted C.
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_foreign_result_rollback_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Foreign result rollback verifier contract build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Foreign result rollback verifier/Cgen mutations failed' }
} finally { Pop-Location }
Write-Output 'XLIR foreign result rollback passed: exact ownership partition, original error priority, native reentry, NULL results/diagnostics, borrowed results, deterministic C, strict memory accounting and JIT/AOT.'
exit 0
