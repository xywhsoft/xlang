param([string]$Root = '', [switch]$FullOutcome)
# FullOutcome is retained for callers of the former route-only gate. All runs
# now certify the full outcome matrix; there is no partial-success default.
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_pending_exception_outcome.xl'
    $prefix = 'build/test/xlir_pending_exception_outcome'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    foreach ($stage in @('typed', 'lowered')) {
        $arguments = @('ir', $source, '--format', 'json', '--emit', "$prefix.$stage.json")
        if ($stage -eq 'lowered') { $arguments += '--lowered' }
        & release/xl.exe @arguments
        if ($LASTEXITCODE -ne 0) { throw "Pending exception $stage IR failed" }
        $ir = [IO.File]::ReadAllText("$prefix.$stage.json") | ConvertFrom-Json
        if (@($ir.exitPlans | Where-Object pointName -eq 'after-try-exception').Count -ne 25) {
            throw 'Pending exception lost an explicit post-try continuation'
        }
        if ($stage -eq 'lowered' -and @($ir.instructions | Where-Object {
            $_.kindName -eq 'error-unwind-begin' -and $_.cleanupPlan -gt 0 -and
            $ir.exitPlans[$_.cleanupPlan - 1].pointName -eq 'after-try-exception'
        }).Count -lt 12) { throw 'Pending exception continuations were not materialized' }
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception Cgen/header failed' }
    $header = [IO.File]::ReadAllText("$prefix.h")
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne $header) { throw 'Pending exception C/header is not deterministic' }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $runtime = 'test/cases/xlir_pending_exception_outcome_runtime.c'
    $hostSource = 'test/cases/xlir_pending_exception_outcome_host.c'
    $probeArguments = @()
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception strict C build failed' }
    & "$prefix.strict.exe" @probeArguments
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception lost an owner, error or cleanup order' }
    & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke @probeArguments
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception C-host JIT failed' }
    & release/xl.exe build $runtime $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception AOT build failed' }
    & "$prefix.aot.exe" --smoke @probeArguments
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception AOT runtime failed' }
    $controlSource = 'test/cases/xlir_pending_exception_control.xl'
    $controlPrefix = 'build/test/xlir_pending_exception_control'
    & release/xl.exe ir $controlSource --lowered --format json --emit "$controlPrefix.lowered.json"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception control IR failed' }
    $controlIr = [IO.File]::ReadAllText("$controlPrefix.lowered.json") | ConvertFrom-Json
    if (@($controlIr.pendingExceptions).Count -ne 9 -or
        @($controlIr.asyncMachineFrameSlots | Where-Object destroyAbiName -eq 'free-error').Count -ne 1 -or
        @($controlIr.generatorFrameSlots | Where-Object destroyAbiName -eq 'free-error').Count -ne 1) {
        throw 'Pending exception control lost explicit owners or continuation error frames'
    }
    $asyncTargets = @($controlIr.callTargets | Where-Object { $_.kind -eq 4 -and $_.asyncMachine -ne 4294967295 })
    if ($asyncTargets.Count -ne 1) { throw 'Pending exception control needs one exact async worker binding' }
    [IO.File]::WriteAllText("${controlPrefix}_bind.h", "#define PENDING_CONTROL_ASYNC_START xlir_async_start_$($asyncTargets[0].symbol)`n", [Text.UTF8Encoding]::new($false))
    & release/xl.exe cgen $controlSource --backend xlir --emit "$controlPrefix.c" --emit-header "$controlPrefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception control C/header failed' }
    $controlHeader = [IO.File]::ReadAllText("$controlPrefix.h")
    & release/xl.exe cgen $controlSource --backend xlir --emit "$controlPrefix.repeat.c" --emit-header "$controlPrefix.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$controlPrefix.c") -cne [IO.File]::ReadAllText("$controlPrefix.repeat.c") -or
        [IO.File]::ReadAllText("$controlPrefix.h") -cne $controlHeader) { throw 'Pending exception control C/header is not deterministic' }
    $controlRuntime = 'test/cases/xlir_pending_exception_control_runtime.c'
    $controlHost = 'test/cases/xlir_pending_exception_control_host.c'
    & gcc @strict $controlRuntime $controlHost release/xl.dll -o "$controlPrefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception control strict C build failed' }
    & "$controlPrefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception control lost a pending outcome, frame owner or cleanup order' }
    & release/xl.exe run $controlRuntime $controlHost --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception control C-host JIT failed' }
    & release/xl.exe build $controlRuntime $controlHost src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$controlPrefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception control AOT build failed' }
    & "$controlPrefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception control AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_pending_exception_route_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception verifier build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception verifier mutation failed' }
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_pending_exception_owner_contract.c @compilerSources release/xl.dll -o "$controlPrefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception owner verifier build failed' }
    & "$controlPrefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Pending exception owner verifier mutation failed' }
} finally { Pop-Location }
Write-Output 'XLIR pending exception routing and ownership passed: managed/nested finally, exact lexical/item/cursor cleanup, nullable diagnostics and real OOM, return/break/continue override, internal catches, real await/yield suspension, allocation balance, typed/lowered verifier mutations, deterministic C/header and C-host JIT/AOT.'
exit 0
