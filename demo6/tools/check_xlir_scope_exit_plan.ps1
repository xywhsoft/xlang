param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_scope_exit_plan.xl'
    $prefix = 'build/test/xlir_scope_exit_plan'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    foreach ($stage in @('typed', 'lowered')) {
        $arguments = @('ir', $source, '--format', 'json', '--emit', "$prefix.$stage.json")
        if ($stage -eq 'lowered') { $arguments += '--lowered' }
        & release/xl.exe @arguments
        if ($LASTEXITCODE -ne 0) { throw "Scope exit plan $stage IR failed" }
        $ir = [IO.File]::ReadAllText("$prefix.$stage.json") | ConvertFrom-Json
        $blocks = @($ir.nodes | Where-Object kindName -eq 'block')
        $plans = @($ir.exitPlans | Where-Object pointName -eq 'after-scope')
        if ($plans.Count -ne $blocks.Count -or $plans.Count -lt 5) {
            throw 'Every IR BLOCK must have one explicit AFTER_SCOPE plan'
        }
        foreach ($block in $blocks) {
            $plan = @($plans | Where-Object origin -eq $block.id)
            if ($plan.Count -ne 1 -or $plan[0].kind -ne 0 -or
                $plan[0].targetScopeDepth -ne $block.scopeDepth -or $plan[0].targetRegion -ne 0) {
                throw 'Normal scope exit point has an ambiguous origin or target'
            }
        }
        if ($stage -eq 'typed' -and @($ir.cleanupActions | Where-Object kind -eq 5).Count -lt 2) {
            throw 'Typed scope plans lost their scalar lifetime anchors'
        }
        if ($stage -eq 'lowered' -and @($ir.cleanupActions | Where-Object kind -eq 4).Count -eq 0) {
            throw 'Promoted scalar lifetime did not become explicit CELL_RELEASE'
        }
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c"
    if ($LASTEXITCODE -ne 0) { throw 'Scope plan Cgen failed' }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ($LASTEXITCODE -ne 0) { throw 'Repeated scope plan Cgen failed' }
    if (![System.Linq.Enumerable]::SequenceEqual(
        [byte[]][IO.File]::ReadAllBytes("$prefix.c"),
        [byte[]][IO.File]::ReadAllBytes("$prefix.repeat.c"))) {
        throw 'Scope plan Cgen is not byte deterministic'
    }
    $strict = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-O0',
        '-Wno-pointer-sign', '-Wno-unused-function', '-Wno-unused-label', '-Wno-unused-variable',
        '-Wno-unused-parameter', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers', '-I.', '-Ibuild/test')
    & gcc @strict test/cases/xlir_scope_exit_plan_host.c test/cases/xlir_scope_exit_plan_runtime.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope plan strict C compilation failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope plan runtime order or ownership balance failed' }
    & release/xl.exe run $source --backend xlir
    if ($LASTEXITCODE -ne 0) { throw 'Scope plan language JIT failed' }
    & release/xl.exe build $source --backend xlir -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope plan language AOT build failed' }
    & "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope plan language AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c', 'src/type/xl6_type_model.c',
        'src/ir/xl6_ir.c', 'src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c', 'src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c', 'src/common/xl6_output.c', 'src/common/xl6_console.c')
    & gcc @strict test/cases/xlir_scope_exit_plan_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope plan verifier contract compilation failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Scope plan verifier/Cgen mutation contract failed' }
} finally { Pop-Location }
Write-Output 'XLIR scope exit plans passed: canonical AFTER_SCOPE lifetime plans, promotion, reverse class cleanup, products/generics, strict C memory accounting, language JIT/AOT and verifier mutations.'
exit 0
