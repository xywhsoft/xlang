param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $taskRoot = (Get-Location).Path
    function SourceDigest {
        $sources = @(Get-ChildItem -LiteralPath src -Recurse -File | Where-Object Extension -in '.c','.h','.inc','.l','.y' | Sort-Object FullName)
        $rows = @($sources | ForEach-Object { $_.FullName.Substring($taskRoot.Length + 1).Replace('\','/') + '|' + (Get-FileHash -LiteralPath $_.FullName).Hash })
        return [BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes(($rows -join "`n")))).Replace('-','')
    }
    $sourceBefore = SourceDigest
    Write-Output "Generator close source: $sourceBefore"
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
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign',
        '-Wno-format-truncation','-I.','-Ibuild/test')
    $fixtures = @(
        @{Name='xlir_generator_close_routes';Contract='test/cases/xlir_generator_close_routes_contract.c'},
        @{Name='xlir_generator_close_extended';Contract='test/cases/xlir_generator_close_routes_contract.c';Source='test/cases/xlir_generator_close_extended.xl'},
        @{Name='xlir_generator_close_abrupt_finally';Contract='test/cases/xlir_generator_close_abrupt_finally_contract.c'},
        @{Name='xlir_generator_close_catch_dispatch';Contract='test/cases/xlir_generator_close_catch_dispatch_contract.c'},
        @{Name='xlir_generator_close_first_outcome';Contract='test/cases/xlir_generator_close_first_outcome_contract.c';Binding=$true}
    )
    $contracts = @{}
    foreach ($fixture in $fixtures) {
        $name = $fixture.Name
        $prefix = "build/test/$name"
        if (!$contracts.ContainsKey($fixture.Contract)) {
            & gcc @strict $fixture.Contract @compilerSources release/xl.dll -o "$prefix.contract.exe"
            if ($LASTEXITCODE -ne 0) { throw "Source contract build failed: $name" }
            $contracts[$fixture.Contract] = "$prefix.contract.exe"
        }
        foreach ($run in @($prefix,"$prefix.repeat")) {
            $arguments = @("$run.c","$run.h","$run.json")
            if ($fixture.Source) { $arguments += $fixture.Source }
            & $contracts[$fixture.Contract] @arguments
            if ($LASTEXITCODE -ne 0) { throw "Source contract failed: $name" }
        }
        $suffixes = @('c','h','json')
        if ($fixture.Binding) { $suffixes += 'h.bind.h' }
        foreach ($suffix in $suffixes) {
            if ([IO.File]::ReadAllText("$prefix.$suffix") -cne [IO.File]::ReadAllText("$prefix.repeat.$suffix")) {
                throw "Nondeterministic $suffix output: $name"
            }
        }
        $hostSource = "test/cases/${name}_host.c"
        $runtimeSource = "test/cases/${name}_runtime.c"
        & gcc @strict $runtimeSource $hostSource release/xl.dll -o "$prefix.strict.exe"
        if ($LASTEXITCODE -ne 0) { throw "Strict C build failed: $name" }
        & "$prefix.strict.exe"
        if ($LASTEXITCODE -ne 0) { throw "Source runtime/memory failed: $name" }
        & release/xl.exe run $runtimeSource $hostSource --inc-path . --inc-path build/test -- --smoke
        if ($LASTEXITCODE -ne 0) { throw "C-host JIT failed: $name" }
        & release/xl.exe build $runtimeSource $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
        if ($LASTEXITCODE -ne 0) { throw "C-host AOT build failed: $name" }
        & "$prefix.aot.exe" --smoke
        if ($LASTEXITCODE -ne 0) { throw "C-host AOT runtime failed: $name" }
    }
    if ((SourceDigest) -ne $sourceBefore) { throw 'Production source changed during generator close verification' }
} finally { Pop-Location }
Write-Output 'XLIR generator close routes passed: explicit close and cleanup-yield ABIs, pending and consumer outcomes, repeated logical close, nested finally, abrupt/internal loop exits, typed handler completion, verifier mutations, strict C, JIT/AOT and deterministic C/header/IR/bindings.'
exit 0
