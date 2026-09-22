param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_task_aggregate_array_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_task_aggregate_array_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_task_aggregate_array_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_task_aggregate_array_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_task_aggregate_array_abi.second.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR task aggregate array ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

function Assert-TypedArrayAggregate($document, $intrinsic, $expectedKind) {
    $operands = @($document.intrinsicOperands |
        Select-Object -Skip $intrinsic.firstOperand -First $intrinsic.operandCount)
    $resultFuture = Get-TypeById $document $intrinsic.resultType
    $payload = Get-TypeById $document $intrinsic.payloadType
    $arrayType = if ( $operands.Count -eq 1 ) {
        Get-TypeById $document $operands[0].type
    } else { $null }
    $elementFuture = if ( $null -ne $arrayType -and
                           @($arrayType.params).Count -eq 1 ) {
        Get-TypeById $document $arrayType.params[0]
    } else { $null }
    $publicPayload = if ( $null -ne $resultFuture -and
                           @($resultFuture.params).Count -eq 1 ) {
        Get-TypeById $document $resultFuture.params[0]
    } else { $null }
    $resultMatches = if ( $expectedKind -eq "task-all-array" ) {
        $null -ne $publicPayload -and
        $publicPayload.collectionAbiName -eq "array" -and
        @($publicPayload.params).Count -eq 1 -and
        $publicPayload.params[0] -eq $intrinsic.payloadType
    } else {
        $null -ne $publicPayload -and $publicPayload.id -eq $intrinsic.payloadType
    }
    if ( $intrinsic.kindName -ne $expectedKind -or
         $intrinsic.admissionName -ne "verified" -or
         $intrinsic.resultAbiName -ne "owned" -or
         $intrinsic.errorAbiName -ne "may-raise" -or
         $intrinsic.payloadAbiName -ne "copy-i64" -or
         $intrinsic.operandCount -ne 1 -or $operands.Count -ne 1 -or
         $operands[0].abiName -ne "borrow" -or
         $null -eq $resultFuture -or $resultFuture.name -ne "future" -or
         @($resultFuture.params).Count -ne 1 -or
         $null -eq $payload -or $payload.name -ne "int" -or
         $payload.integerTargetName -ne "i64" -or
         $null -eq $arrayType -or $arrayType.collectionAbiName -ne "array" -or
         @($arrayType.params).Count -ne 1 -or
         $null -eq $elementFuture -or $elementFuture.name -ne "future" -or
         @($elementFuture.params).Count -ne 1 -or
         $elementFuture.params[0] -ne $intrinsic.payloadType -or
         ! $resultMatches ) {
        throw "XLIR $expectedKind has no exact borrowed array<Future<T>>, payload ABI, and owned result Future ABI"
    }
}

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task aggregate array typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$typedAll = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "task-all-array"
})
$typedRace = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "task-race-array"
})
if ( $typedAll.Count -ne 1 -or $typedRace.Count -ne 1 ) {
    throw "XLIR task aggregate array IntrinsicCalls are incomplete: all=$($typedAll.Count), race=$($typedRace.Count)"
}
Assert-TypedArrayAggregate $typed $typedAll[0] "task-all-array"
Assert-TypedArrayAggregate $typed $typedRace[0] "task-race-array"

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task aggregate array lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$boxes = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-future-ref"
})
$allOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "future-all-xvalues-array"
})
$raceOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "future-race-array"
})
if ( $boxes.Count -ne 4 ) {
    throw "XLIR Future-to-xvalue retained-reference box count is incomplete: $($boxes.Count)"
}
if ( $allOperations.Count -ne 1 -or $raceOperations.Count -ne 1 ) {
    throw "XLIR task aggregate array RuntimeOps are incomplete: all=$($allOperations.Count), race=$($raceOperations.Count)"
}
foreach ( $operation in @($allOperations[0], $raceOperations[0]) ) {
    $resultFuture = Get-TypeById $lowered $operation.type
    $payload = Get-TypeById $lowered $operation.payloadType
    $sourceDefinition = @($lowered.instructions | Where-Object {
        $_.result -eq $operation.operands[0]
    })[0]
    $arrayType = if ( $null -ne $sourceDefinition ) {
        Get-TypeById $lowered $sourceDefinition.type
    } else { $null }
    if ( $operation.operandCount -ne 1 -or
         $operation.asyncResultAbiName -ne "copy-i64" -or
         $null -eq $resultFuture -or $resultFuture.name -ne "future" -or
         @($resultFuture.params).Count -ne 1 -or
         $null -eq $payload -or $payload.name -ne "int" -or
         $payload.integerTargetName -ne "i64" -or
         $null -eq $arrayType -or $arrayType.collectionAbiName -ne "array" ) {
        throw "XLIR $($operation.runtimeOpName) has no exact array operand, owned Future result, and COPY_I64 payload ABI"
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task aggregate array C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task aggregate array repeat C generation failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR task aggregate array generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtValueFuture(v",
    "#include <__xl_runtime/xl6_future_aggregate.h>",
    "xlir_future_aggregate_owner",
    " = (xl6FutureAggregateRuntimeV1Get()->AllArray)(v",
    " = (xl6FutureAggregateRuntimeV1Get()->RaceArray)(v"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR task aggregate array generated C is missing mechanical ABI emission: $required"
    }
}
foreach ( $helper in @(
    '#include <__xl_runtime/xl6_future_aggregate\.h>'
) ) {
    if ( ([regex]::Matches($c, $helper)).Count -ne 1 ) {
        throw "XLIR task aggregate array helper emission is not use-directed and unique: $helper"
    }
}
foreach ( $forbidden in @(
    "xl6_task_all", "xl6_task_race", "xl6_task_all_from_array",
    "xl6_task_race_from_array", "xl6_future_to_xvalue",
    "xlir_future_array_unpack", "xlir_future_all_xvalues_array", "xlir_future_race_array"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR task aggregate array Cgen reused legacy/source-shape generation: $forbidden"
    }
}

Push-Location $Root
try {
    # The aggregate RuntimeOps deliberately target the exported xl.dll Future
    # bridge, so this strict generated-C gate links that real ABI instead of a
    # test shim or the XRT-only object used by self-contained RuntimeOps.
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_task_aggregate_array_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_task_aggregate_array_abi.exe && build\test\xlir_task_aggregate_array_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR task aggregate array strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR task.all/task.race typed-array ABI gate passed: verified retained Future xvalue boxes, exact borrowed array<Future<T>> inputs, owned result/payload ABIs, deterministic mechanical bridge C, and strict runtime."
