param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_exit_route.xl'
    $prefix = 'build/test/xlir_exit_route'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    foreach ($stage in @('typed', 'lowered')) {
        $arguments = @('ir', $source, '--format', 'json', '--emit', "$prefix.$stage.json")
        if ($stage -eq 'lowered') { $arguments += '--lowered' }
        & release/xl.exe @arguments
        if ($LASTEXITCODE -ne 0) { throw "Exit route $stage IR failed" }
        $ir = [IO.File]::ReadAllText("$prefix.$stage.json") | ConvertFrom-Json
        $routes = @($ir.exitPlans | Where-Object stepCount -gt 0)
        if ($routes.Count -lt 10 -or $ir.exitSteps.Count -lt 40) { throw 'Exit route topology was omitted' }
        foreach ($route in $routes) {
            $terminal = $ir.exitSteps[$route.firstStep + $route.stepCount - 1]
            if ($terminal.kindName -ne 'terminal' -or $terminal.failureTarget -ne 0) {
                throw 'Exit route has no unique terminal step'
            }
        }
    }
    & release/xl.exe ir $source --lowered --format text --emit "$prefix.lowered.txt"
    if ($LASTEXITCODE -ne 0 -or !([IO.File]::ReadAllText("$prefix.lowered.txt").Contains('exit-step'))) {
        throw 'Text IR dump omitted exit steps'
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Exit route Cgen/header failed' }
    $header = [IO.File]::ReadAllText("$prefix.h")
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne $header) { throw 'Exit route C/header output is not deterministic' }
    $strict = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-O0', '-Wno-unused-function',
        '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers', '-Wno-pointer-sign', '-Wno-format-truncation', '-I.')
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    & gcc @strict test/cases/xlir_exit_route_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Exit route verifier unit build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Exit route topology or verifier mutation failed' }
} finally { Pop-Location }
Write-Output 'XLIR exit-route topology passed: ordered scope/catch/finally/loop/parameter boundaries, exact failure targets and typed/lowered verifier rejection before C/header output. This gate does not certify CFG cleanup outcomes.'
exit 0
