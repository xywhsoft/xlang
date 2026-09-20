param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_managed_storage_inputs.xl'
    $prefix = 'build/test/xlir_managed_storage_inputs'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Managed storage input lowered IR failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    foreach ($kind in @('cell-new','cell-init','cell-exchange','virtual-call','move')) {
        if (@($ir.instructions | Where-Object kindName -eq $kind).Count -eq 0) { throw "Storage fixture lost $kind" }
    }
    $managedTypes = @($ir.types | Where-Object ownershipAbi -eq 2 | ForEach-Object id)
    if (@($ir.instructions | Where-Object { $_.kindName -eq 'value' -and $_.targetValue -ne 0 -and $_.type -in $managedTypes }).Count -ne 0) {
        throw 'Managed match result must use owned storage instead of cross-branch VALUE reassignment'
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0) { throw 'Managed storage C/header failed' }
    $header = [IO.File]::ReadAllText("$prefix.h")
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c" --emit-header "$prefix.h"
    if ($LASTEXITCODE -ne 0 -or [IO.File]::ReadAllText("$prefix.c") -cne [IO.File]::ReadAllText("$prefix.repeat.c") -or
        [IO.File]::ReadAllText("$prefix.h") -cne $header) { throw 'Managed storage C/header is not deterministic' }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    $runtime = 'test/cases/xlir_managed_storage_inputs_runtime.c'
    $hostSource = 'test/cases/xlir_managed_storage_inputs_host.c'
    & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Managed storage strict C build failed' }
    & "$prefix.strict.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Managed storage value/lifetime/allocation balance failed' }
    & release/xl.exe run $runtime $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Managed storage C-host JIT failed' }
    & release/xl.exe build $runtime $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Managed storage AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'Managed storage AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    & gcc @strict '-Wno-pointer-sign' '-Wno-format-truncation' test/cases/xlir_managed_storage_inputs_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Managed storage verifier contract build failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Managed storage verifier contract failed' }
    foreach ($original in @('cgen_dynamic_member_access','cgen_derive_persistence','cgen_threaded_context_matrix',
        'hir_flow_value_ownership_matrix','nullable_string_lifecycle','cgen_base_method_static_dispatch','cgen_async_generator')) {
        & release/xl.exe cgen "test/cases/$original.xl" --backend xlir --emit "$prefix.original.c"
        if ($LASTEXITCODE -ne 0) { throw "Original managed storage representative regressed: $original" }
    }
} finally { Pop-Location }
Write-Output 'XLIR managed storage inputs passed: independent acquired owners, explicit match storage merges, dominating MOVE, scalar cell payloads, deterministic C/header, strict allocation balance and C-host JIT/AOT.'
exit 0
