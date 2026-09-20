param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_persistence_recursive_collection_values.xl"
$moduleHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_persistence_recursive_collection_values.typed.json"
$loweredJson = Join-Path $outputDir "xlir_persistence_recursive_collection_values.lowered.json"
$generatedC = Join-Path $outputDir "xlir_persistence_recursive_collection_values.c"
$generatedCSecond = Join-Path $outputDir "xlir_persistence_recursive_collection_values.second.c"

foreach ( $path in @($xl, $source, $moduleHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR recursive collection persistence gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR recursive collection persistence typed IR failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$itemType = @($typed.types | Where-Object { $_.name -eq "CollectionItem" })[0]
$envelopeType = @($typed.types | Where-Object {
    $_.name -eq "CollectionEnvelope"
})[0]
if ( $null -eq $itemType -or $null -eq $envelopeType -or
     $itemType.persistenceValueAbiName -ne "class-schema" -or
     $envelopeType.persistenceValueAbiName -ne "class-schema" ) {
    throw "Typed XLIR recursive collection persistence has no exact class value ABI"
}
$itemLayout = @($typed.classLayouts | Where-Object {
    $_.type -eq $itemType.id
})[0]
$envelopeLayout = @($typed.classLayouts | Where-Object {
    $_.type -eq $envelopeType.id
})[0]
if ( $null -eq $itemLayout -or $null -eq $envelopeLayout -or
     $itemLayout.persistenceConstructionAbiName -ne "schema-default" -or
     $envelopeLayout.persistenceConstructionAbiName -ne "schema-default" ) {
    throw "Typed XLIR recursive collection persistence has no verified source-free construction ABI"
}

$expected = @{
    "list" = @("list-sequence", "empty-list")
    "set" = @("set-sequence", "empty-set")
    "dict" = @("dict-object", "empty-dict")
}
$collectionTypes = @{}
$fields = @($typed.classFields | Where-Object {
    $_.ownerType -eq $envelopeType.id
})
foreach ( $collectionName in $expected.Keys ) {
    $type = @($typed.types | Where-Object {
        $_.collectionAbiName -eq $collectionName -and
        $_.persistenceValueAbiName -eq $expected[$collectionName][0] -and
        $_.persistencePayloadType -eq $itemType.id
    })[0]
    $field = @($fields | Where-Object {
        $_.type -eq $type.id -and
        $_.initializerAbiName -eq "xvalue-container-empty" -and
        $_.persistenceDefaultAbiName -eq $expected[$collectionName][1]
    })[0]
    if ( $null -eq $type -or $null -eq $field ) {
        throw "Typed XLIR recursive collection persistence is missing $collectionName ABI/default facts"
    }
    $collectionTypes[$collectionName] = $type
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR recursive collection persistence lowering failed"
}
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$stringify = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-json-stringify" -and
    $_.payloadType -eq $envelopeType.id
})
$parse = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-json-parse" -and
    $_.payloadType -eq $envelopeType.id
})
if ( $stringify.Count -ne 2 -or $parse.Count -ne 1 ) {
    throw "Lowered XLIR recursive collection persistence RuntimeOps are incomplete"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR recursive collection persistence C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR recursive collection persistence repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR recursive collection persistence generated C is not deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $type in $collectionTypes.Values ) {
    foreach ( $direction in @("project", "restore") ) {
        if ( ! $c.Contains("xlir_persist_json_${direction}_t$($type.id)") ) {
            throw "XLIR recursive collection persistence Cgen is missing $direction helper for TypeId $($type.id)"
        }
    }
}
foreach ( $required in @(
    "xrtValueIntMapSetNew(result, (int64_t)index, item)",
    "xrtValueSetAddNew(result, item)",
    "xrtValueObjectSetNew(result, key, item)",
    "xrtValueTypeIdBind(result",
    "xlir_persist_json_default_t$($envelopeType.id)"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR recursive collection persistence Cgen is missing verified output: $required"
    }
}
foreach ( $forbidden in @(
    "xl6_xrt_json_stringify",
    "xl6_xrt_typed_list_to_sequence_map",
    "xl6_xrt_typed_set_to_sequence_map",
    "xl6_xrt_typed_dict_to_xvalue_map"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR recursive collection persistence generated C leaked a legacy bridge: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_persistence_recursive_collection_values.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\xlir_persistence_recursive_collection_values.exe && build\test\xlir_persistence_recursive_collection_values.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR recursive collection persistence strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR recursive collection persistence gate passed: list/set/dict<class> projection and restore use explicit recursive TypeIds, source-free empty-container defaults, deterministic mechanical C, and an XLIR-only runtime contract."
