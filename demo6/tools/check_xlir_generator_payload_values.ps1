param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
        'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
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
    $driver = 'build/test/xlir_generator_payload.contract.exe'
    & gcc @strict test/cases/xlir_generator_payload_contract.c @compilerSources release/xl.dll -o $driver
    if ($LASTEXITCODE -ne 0) { throw 'Generator payload contract build failed.' }
    $fixtures = @(
        @{Name='xlir_generator_item_object';Entry='generator_item_object';Expected=16},
        @{Name='xlir_generator_item_array';Entry='generator_item_array';Expected=18},
        @{Name='xlir_generator_item_callable';Entry='generator_item_callable';Expected=7},
        @{Name='xlir_generator_item_dynamic';Entry='generator_item_dynamic';Expected=16},
        @{Name='xlir_generator_item_future';Entry='generator_item_future';Expected=1},
        @{Name='xlir_generator_item_dynamic_capture';Entry='generator_item_dynamic_capture';Expected=7},
        @{Name='xlir_generator_payload_lifetime';Custom=$true},
        @{Name='xlir_generator_item_error';Entry='payload_probe';Expected=2},
        @{Name='xlir_generator_item_nested';Entry='payload_probe';Expected=7},
        @{Name='xlir_generator_item_nullable_int';Entry='payload_probe';Expected=7},
        @{Name='xlir_generator_item_nullable_object';Entry='payload_probe';Expected=7},
        @{Name='xlir_generator_launch_allocation';Allocation=$true},
        @{Name='xlir_generator_yield_allocation';Allocation=$true},
        @{Name='xlir_async_launch_allocation';Allocation=$true;Ready=$true},
        @{Name='xlir_async_pool_launch_allocation';Custom=$true}
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
        foreach ($suffix in @('c','h','json','h.launch.h')) {
            if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) {
                Write-Output "Nondeterministic generator payload $name/$suffix"
                ++$failures; $generationOk = $false
            }
        }
        if (!$generationOk) { continue }
        if ($fixture.Allocation) {
            $runtime = 'test/cases/xlir_generator_launch_allocation_runtime.c'
            $hostSource = "$prefix.allocation_host.c"
            $body = @(
                "#define XLIR_LAUNCH_ALLOCATION_SOURCE `"$prefix.c`"",
                "#define XLIR_LAUNCH_EXPECT_BODY $(if ($fixture.Ready) {0} else {1})",
                "#include `"$prefix.h.launch.h`"",
                '#include "test/cases/xlir_generator_launch_allocation_host.c"'
            ) -join "`n"
            [IO.File]::WriteAllText((Join-Path (Get-Location).Path $hostSource), $body, [Text.UTF8Encoding]::new($false))
        } elseif ($fixture.Custom) {
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
        & gcc @strict $runtime $hostSource release/xl.dll -o "$prefix.payload.strict.exe"
        if ($LASTEXITCODE -ne 0) { ++$failures; continue }
        & "$prefix.payload.strict.exe"
        if ($LASTEXITCODE -ne 0) { ++$failures }
    }
    if ($failures) { throw "Generator payload batch has $failures failed checks; all independent fixtures were attempted." }
} finally { Pop-Location }
Write-Output 'XLIR generator payload values passed: object, array, callable, dynamic and Future items, dynamic captures, exact transfer/destruction contracts, deterministic C/header/IR; normal completion, unstarted destruction, early close, released caller reference and destructor outcomes with strict runtime/memory checks.'
