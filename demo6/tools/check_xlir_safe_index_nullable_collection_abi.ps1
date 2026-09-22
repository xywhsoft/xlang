param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_safe_index_nullable_collection_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$loweredJson = Join-Path $outputDir "xlir_safe_index_nullable_collection_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_safe_index_nullable_collection_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_safe_index_nullable_collection_abi.second.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR safe-index nullable-collection ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR safe-index nullable-collection lowering failed"
}
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$safeArray = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-array-at-i64-or-null"
})
$safeList = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-int-map-at-i64-or-null"
})
$tagged = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-tagged-nullable-copy"
})
$strings = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-nullable-string-copy"
})
$narrowedReads = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-array-at-i64-integer"
})
if ( $safeArray.Count -ne 4 -or $safeList.Count -ne 2 -or
     $tagged.Count -ne 4 -or $strings.Count -ne 2 -or
     $narrowedReads.Count -ne 1 ) {
    throw "XLIR safe-index operation/projection matrix is incomplete: array=$($safeArray.Count), list=$($safeList.Count), tagged=$($tagged.Count), string=$($strings.Count), narrowed=$($narrowedReads.Count)"
}
$nullableReceiverCount = 0
foreach ( $operation in @($safeArray + $safeList) ) {
    $receiverDefinition = @($lowered.instructions | Where-Object {
        $_.result -eq $operation.operands[0]
    })[0]
    $receiverType = if ( $null -ne $receiverDefinition ) {
        Get-TypeById $lowered $receiverDefinition.type
    } else { $null }
    $collectionType = $receiverType
    if ( $null -ne $receiverType -and $receiverType.name -eq "nullable" -and
         @($receiverType.params).Count -eq 1 ) {
        ++$nullableReceiverCount
        $collectionType = Get-TypeById $lowered $receiverType.params[0]
    }
    $expectedCollection = if ( $operation.runtimeOpName -eq
        "xvalue-array-at-i64-or-null" ) { "array" } else { "list" }
    $resultType = Get-TypeById $lowered $operation.type
    if ( $operation.operandCount -ne 2 -or
         $null -eq $receiverType -or $receiverType.cAbi -ne 26 -or
         $null -eq $collectionType -or
         $collectionType.collectionAbiName -ne $expectedCollection -or
         $null -eq $resultType -or $resultType.name -ne "var" ) {
        throw "XLIR $($operation.runtimeOpName) has no exact xvalue receiver, static collection payload, i64 key, and canonical-null result ABI"
    }
}
if ( $nullableReceiverCount -ne 2 ) {
    throw "XLIR safe-index nullable receiver coverage is incomplete: $nullableReceiverCount"
}
foreach ( $projection in @($tagged + $strings) ) {
    $sourceDefinition = @($lowered.instructions | Where-Object {
        $_.result -eq $projection.operands[0]
    })[0]
    $resultType = Get-TypeById $lowered $projection.type
    if ( $projection.operandCount -ne 1 -or
         $null -eq $sourceDefinition -or
         $sourceDefinition.runtimeOpName -notmatch '-at-i64-or-null$' -or
         $null -eq $resultType -or $resultType.name -ne "nullable" ) {
        throw "XLIR safe-index projection has no exact canonical-null source and nullable result ABI"
    }
}
$narrowedReceiver = @($lowered.instructions | Where-Object {
    $_.result -eq $narrowedReads[0].operands[0]
})[0]
if ( $null -eq $narrowedReceiver -or
     $narrowedReceiver.kindName -ne "nullable-unwrap" ) {
    throw "XLIR ordinary narrowed collection index does not consume an explicit NULLABLE_UNWRAP"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR safe-index nullable-collection C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR safe-index nullable-collection repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR safe-index nullable-collection generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtValueArrayAt(v", "xrtValueIntMapGet(v",
    "if (v", "== NULL) v", "= xrtValueNull();",
    "xlir_xvalue_project_nullable_string_copy(v",
    "xlir_xvalue_project_xlir_nullable_t",
    "xlir_xvalue_array_at_i64_integer(v"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR safe-index nullable-collection generated C is missing mechanical RuntimeOp emission: $required"
    }
}
foreach ( $forbidden in @(
    "xl6_safe_index", "xl6_nullable_array_index",
    "xl6_flow_narrowed_index", "xl6_source_safe_index"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR safe-index nullable-collection Cgen reconstructed source semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_safe_index_nullable_collection_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\test\xlir_safe_index_nullable_collection_abi.exe && build\test\xlir_safe_index_nullable_collection_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR safe-index nullable-collection strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR safe-index nullable-collection ABI gate passed: exact nullable/non-null collection shapes, canonical-null projections, explicit flow unwrap, deterministic mechanical C, and strict runtime."
