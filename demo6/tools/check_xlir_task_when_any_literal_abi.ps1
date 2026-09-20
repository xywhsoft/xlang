param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_task_when_any_literal_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_task_when_any_literal_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_task_when_any_literal_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_task_when_any_literal_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_task_when_any_literal_abi.second.c"
$legacyC = Join-Path $outputDir "xlir_task_when_any_literal_abi.legacy.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR task.whenAny literal ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.whenAny typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$intrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "task-when-any"
})
if ( $intrinsics.Count -ne 2 ) {
    throw "XLIR task.whenAny IntrinsicCall count is incomplete: $($intrinsics.Count)"
}
$typedPayloadShapes = [System.Collections.Generic.List[string]]::new()
foreach ( $intrinsic in $intrinsics ) {
    $resultFuture = Get-TypeById $typed $intrinsic.resultType
    $payload = Get-TypeById $typed $intrinsic.payloadType
    $operands = @($typed.intrinsicOperands |
        Select-Object -Skip $intrinsic.firstOperand -First $intrinsic.operandCount)
    if ( $intrinsic.admissionName -ne "verified" -or
         $intrinsic.resultAbiName -ne "owned" -or
         $intrinsic.errorAbiName -ne "may-raise" -or
         $intrinsic.payloadAbiName -ne "copy-i64" -or
         $intrinsic.operandCount -ne 2 -or $operands.Count -ne 2 -or
         $null -eq $resultFuture -or $resultFuture.name -ne "future" -or
         @($resultFuture.params).Count -ne 1 -or
         $resultFuture.params[0] -ne $intrinsic.payloadType -or
         $null -eq $payload -or $payload.name -ne "int" -or
         $payload.integerTargetName -ne "i64" ) {
        throw "XLIR task.whenAny intrinsic has no exact owned Future<I64> result ABI"
    }
    $operandPayloads = [System.Collections.Generic.List[string]]::new()
    foreach ( $operand in $operands ) {
        $future = Get-TypeById $typed $operand.type
        if ( $operand.abiName -ne "borrow" -or $null -eq $future -or
             $future.name -ne "future" -or @($future.params).Count -ne 1 ) {
            throw "XLIR task.whenAny intrinsic operand is not a borrowed Future<T>"
        }
        $operandPayloads.Add([string]$future.params[0])
    }
    $typedPayloadShapes.Add(($operandPayloads -join ","))
}
if ( $typedPayloadShapes[0].Split(',')[0] -ne
         $typedPayloadShapes[0].Split(',')[1] -or
     $typedPayloadShapes[1].Split(',')[0] -eq
         $typedPayloadShapes[1].Split(',')[1] ) {
    throw "XLIR task.whenAny gate did not preserve homogeneous and heterogeneous Future payload identities"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.whenAny lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "future-when-any-index"
})
if ( $operations.Count -ne 2 ) {
    throw "XLIR task.whenAny RuntimeOp count is incomplete: $($operations.Count)"
}
$loweredPayloadShapes = [System.Collections.Generic.List[string]]::new()
foreach ( $operation in $operations ) {
    $resultFuture = Get-TypeById $lowered $operation.type
    $payload = Get-TypeById $lowered $operation.payloadType
    if ( $operation.kindName -ne "runtime-call" -or
         $operation.asyncResultAbiName -ne "copy-i64" -or
         $operation.operandCount -ne 2 -or
         @($operation.operands).Count -ne 2 -or
         $null -eq $resultFuture -or $resultFuture.name -ne "future" -or
         @($resultFuture.params).Count -ne 1 -or
         $resultFuture.params[0] -ne $operation.payloadType -or
         $null -eq $payload -or $payload.name -ne "int" -or
         $payload.integerTargetName -ne "i64" ) {
        throw "XLIR task.whenAny RuntimeOp has no exact Future<I64>/COPY_I64 ABI"
    }
    $operandPayloads = [System.Collections.Generic.List[string]]::new()
    foreach ( $value in @($operation.operands) ) {
        $definition = @($lowered.instructions | Where-Object {
            $_.result -eq $value
        })[-1]
        $future = if ( $null -ne $definition ) {
            Get-TypeById $lowered $definition.type
        } else { $null }
        if ( $null -eq $future -or $future.name -ne "future" -or
             @($future.params).Count -ne 1 ) {
            throw "XLIR task.whenAny RuntimeOp operand lost its exact Future<T> ABI"
        }
        $operandPayloads.Add([string]$future.params[0])
    }
    $loweredPayloadShapes.Add(($operandPayloads -join ","))
}
if ( $loweredPayloadShapes[0].Split(',')[0] -ne
         $loweredPayloadShapes[0].Split(',')[1] -or
     $loweredPayloadShapes[1].Split(',')[0] -eq
         $loweredPayloadShapes[1].Split(',')[1] ) {
    throw "XLIR task.whenAny lowering conflated heterogeneous Future payload identities"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.whenAny C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.whenAny repeat C generation failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR task.whenAny generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "#include <__xl_runtime/xl6_future_aggregate.h>",
    "xfuture* fa", " = (xl6FutureAggregateRuntimeV1Get()->WhenAnyIndex)(fa"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR task.whenAny generated C is missing direct mechanical XRT emission: $required"
    }
}
if ( ([regex]::Matches($c,
        '#include <__xl_runtime/xl6_future_aggregate\.h>')).Count -ne 1 ) {
    throw "XLIR task.whenAny helper emission is not use-directed and unique"
}
foreach ( $forbidden in @(
    "xl6_task_when_any", "xl6FutureAny(", "xl6FuturePickIndex(", "xl6FutureWhenAnyIndex(",
    "xl6FutureThenOwned(", "xlir_future_when_any_index_continue",
    "xrtFutureThenOwnedCancelSource(", "xlir_future_when_any_index_free"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR task.whenAny Cgen reused legacy/source-shape generation: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_task_when_any_literal_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_task_when_any_literal_abi.exe && build\tmp\xlir_task_when_any_literal_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR task.whenAny strict-C/runtime gate failed"
    }

    & $xl cgen $source --backend legacy --emit $legacyC
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR task.whenAny legacy C generation failed"
    }
    $legacyCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror build\tmp\xlir_task_when_any_literal_abi.legacy.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_task_when_any_literal_abi.legacy.exe && build\test\xlir_task_when_any_literal_abi.legacy.exe'
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR task.whenAny legacy differential runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR task.whenAny literal ABI gate passed: verified heterogeneous borrowed Future operands, exact Future<I64>/COPY_I64 projection, deterministic direct-XRT continuation C, strict runtime, and legacy differential semantics."
