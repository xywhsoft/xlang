param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    if (!(Test-Path -LiteralPath test/cases/xlir_continuation_owner_test.h -PathType Leaf)) {
        throw 'Continuation owner test helpers are missing'
    }
    $prefix = 'build/test/xlir_continuation_owner_views'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    if (!(Test-Path -LiteralPath build/test/xl.dll) -or
        (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
        Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    }
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
        'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
        'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
        'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
        'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
        'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-Wno-pointer-sign','-Wno-format-truncation','-I.','-Ibuild/test')
    & gcc @strict test/cases/xlir_continuation_owner_views_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Continuation owner views source contract build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Continuation owner views mutation/verification contract failed' }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Continuation owner views repeated verification failed' }
    foreach ($suffix in @('c','h','json','h.bind.h')) {
        if ([IO.File]::ReadAllText("$prefix.$suffix") -cne [IO.File]::ReadAllText("$prefix.repeat.$suffix")) {
            throw "Continuation owner views nondeterministic $suffix"
        }
    }
    & "$prefix.contract.exe" "$prefix.single.c" "$prefix.single.h" "$prefix.single.json" --single-view
    if ($LASTEXITCODE -ne 0) { throw 'Continuation owner views single generation failed' }
    foreach ($variant in @('aliased','single')) {
        $hostSource = if ($variant -eq 'single') { 'test/cases/xlir_continuation_owner_views_single_host.c' } else { 'test/cases/xlir_continuation_owner_views_host.c' }
        # The host translation unit embeds only current-source verified C.
        & gcc @strict test/cases/xlir_continuation_owner_views_runtime.c $hostSource release/xl.dll -o "$prefix.$variant.strict.exe"
        if ($LASTEXITCODE -ne 0) { throw "Continuation owner views $variant strict C build failed" }
        & "$prefix.$variant.strict.exe"
        if ($LASTEXITCODE -ne 0) { throw "Continuation owner views $variant runtime/memory failed" }
        & release/xl.exe run test/cases/xlir_continuation_owner_views_runtime.c $hostSource --inc-path . --inc-path build/test -- --smoke
        if ($LASTEXITCODE -ne 0) { throw "Continuation owner views $variant C-host JIT failed" }
        & release/xl.exe build test/cases/xlir_continuation_owner_views_runtime.c $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.$variant.aot.exe"
        if ($LASTEXITCODE -ne 0) { throw "Continuation owner views $variant C-host AOT build failed" }
        & "$prefix.$variant.aot.exe" --smoke
        if ($LASTEXITCODE -ne 0) { throw "Continuation owner views $variant C-host AOT runtime failed" }
    }
} finally { Pop-Location }
Write-Output 'XLIR continuation owner views passed: one physical owner for transparent SSA views, exact retirement, single/aliased strict C runtime and memory, C-host JIT/AOT, deterministic C/header/JSON.'
exit 0
