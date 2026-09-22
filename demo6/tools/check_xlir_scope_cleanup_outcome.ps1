param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_scope_cleanup_outcome.xl'
    $prefix = 'build/test/xlir_scope_cleanup_outcome'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    if (!(Test-Path -LiteralPath build/test/xl.dll) -or
        (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
        Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    }
    foreach ($stage in @('typed', 'lowered')) {
        $arguments = @('ir', $source, '--format', 'json', '--emit', "$prefix.$stage.json")
        if ($stage -eq 'lowered') { $arguments += '--lowered' }
        & release/xl.exe @arguments
        if ($LASTEXITCODE -ne 0) { throw "Scope cleanup $stage IR failed" }
        $ir = [IO.File]::ReadAllText("$prefix.$stage.json") | ConvertFrom-Json
        $plans = @($ir.exitPlans | Where-Object pointName -eq 'after-scope-failure')
        if ($plans.Count -lt 20) { throw 'Scope cleanup lost its explicit failure plans' }
        foreach ($plan in $plans) {
            $normal = @($ir.exitPlans | Where-Object { $_.origin -eq $plan.origin -and $_.pointName -eq 'after-scope' })
            if ($normal.Count -ne 1 -or $plan.targetScopeDepth -gt $normal[0].targetScopeDepth) {
                throw 'Scope failure has no unique completed-scope plan and enclosing boundary'
            }
        }
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Scope cleanup Cgen/header failed' }
    $headerText = [IO.File]::ReadAllText("$prefix.h")
    # The include guard intentionally contains the output basename. Compare
    # repeated output at the same destination rather than two different guards.
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne $headerText) {
        throw 'Scope cleanup C/header output is not deterministic'
    }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $runtime = 'test/cases/xlir_scope_cleanup_outcome_runtime.c'
    $hostSource = 'test/cases/xlir_scope_cleanup_outcome_host.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope cleanup strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope cleanup lost an outcome, owner or outer handler lifetime' }
    & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Scope cleanup C-host JIT failed' }
    $artifactRuntimeSources = @(
        'src/runtime/xl6_slot.c', 'src/runtime/xl6_code_lease.c',
        'src/runtime/xl6_cell.c', 'src/runtime/xl6_closure.c',
        'src/runtime/xl6_code_finalizer.c')
    & release/xl.exe build $runtime $hostSource @artifactRuntimeSources src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope cleanup AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Scope cleanup AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_scope_cleanup_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope cleanup verifier unit build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope cleanup verifier mutations failed' }
} finally { Pop-Location }
Write-Output 'XLIR normal lexical cleanup outcomes passed: exact completed/remaining owner plans, handler boundaries, first failure with NULL diagnostics/OOM, nested catch payloads, cell promotion/projection lifetimes, deterministic C/header and C-host JIT/AOT.'
exit 0
