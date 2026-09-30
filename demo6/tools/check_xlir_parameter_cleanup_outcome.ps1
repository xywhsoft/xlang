param([string]$Root = '', [string]$SourceCompiler = 'release/xl.exe')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_parameter_cleanup_outcome.xl'
    $prefix = 'build/test/xlir_parameter_cleanup_outcome'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    if (!(Test-Path -LiteralPath build/test/xl.dll) -or
        (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
        Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    }
    foreach ($stage in @('typed', 'lowered')) {
        $arguments = @('ir', $source, '--format', 'json', '--emit', "$prefix.$stage.json")
        if ($stage -eq 'lowered') { $arguments += '--lowered' }
        & $SourceCompiler @arguments
        if ($LASTEXITCODE -ne 0) { throw "Parameter cleanup $stage IR failed" }
        $ir = [IO.File]::ReadAllText("$prefix.$stage.json") | ConvertFrom-Json
        if ($stage -eq 'lowered') {
            $lifetimes = @($ir.callTargets | ForEach-Object parameterLifetimes)
            if (@($lifetimes | Where-Object rebindable -eq 1).Count -lt 20 -or
                @($lifetimes | Where-Object { $_.rebindable -eq 0 -and $_.sourceSymbol -gt 0 }).Count -lt 1) {
                throw 'Parameter binding mutation facts lost stable/rebound or out/byref coverage'
            }
            if (@($ir.instructions | Where-Object parameterScope -gt 0).Count -lt 24) {
                throw 'Parameter transactions lost their explicit terminal scope identities'
            }
            $cells = @($ir.functions | ForEach-Object parameterCells | Where-Object { $_ -gt 0 })
            if ($cells.Count -ne 2) { throw 'Parameter promoted-cell map is incomplete' }
        }
    }
    & $SourceCompiler cgen $source --backend xlir --emit "$prefix.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Parameter cleanup Cgen/header failed' }
    $header = [IO.File]::ReadAllText("$prefix.h")
    & $SourceCompiler cgen $source --backend xlir --emit "$prefix.repeat.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne $header) { throw 'Parameter cleanup C/header is not deterministic' }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $runtime = 'test/cases/xlir_parameter_cleanup_outcome_runtime.c'
    $hostSource = 'test/cases/xlir_parameter_cleanup_outcome_host.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Parameter cleanup strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Parameter cleanup lost an outcome, pending owner or visible binding' }
    & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Parameter cleanup C-host JIT failed' }
    $artifactRuntimeSources = @(
        'src/runtime/xl6_slot.c', 'src/runtime/xl6_code_lease.c',
        'src/runtime/xl6_cell.c', 'src/runtime/xl6_closure.c',
        'src/runtime/xl6_code_finalizer.c')
    & release/xl.exe build $runtime $hostSource @artifactRuntimeSources src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Parameter cleanup AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Parameter cleanup AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_parameter_cleanup_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Parameter cleanup verifier build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Parameter cleanup verifier mutation failed' }
} finally { Pop-Location }
Write-Output 'XLIR parameter cleanup passed: exact parameter/cell owner map and stable/rebound effects through multi-result assignments, isolated terminal outcomes, first-error preservation with NULL/OOM, scalar/void/product/owning result commit or rollback, no reentry into a completed catch/finally, strict allocation balance, verifier rejection and C-host JIT/AOT. Loop-resource cleanup remains separate work.'
exit 0
