param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_xcall_result_failure.xl'
    $hostSource = 'test/cases/xlir_xcall_result_failure_host.c'
    $runtimeSource = 'test/cases/xlir_xcall_result_failure_runtime.c'
    $prefix = 'build/test/xlir_xcall_result_failure'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    if (!(Test-Path -LiteralPath build/test/xl.dll) -or
        (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
        Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    }
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
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-Wno-pointer-sign','-Wno-format-truncation','-I.','-Ibuild/test')
    & gcc @strict test/cases/xlir_xcall_result_failure_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Callable result source contract build failed' }
    & "$prefix.contract.exe" "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'XCall result transaction lowering failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $adapters = @($ir.instructions | Where-Object runtimeOpName -eq 'xvalue-callable-from-closure')
    $products = @($ir.types | Where-Object { @($_.returns).Count -eq 7 })
    if ($adapters.Count -ne 5 -or $products.Count -ne 1 -or
        $products[0].callableResultType -eq 0 -or
        $products[0].id -notin @($adapters.calleeType)) {
        throw 'XCall result transaction lost its exact seven-slot IR signature'
    }
    foreach ($adapter in $adapters) {
        if (@($ir.instructions | Where-Object {
            $_.kindName -eq 'error-test' -and $_.operandCount -eq 1 -and
            $_.operands[0] -eq $adapter.result
        }).Count -ne 1) { throw 'Closure boxing lost its explicit error test' }
    }
    & "$prefix.contract.exe" "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Repeated XCall result transaction Cgen failed' }
    if (![System.Linq.Enumerable]::SequenceEqual(
        [byte[]][IO.File]::ReadAllBytes("$prefix.c"),
        [byte[]][IO.File]::ReadAllBytes("$prefix.repeat.c"))) {
        throw 'XCall result transaction Cgen is not byte deterministic'
    }
    $generated = [IO.File]::ReadAllText("$prefix.c")
    $bridge = @($ir.callableBridges | Where-Object signatureType -eq $products[0].id)
    if ($bridge.Count -ne 1 -or $bridge[0].resultCount -ne 7 -or
        $bridge[0].resultCommitAbi -ne 1 -or $bridge[0].frameCleanupAbi -ne 1) {
        throw 'XCall result transport lost its verified result transaction'
    }
    $slots = @($ir.callableBridgeResults | Where-Object {
        $_.id -ge $bridge[0].firstResult -and $_.id -lt ($bridge[0].firstResult + 7)
    })
    if ($slots.Count -ne 7) { throw 'XCall result transport requires all seven result slot programs' }
    for ($index = 0; $index -lt 7; ++$index) {
        if ($slots[$index].result -ne $index -or $slots[$index].type -ne $products[0].returns[$index]) {
            throw 'XCall result slot program disagrees with its declared product'
        }
    }
    if (!$generated.Contains('boxed = value.e0; value.e0 = NULL;') -or
        !$generated.Contains('xl6ErrorScopeBeginCleanup(&result_cleanup, true)') -or
        !$generated.Contains('xrtCallResultSet(&owned_results')) {
        throw 'XCall result transport must stage ownership and use the shared detached cleanup transaction'
    }
    $strictFlags = @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-function', '-Wno-unused-label', '-Wno-unused-variable',
        '-Wno-unused-parameter', '-Wno-unused-const-variable', '-Wno-missing-field-initializers',
        '-I.', '-Ibuild/test')
    & gcc @strictFlags $hostSource $runtimeSource release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'XCall result transaction strict C compilation failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'XCall result transaction strict C runtime failed' }
    & release/xl.exe run $runtimeSource $hostSource --inc-path . --inc-path build/test -- --smoke
    if ($LASTEXITCODE -ne 0) { throw 'XCall result transaction generated-C JIT failed' }
    # This is a C-only host entry with the generated module behind #include;
    # declare its shared error-runtime provider explicitly for static linkage.
    & release/xl.exe build $runtimeSource $hostSource src/api/xl_foreign_error.c --inc-path . --inc-path build/test -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'XCall result transaction generated-C AOT build failed' }
    & "$prefix.aot.exe" --smoke
    if ($LASTEXITCODE -ne 0) { throw 'XCall result transaction generated-C AOT runtime failed' }
} finally { Pop-Location }
Write-Output 'XLIR XCall result transaction passed: verified signature, deterministic C, strict C allocation accounting, JIT/AOT injected-failure smoke, native diagnostic consumption and cleanup reentry.'
exit 0
