param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = Join-Path $Root 'test/cases/xlir_return_commit.xl'
    # System.IO resolves relative paths from the native process directory, not
    # PowerShell's provider location.  Keep every direct file operation rooted.
    $prefix = Join-Path $Root 'build/test/xlir_return_commit'
    New-Item -ItemType Directory -Force -Path build/test | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Return commit lowered IR failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    $commits = 0
    $cleanupAfterPrepare = 0
    foreach ($function in $ir.functions) {
        $type = @($ir.types | Where-Object id -eq $function.returnType)
        if ($type.Count -ne 1 -or $type[0].ownershipAbi -ne 2) { continue }
        $instructions = @($ir.instructions | Where-Object {
            $_.index -ge $function.firstInstruction -and
            $_.index -lt $function.firstInstruction + $function.instructionCount
        })
        foreach ($ret in @($instructions | Where-Object kindName -eq 'return')) {
            $load = $ir.instructions[$ret.index - 2]
            $move = $ir.instructions[$ret.index - 1]
            $storage = @($ir.storages | Where-Object symbol -eq $load.targetSymbol)
            if ($load.kindName -ne 'load' -or $move.kindName -ne 'move' -or
                $load.result -ne $ret.operands[0] -or $move.operands[0] -ne $load.result -or
                $move.targetSymbol -ne $load.targetSymbol -or $storage.Count -ne 1 -or
                $storage[0].kindName -ne 'temporary' -or $storage[0].owner -ne $function.declaration -or
                $storage[0].type -ne $function.returnType) {
                throw 'Managed RETURN must end in an adjacent exact pending-owner LOAD/MOVE commit'
            }
            $prepares = @($instructions | Where-Object {
                $_.kindName -in @('store', 'product-store-move') -and
                $_.targetSymbol -eq $load.targetSymbol -and $_.index -lt $load.index
            })
            if ($prepares.Count -eq 0) { throw 'Managed return has no pending storage preparation' }
            $lastPrepare = $prepares[-1]
            if (@($instructions | Where-Object {
                $_.kindName -in @('release', 'cell-release') -and
                $_.index -gt $lastPrepare.index -and $_.index -lt $load.index
            }).Count -gt 0) { ++$cleanupAfterPrepare }
            ++$commits
        }
    }
    if ($commits -lt 5 -or $cleanupAfterPrepare -lt 5) {
        throw 'Return commit coverage lost single/product/finally paths or cleanup before commit'
    }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.c"
    if ($LASTEXITCODE -ne 0) { throw 'Return commit Cgen failed' }
    & release/xl.exe cgen $source --backend xlir --emit "$prefix.repeat.c"
    if ($LASTEXITCODE -ne 0) { throw 'Repeated return commit Cgen failed' }
    if (![System.Linq.Enumerable]::SequenceEqual(
        [byte[]][IO.File]::ReadAllBytes("$prefix.c"),
        [byte[]][IO.File]::ReadAllBytes("$prefix.repeat.c"))) {
        throw 'Return commit Cgen is not byte deterministic'
    }
    $strict = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-O0',
        '-Wno-pointer-sign', '-Wno-unused-function', '-Wno-unused-label', '-Wno-unused-variable',
        '-Wno-unused-parameter', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers', '-I.', '-Ibuild/test')
    & gcc @strict test/cases/xlir_return_commit_host.c test/cases/xlir_return_commit_runtime.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return commit strict C compilation failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return commit runtime ownership balance failed' }
    & release/xl.exe run $source --backend xlir
    if ($LASTEXITCODE -ne 0) { throw 'Return commit language JIT failed' }
    & release/xl.exe build $source --backend xlir -o "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return commit language AOT build failed' }
    & "$prefix.aot.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return commit language AOT runtime failed' }
    $compilerSources = @('src/ast/xl6_ast.c', 'src/type/xl6_type_model.c',
        'src/ir/xl6_ir.c', 'src/ir/xl6_ir_lower.c',
        'src/cgen/xl6_ir_cgen.c', 'src/cgen/xl6_cgen_shared_runtime.c',
        'src/diag/xl6_diag.c', 'src/common/xl6_output.c', 'src/common/xl6_console.c')
    & gcc @strict test/cases/xlir_return_commit_contract.c @compilerSources release/xl.dll -o "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return commit verifier contract compilation failed' }
    & "$prefix.contract.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Return commit verifier/Cgen mutation contract failed' }
} finally { Pop-Location }
Write-Output "XLIR return commit passed: $commits exact pending-owner commits, $cleanupAfterPrepare cleanup-before-commit paths, strict C memory accounting, JIT/AOT runtime, deterministic C and 14 verifier/Cgen mutations."
exit 0
