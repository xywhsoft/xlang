param([string]$Root = '', [switch]$ProbeScopeExits)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_class_cleanup_outcome.xl'
    $prefix = 'build/test/xlir_class_cleanup_outcome'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Class cleanup outcome lowering failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $programs = @($ir.functions | Where-Object kindName -eq 'class-fini')
    if ($programs.Count -ne 3) { throw 'Class cleanup fixture needs three exact class-fini programs' }
    foreach ($layout in @($ir.classLayouts | Where-Object destroyStepCount -gt 0)) {
        if ($layout.destroyFunction -le 0) { throw 'Class layout has no lowered finalizer function' }
        $program = $ir.functions[$layout.destroyFunction - 1]
        $body = @($ir.instructions | Where-Object {
            $_.index -ge $program.firstInstruction -and $_.index -lt $program.firstInstruction + $program.instructionCount
        })
        $calls = @($body | Where-Object kindName -eq 'call')
        if ($program.kindName -ne 'class-fini' -or $program.errorAbi -ne 'may-raise' -or
            $program.parameterCount -ne 1 -or $calls.Count -ne $layout.destroyStepCount -or
            $body[0].kindName -ne 'load' -or $body[1].kindName -ne 'cleanup-inherit-begin' -or
            @($body | Where-Object kindName -eq 'cleanup-fail').Count -ne $calls.Count -or
            @($body | Where-Object kindName -eq 'cleanup-capture').Count -ne $calls.Count -or
            $body[-4].kindName -ne 'cleanup-end' -or $body[-3].kindName -ne 'error-test' -or
            $body[-3].operands[0] -ne $body[-4].result -or
            $body[-2].kindName -ne 'throw' -or $body[-1].kindName -ne 'return') {
            throw 'Class cleanup must retain exact ordered CALLs and an explicit aggregate outcome'
        }
        for ($i = 0; $i -lt $calls.Count; ++$i) {
            if ($calls[$i].targetSymbol -ne $ir.classDestroySteps[$layout.firstDestroyStep + $i].targetSymbol) {
                throw 'Class cleanup changed the derived-to-base order'
            }
        }
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Class cleanup Cgen/header failed' }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c" --emit-header "$prefix.repeat.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c")) {
        throw 'Class cleanup Cgen is not deterministic'
    }
    $cText = [IO.File]::ReadAllText("$prefix.c")
    foreach ($layout in @($ir.classLayouts | Where-Object destroyStepCount -gt 0)) {
        $program = $ir.functions[$layout.destroyFunction - 1]
        if ($program.reachability -eq 'unreachable') { continue }
        $helper = [regex]::Match($cText, "static void xlir_class_destroy_finalizer_t$($layout.type)\(xvalue\* value, void\* data\)\s*\{(?<body>.*?)\}", 'Singleline')
        if (!$helper.Success -or [regex]::Matches($helper.Groups['body'].Value, 'xlir_fn_\d+\(value\)').Count -ne 1 -or
            !$helper.Groups['body'].Value.Contains("xlir_fn_$($program.symbol)(value)") -or
            !$helper.Groups['body'].Value.Contains('outcome.ok')) {
            throw 'Native finalizer adapter must consume exactly one verified class-fini outcome'
        }
    }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $runtime = 'test/cases/xlir_class_cleanup_outcome_runtime.c'
    $hostSource = 'test/cases/xlir_class_cleanup_outcome_host.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Class cleanup strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Class cleanup lost an outcome or resource' }
    & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Class cleanup C-host JIT failed' }
    & release/xl.exe build $runtime $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Class cleanup AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Class cleanup AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_class_cleanup_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Class cleanup verifier unit build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Class cleanup verifier mutations failed' }
    # Keep -ProbeScopeExits accepted for old invocations; this is now required.
    & "$prefix.strict.exe" --probe-scopes
    if ($LASTEXITCODE -ne 0) { throw 'Lexical cleanup failure must enter catch' }
} finally { Pop-Location }
Write-Output 'XLIR class cleanup outcome passed: borrowed receiver, complete derived-to-base/field cleanup, inherited and missing primary diagnostics, real OOM, reentry/retry, deterministic C, verifier mutations, C-host JIT/AOT and required normal lexical-exit catch routing.'
exit 0
