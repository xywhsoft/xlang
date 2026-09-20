param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
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
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign',
        '-Wno-format-truncation','-I.','-Ibuild/test')
    & gcc @strict test/cases/xlir_owner_consumption_contract.c @compilerSources release/xl.dll -o build/test/xlir_owner_consumption_contract.exe
    if ($LASTEXITCODE -ne 0) { throw 'Owner consumption contract build failed' }
    & build/test/xlir_owner_consumption_contract.exe
    if ($LASTEXITCODE -ne 0) { throw 'Owner consumption CFG/invalid-IR contract failed' }
    foreach ($source in @('test/cases/xlir_managed_storage_inputs.xl', 'test/cases/xlir_parameter_cleanup_outcome.xl',
        'test/cases/xlir_loop_cleanup_outcome.xl', 'test/cases/xlir_pending_exception_outcome.xl',
        'test/cases/cgen_error_model.xl', 'release/examples/error_model.xl', 'test/cases/xlir_checked_projection.xl')) {
        & build/test/xlir_owner_consumption_contract.exe $source build/test/xlir_owner_consumption.c build/test/xlir_owner_consumption.h build/test/xlir_owner_consumption.json
        if ($LASTEXITCODE -ne 0) { throw "Owner consumption lowering failed: $source" }
        $ir = Get-Content -Raw -LiteralPath build/test/xlir_owner_consumption.json | ConvertFrom-Json
        $storageTypes = @{}
        foreach ($storage in $ir.storages) { $storageTypes[[string]$storage.symbol] = $storage.type }
        foreach ($instruction in @($ir.instructions | Where-Object kindName -eq 'store')) {
            if ($instruction.type -eq 0 -or $instruction.type -ne $storageTypes[[string]$instruction.targetSymbol]) {
                throw "STORE must carry its physical storage type: $source"
            }
        }
        & build/test/xlir_owner_consumption_contract.exe $source build/test/xlir_owner_consumption.c build/test/xlir_owner_consumption.h build/test/xlir_owner_consumption.json
        if ($LASTEXITCODE -ne 0) { throw "Owner consumption C/header failed: $source" }
        $firstC = Get-Content -Raw build/test/xlir_owner_consumption.c
        $firstHeader = Get-Content -Raw build/test/xlir_owner_consumption.h
        & build/test/xlir_owner_consumption_contract.exe $source build/test/xlir_owner_consumption.c build/test/xlir_owner_consumption.h build/test/xlir_owner_consumption.json
        if ($LASTEXITCODE -ne 0 -or $firstC -cne (Get-Content -Raw build/test/xlir_owner_consumption.c) -or
            $firstHeader -cne (Get-Content -Raw build/test/xlir_owner_consumption.h)) {
            throw "Owner consumption C/header determinism failed: $source"
        }
    }
} finally { Pop-Location }
Write-Output 'XLIR owner consumption passed: explicit operand effects, path-sensitive SSA and per-product-field completion, exact STORE payloads and invalid-IR rejection before C/header output. Storage borrow liveness remains a separate contract.'
exit 0
