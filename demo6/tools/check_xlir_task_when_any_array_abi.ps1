param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_task_when_any_array_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_task_when_any_array_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_task_when_any_array_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_task_when_any_array_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_task_when_any_array_abi.second.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR task.whenAny array ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.whenAny array typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$intrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "task-when-any-array"
})
if ( $intrinsics.Count -ne 1 ) {
    throw "XLIR task.whenAny array IntrinsicCall count is incomplete: $($intrinsics.Count)"
}
$intrinsic = $intrinsics[0]
$operands = @($typed.intrinsicOperands |
    Select-Object -Skip $intrinsic.firstOperand -First $intrinsic.operandCount)
$resultFuture = Get-TypeById $typed $intrinsic.resultType
$payload = Get-TypeById $typed $intrinsic.payloadType
$arrayType = if ( $operands.Count -eq 1 ) {
    Get-TypeById $typed $operands[0].type
} else { $null }
$elementFuture = if ( $null -ne $arrayType -and
                       @($arrayType.params).Count -eq 1 ) {
    Get-TypeById $typed $arrayType.params[0]
} else { $null }
if ( $intrinsic.admissionName -ne "verified" -or
     $intrinsic.resultAbiName -ne "owned" -or
     $intrinsic.errorAbiName -ne "may-raise" -or
     $intrinsic.payloadAbiName -ne "copy-i64" -or
     $intrinsic.operandCount -ne 1 -or $operands.Count -ne 1 -or
     $operands[0].abiName -ne "borrow" -or
     $null -eq $resultFuture -or $resultFuture.name -ne "future" -or
     @($resultFuture.params).Count -ne 1 -or
     $resultFuture.params[0] -ne $intrinsic.payloadType -or
     $null -eq $payload -or $payload.name -ne "int" -or
     $payload.integerTargetName -ne "i64" -or
     $null -eq $arrayType -or $arrayType.collectionAbiName -ne "array" -or
     @($arrayType.params).Count -ne 1 -or
     $null -eq $elementFuture -or $elementFuture.name -ne "future" -or
     @($elementFuture.params).Count -ne 1 ) {
    throw "XLIR task.whenAny array intrinsic has no exact borrowed array<Future<T>> and owned Future<I64>/COPY_I64 ABI"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.whenAny array lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$boxes = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-future-ref"
})
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "future-when-any-index-array"
})
if ( $boxes.Count -ne 2 ) {
    throw "XLIR Future-to-xvalue retained-reference box count is incomplete: $($boxes.Count)"
}
if ( $operations.Count -ne 1 ) {
    throw "XLIR task.whenAny array RuntimeOp count is incomplete: $($operations.Count)"
}
$operation = $operations[0]
$loweredResult = Get-TypeById $lowered $operation.type
$loweredPayload = Get-TypeById $lowered $operation.payloadType
$sourceDefinition = @($lowered.instructions | Where-Object {
    $_.result -eq $operation.operands[0]
})[0]
$loweredArray = if ( $null -ne $sourceDefinition ) {
    Get-TypeById $lowered $sourceDefinition.type
} else { $null }
if ( $operation.operandCount -ne 1 -or
     $operation.asyncResultAbiName -ne "copy-i64" -or
     $null -eq $loweredResult -or $loweredResult.name -ne "future" -or
     @($loweredResult.params).Count -ne 1 -or
     $loweredResult.params[0] -ne $operation.payloadType -or
     $null -eq $loweredPayload -or $loweredPayload.name -ne "int" -or
     $loweredPayload.integerTargetName -ne "i64" -or
     $null -eq $loweredArray -or
     $loweredArray.collectionAbiName -ne "array" ) {
    throw "XLIR task.whenAny array RuntimeOp has no exact array operand and Future<I64>/COPY_I64 ABI"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.whenAny array C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.whenAny array repeat C generation failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR task.whenAny array generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtValueFuture(v",
    "#include <__xl_runtime/xl6_future_aggregate.h>",
    " = (xl6FutureAggregateRuntimeV1Get()->WhenAnyIndexArray)(v"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR task.whenAny array generated C is missing direct mechanical XRT emission: $required"
    }
}
if ( ([regex]::Matches($c,
        '#include <__xl_runtime/xl6_future_aggregate\.h>')).Count -ne 1 ) {
    throw "XLIR task.whenAny array helper emission is not use-directed and unique"
}
foreach ( $forbidden in @(
    "xl6_task_when_any", "xl6_task_when_any_from_array",
    "xl6_future_to_xvalue", "xl6FutureAny(", "xl6FuturePickIndex(",
    "xlir_future_when_any_index_array", "xrtValueGetFuture(item)", "xrtValueArrayGet(array, i)"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR task.whenAny array Cgen reused legacy/source-shape generation: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_task_when_any_array_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_task_when_any_array_abi.exe && build\tmp\xlir_task_when_any_array_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR task.whenAny array strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR task.whenAny typed-array ABI gate passed: verified retained Future xvalue boxes, exact borrowed array<Future<T>>, owned Future<I64>/COPY_I64 projection, deterministic direct-XRT C, and strict runtime."
