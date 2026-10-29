param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_persistence_recursive_class_values.xl"
$moduleHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_persistence_recursive_class_values.typed.json"
$loweredJson = Join-Path $outputDir "xlir_persistence_recursive_class_values.lowered.json"
$generatedC = Join-Path $outputDir "xlir_persistence_recursive_class_values.c"
$generatedCSecond = Join-Path $outputDir "xlir_persistence_recursive_class_values.second.c"

foreach ( $path in @($xl, $source, $moduleHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR recursive persistence gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR recursive persistence typed IR failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$itemType = @($typed.types | Where-Object { $_.name -eq "Item" })[0]
$envelopeType = @($typed.types | Where-Object { $_.name -eq "Envelope" })[0]
if ( $null -eq $itemType -or $null -eq $envelopeType -or
     $itemType.persistenceValueAbiName -ne "class-schema" -or
     $envelopeType.persistenceValueAbiName -ne "class-schema" ) {
    throw "Typed XLIR recursive persistence has no exact class value ABI"
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
    throw "Typed XLIR recursive persistence has no verified class construction ABI"
}
$envelopeFields = @($typed.classFields | Where-Object {
    $_.ownerType -eq $envelopeType.id
})
$selectedField = @($envelopeFields | Where-Object {
    $_.persistenceDefaultAbiName -eq "xvalue-null"
})[0]
$itemsField = @($envelopeFields | Where-Object {
    $_.persistenceDefaultAbiName -eq "empty-array"
})[0]
if ( $null -eq $selectedField -or $null -eq $itemsField ) {
    throw "Typed XLIR recursive persistence defaults are incomplete"
}
$selectedType = @($typed.types | Where-Object {
    $_.id -eq $selectedField.type
})[0]
$itemsType = @($typed.types | Where-Object {
    $_.id -eq $itemsField.type
})[0]
if ( $selectedType.persistenceValueAbiName -ne "nullable" -or
     $selectedType.persistencePayloadType -ne $itemType.id -or
     $itemsType.persistenceValueAbiName -ne "array" -or
     $itemsType.persistencePayloadType -ne $itemType.id ) {
    throw "Typed XLIR nullable/array recursive payload identity is incomplete"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR recursive persistence lowering failed"
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
    throw "Lowered XLIR recursive persistence RuntimeOps are incomplete"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR recursive persistence C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR recursive persistence repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR recursive persistence generated C is not deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_persist_json_default_t$($itemType.id)",
    "xlir_persist_json_project_t$($selectedType.id)",
    "xlir_persist_json_restore_t$($selectedType.id)",
    "xlir_persist_json_project_t$($itemsType.id)",
    "xlir_persist_json_restore_t$($itemsType.id)",
    "xrtValueArrayGet(source, index)",
    "xrtValueTypeIdBind(result"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR recursive persistence Cgen is missing verified output: $required"
    }
}
foreach ( $forbidden in @("xl6_xrt_json_stringify") ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR recursive persistence generated C leaked a legacy bridge or class metadata: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_persistence_recursive_class_values.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\xlir_persistence_recursive_class_values.exe && build\test\xlir_persistence_recursive_class_values.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR recursive persistence strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR recursive persistence gate passed: nullable class and array<class> payloads use canonical TypeIds, verified source-free construction, deterministic mechanical C, and strict runtime behavior."
