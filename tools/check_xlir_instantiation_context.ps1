param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
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
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    $driver = 'build/test/xlir_instantiation_context.contract.exe'
    & gcc @strict test/cases/xlir_instantiation_context_contract.c @compilerSources release/xl.dll -o $driver
    if ($LASTEXITCODE -ne 0) { throw 'Instantiation contract build failed.' }
    $fixtures = @(
        @{Name='xlir_generator_item_object';Entry='generator_item_object';Expected=16},
        @{Name='xlir_generator_initializer_scalar';Entry='generator_initializer_scalar';Expected=8},
        @{Name='xlir_generator_initializer_nested';Entry='generator_initializer_nested';Expected=8},
        @{Name='xlir_generator_close_item_outcome';Custom=$true}
    )
    $failures = 0
    foreach ($fixture in $fixtures) {
        $name = $fixture.Name
        $prefix = "build/test/$name"
        $generationOk = $true
        foreach ($output in @($prefix,"$prefix.repeat")) {
            & $driver "test/cases/$name.xl" "$output.c" "$output.h" "$output.json"
            if ($LASTEXITCODE -ne 0) { $generationOk = $false; ++$failures }
        }
        if (!$generationOk) { continue }
        foreach ($suffix in @('c','h','json')) {
            if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) {
                Write-Output "Nondeterministic instantiation $name/$suffix"
                ++$failures; $generationOk = $false
            }
        }
        if (!$generationOk) { continue }
        if ($fixture.Custom) {
            $runtime = "test/cases/${name}_runtime.c"
            $hostSource = "test/cases/${name}_host.c"
        } else {
            $runtime = 'test/cases/xlir_generator_item_basic_runtime.c'
            $hostSource = "$prefix.basic_host.c"
            $body = @(
                "#define XLIR_GENERATOR_ITEM_SOURCE `"$prefix.c`"",
                "#define XLIR_GENERATOR_ITEM_ENTRY $($fixture.Entry)",
                "#define XLIR_GENERATOR_ITEM_EXPECTED $($fixture.Expected)",
                '#include "test/cases/xlir_generator_item_basic_host.c"'
            ) -join "`n"
            [IO.File]::WriteAllText((Join-Path (Get-Location).Path $hostSource), $body, [Text.UTF8Encoding]::new($false))
        }
        & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.instantiation.strict.exe"
        if ($LASTEXITCODE -ne 0) { ++$failures; continue }
        & "$prefix.instantiation.strict.exe"
        if ($LASTEXITCODE -ne 0) { ++$failures }
    }
    if ($failures) { throw "Instantiation batch has $failures failed checks; all independent fixtures were attempted." }
} finally { Pop-Location }
Write-Output 'XLIR instantiation context passed: declared fields and native wrappers, nested execution provenance, exact intervals/receivers/commits, coherent metadata deletion rejection, deterministic C/header/IR and strict source runtime/memory.'
