param([string]$Root = '', [switch]$ProbeBefore)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_error_unwind.xl'
    $prefix = 'build/test/xlir_error_unwind'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Exceptional cleanup lowering failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $async = @($ir.callTargets | Where-Object { $_.kind -eq 4 -and $_.asyncMachine -ne 4294967295 })
    if ($async.Count -ne 1) { throw 'Unwind fixture needs one exact async target' }
    [IO.File]::WriteAllText((Join-Path (Get-Location) "$prefix`_bind.h"),
        "#define UNWIND_ASYNC_START xlir_async_start_$($async[0].symbol)`n", [Text.UTF8Encoding]::new($false))
    if (!$ProbeBefore) {
        $begins = @($ir.instructions | Where-Object kindName -eq 'error-unwind-begin')
        $ends = @($ir.instructions | Where-Object kindName -eq 'error-unwind-end')
        if ($begins.Count -lt 10 -or $begins.Count -ne $ends.Count) { throw 'Unwind region coverage missing' }
        foreach ($begin in $begins) {
            $end = @($ends | Where-Object errorScope -eq $begin.errorScope)
            if ($begin.errorScope -ne $begin.index + 1 -or $end.Count -ne 1) { throw 'Unwind identity mismatch' }
        }
        $normalBegins = @($ir.instructions | Where-Object kindName -eq 'cleanup-begin')
        $normalEnds = @($ir.instructions | Where-Object {
            $_.kindName -eq 'cleanup-end' -and
            $ir.instructions[$_.errorScope - 1].kindName -eq 'cleanup-begin'
        })
        if ($normalBegins.Count -lt 8 -or $normalBegins.Count -ne $normalEnds.Count) {
            throw 'Normal cleanup region coverage missing'
        }
        foreach ($begin in $normalBegins) {
            $end = @($normalEnds | Where-Object errorScope -eq $begin.errorScope)
            if ($begin.errorScope -ne $begin.index + 1 -or $end.Count -ne 1 -or $end[0].result -eq 0) {
                throw 'Normal cleanup identity/outcome mismatch'
            }
            $test = $ir.instructions[$end[0].index + 1]
            if ($test.kindName -ne 'error-test' -or $test.operands.Count -ne 1 -or
                $test.operands[0] -ne $end[0].result -or $test.errorScope -ne 0) {
                throw 'Normal cleanup must have an immediate exclusive outcome edge'
            }
        }
        foreach ($entry in @('unwind_property', 'unwind_operator', 'unwind_closure', 'unwind_constructor',
                            'unwind_operator_caught', 'unwind_closure_callee',
                            'unwind_setter', 'unwind_indexed_setter',
                            'normal_getter', 'normal_escape', 'normal_pair', 'normal_closure',
                            'normal_owned_result', 'normal_constructor', 'normal_compound', 'unwind_compound')) {
            if (@($ir.exports | Where-Object cName -eq $entry).Count -ne 1) {
                throw "Call completion runtime coverage missing: $entry"
            }
        }
        if (@($ir.instructions | Where-Object kindName -eq 'closure-call').Count -eq 0) {
            throw 'Call completion needs a real typed closure invocation'
        }
        foreach ($call in @($ir.instructions | Where-Object { $_.kindName -in @('call', 'closure-call', 'async-start') })) {
            $callee = @($ir.callTargets | Where-Object symbol -eq $call.targetSymbol)
            if ($call.kindName -eq 'closure-call' -or ($callee.Count -eq 1 -and $callee[0].errorAbi -eq 2)) {
                $test = $ir.instructions[$call.index + 1]
                if ($test.kindName -ne 'error-test' -or $test.operands[0] -ne $call.result) {
                    throw 'Call outcome must precede potentially throwing argument cleanup'
                }
            }
        }
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c"
    if ($LASTEXITCODE -ne 0) { throw 'Exceptional cleanup Cgen failed' }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c")) {
        throw 'Exceptional cleanup C is not deterministic'
    }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0',
        '-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $runtime = 'test/cases/xlir_error_unwind_runtime.c'
    $hostSource = 'test/cases/xlir_error_unwind_host.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Exceptional cleanup strict C build failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Exceptional cleanup lost original error or ownership' }
    if (!$ProbeBefore) {
        & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke
        if ($LASTEXITCODE -ne 0) { throw 'Exceptional cleanup C-host JIT failed' }
        & release/xl.exe build $runtime $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Exceptional cleanup AOT build failed' }
        & "$prefix.aot.exe" --smoke
        if ($LASTEXITCODE -ne 0) { throw 'Exceptional cleanup AOT runtime failed' }
        $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
            'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
            'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
            'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
            'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
            'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
            'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
            'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
            'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
        & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_error_unwind_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Exceptional cleanup contract build failed' }
        & "$prefix.contract.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Exceptional cleanup verifier mutations failed' }
    }
} finally { Pop-Location }
Write-Output 'XLIR exceptional cleanup passed: first-failure preservation, NULL diagnostics, catch isolation, complete reverse cleanup, sync/async/generator and C-host JIT/AOT.'
exit 0
