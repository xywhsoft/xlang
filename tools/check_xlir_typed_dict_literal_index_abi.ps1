param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_typed_dict_literal_index_abi.xl"
$support = Join-Path $Root "test\cases\cgen_class_host.c"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$loweredJson = Join-Path $outputDir "xlir_typed_dict_literal_index_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_typed_dict_literal_index_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_typed_dict_literal_index_abi.second.c"

foreach ( $path in @($xl, $source, $support, $genericHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR typed-dict literal-index ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

function Get-DefinitionByValue($document, $value) {
    return @($document.instructions | Where-Object { $_.result -eq $value })[0]
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed-dict literal-index lowering failed"
}
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$expectedDirectCounts = [ordered]@{
    "xvalue-dict-get-literal-i64" = 1
    "xvalue-dict-get-literal-u64" = 1
    "xvalue-dict-get-literal-bool" = 2
    "xvalue-dict-get-literal-f64" = 2
    "xvalue-dict-get-literal-string-copy" = 1
}
foreach ( $entry in $expectedDirectCounts.GetEnumerator() ) {
    $operations = @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq $entry.Key
    })
    if ( $operations.Count -ne $entry.Value ) {
        throw "XLIR typed-dict direct projection count is incomplete for $($entry.Key): $($operations.Count)"
    }
    foreach ( $operation in $operations ) {
        $receiverDefinition = Get-DefinitionByValue $lowered $operation.operands[0]
        $receiverType = if ( $null -ne $receiverDefinition ) {
            Get-TypeById $lowered $receiverDefinition.type
        } else { $null }
        $resultType = Get-TypeById $lowered $operation.type
        if ( $operation.operandCount -ne 1 -or $operation.hasImmediate -ne 1 -or
             $operation.targetSymbol -ne 0 -or $operation.targetValue -ne 0 -or
             $operation.calleeType -ne 0 -or $operation.payloadType -ne 0 -or
             $operation.runtimeIdentityType -ne 0 -or
             $operation.collectionAbiName -ne "none" -or
             $null -eq $receiverType -or $receiverType.name -ne "dict" -or
             $receiverType.collectionAbiName -ne "dict" -or
             $receiverType.cAbi -ne 26 -or $receiverType.ownershipAbi -ne 2 -or
             @($receiverType.params).Count -ne 1 -or
             $receiverType.params[0] -ne $operation.type -or
             $null -eq $resultType ) {
            throw "XLIR $($entry.Key) has no exact typed-dict receiver, element TypeId, literal key, and result ABI"
        }
    }
}

$safeLookups = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-object-get-literal"
})
$safeProjections = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-tagged-nullable-copy"
})
if ( $safeLookups.Count -ne 6 -or $safeProjections.Count -ne 6 ) {
    throw "XLIR typed-dict safe-index matrix is incomplete: lookups=$($safeLookups.Count), projections=$($safeProjections.Count)"
}
$payloadNames = @{}
foreach ( $projection in $safeProjections ) {
    $sourceDefinition = Get-DefinitionByValue $lowered $projection.operands[0]
    $sourceType = if ( $null -ne $sourceDefinition ) {
        Get-TypeById $lowered $sourceDefinition.type
    } else { $null }
    $resultType = Get-TypeById $lowered $projection.type
    $payloadType = Get-TypeById $lowered $projection.payloadType
    if ( $projection.operandCount -ne 1 -or
         $null -eq $sourceDefinition -or
         $sourceDefinition.runtimeOpName -ne "xvalue-object-get-literal" -or
         $null -eq $sourceType -or $sourceType.name -ne "var" -or
         $null -eq $resultType -or $resultType.name -ne "nullable" -or
         $resultType.cAbi -ne 36 -or $resultType.nullAbi -ne 3 -or
         @($resultType.params).Count -ne 1 -or
         $resultType.params[0] -ne $projection.payloadType -or
         $null -eq $payloadType ) {
        throw "XLIR typed-dict safe index has no canonical dynamic lookup and exact tagged-nullable projection ABI"
    }
    $payloadNames[$payloadType.name] = 1 + [int]($payloadNames[$payloadType.name])
}
foreach ( $payload in @("int", "bool", "float") ) {
    if ( $payloadNames[$payload] -ne 2 ) {
        throw "XLIR typed-dict safe-index payload coverage is incomplete for ${payload}: $($payloadNames[$payload])"
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed-dict literal-index C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed-dict literal-index repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR typed-dict literal-index generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_object_get_literal_i64(v",
    "xlir_xvalue_object_get_literal_u64(v",
    "xlir_xvalue_object_get_literal_bool(v",
    "xlir_xvalue_object_get_literal_f64(v",
    "xlir_xvalue_object_get_literal_string_copy(v",
    "xlir_xvalue_object_get_literal_or_null(v",
    "xlir_xvalue_project_xlir_nullable_t"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR typed-dict literal-index generated C is missing mechanical RuntimeOp emission: $required"
    }
}
foreach ( $forbidden in @(
    "xl6_typed_dict_get", "xl6_safe_dict_get",
    "xl6_infer_dict_element", "xl6_source_index_kind"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR typed-dict literal-index Cgen reconstructed source semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_typed_dict_literal_index_abi.c test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c !XL6_CGEN_LINKS! -o build\test\xlir_typed_dict_literal_index_abi.exe && build\test\xlir_typed_dict_literal_index_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed-dict literal-index strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR typed-dict literal-index ABI gate passed: five exact direct projection identities, safe tagged-nullable lookup, deterministic mechanical C, and strict runtime."
