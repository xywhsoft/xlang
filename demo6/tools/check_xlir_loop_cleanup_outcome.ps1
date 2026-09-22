param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_loop_cleanup_outcome.xl'
    $prefix = 'build/test/xlir_loop_cleanup_outcome'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    if (!(Test-Path -LiteralPath build/test/xl.dll) -or
        (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
        Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_loop_cleanup_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Loop cleanup verifier build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Loop cleanup verifier mutation failed' }
    # The first invocation already rejects every mutation.  The repeat lane
    # only proves byte-for-byte emission determinism; replaying thousands of
    # identical verifier mutations used to double this gate's runtime.
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json" --emit-only
    if ($LASTEXITCODE -ne 0) { throw 'Loop cleanup repeated verifier mutation failed' }
    foreach ($suffix in @('c','h','json','json.typed.json')) {
        if ([IO.File]::ReadAllText("$prefix.$suffix") -cne [IO.File]::ReadAllText("$prefix.repeat.$suffix")) {
            throw "Loop cleanup same-source C/header/IR is not deterministic: $suffix"
        }
    }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    if (@($ir.loopOwners).Count -lt 20 -or
        @($ir.instructions | Where-Object { $_.kindName -eq 'cleanup-begin' -and $_.cleanupPlan -gt 0 }).Count -lt 20) {
        throw 'Loop physical owners or isolated cleanup transactions are missing'
    }
    if (@($ir.exitPlans | Where-Object pointName -eq 'after-loop-iteration-failure').Count -lt 10 -or
        @($ir.exitPlans | Where-Object pointName -eq 'after-loop-resource-failure').Count -lt 10) {
        throw 'Loop cleanup failure phases are missing'
    }
    $runtime = 'test/cases/xlir_loop_cleanup_outcome_runtime.c'
    $hostSource = 'test/cases/xlir_loop_cleanup_outcome_host.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Loop cleanup strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Loop cleanup lost an outcome, pending owner or visible binding' }
    & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Loop cleanup C-host JIT failed' }
    $artifactRuntimeSources = @(
        'src/runtime/xl6_slot.c', 'src/runtime/xl6_code_lease.c',
        'src/runtime/xl6_cell.c', 'src/runtime/xl6_closure.c',
        'src/runtime/xl6_capture.c', 'src/runtime/xl6_value_iterator.c',
        'src/runtime/xl6_generator.c', 'src/runtime/xl6_code_finalizer.c')
    & release/xl.exe build $runtime $hostSource @artifactRuntimeSources src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Loop cleanup AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Loop cleanup AOT runtime failed' }
} finally { Pop-Location }
Write-Output 'XLIR loop cleanup passed: explicit item/resource owners, single-close CFG lifetimes, normal/break/continue/return outcomes, nested owner order, pending-result rollback, inner/outer handlers, NULL/OOM first-error preservation, generator next success/end/failure separation, allocation balance, verifier rejection and C-host JIT/AOT.'
exit 0
