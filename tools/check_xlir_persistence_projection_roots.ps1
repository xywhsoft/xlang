param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_persistence_projection_roots.xl"
$moduleHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_persistence_projection_roots.typed.json"
$loweredJson = Join-Path $outputDir "xlir_persistence_projection_roots.lowered.json"
$generatedC = Join-Path $outputDir "xlir_persistence_projection_roots.c"
$generatedCSecond = Join-Path $outputDir "xlir_persistence_projection_roots.second.c"

foreach ( $path in @($xl, $source, $moduleHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR persistence projection-root gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR persistence projection-root typed IR failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$intType = @($typed.types | Where-Object { $_.name -eq "int" })[0]
$stringType = @($typed.types | Where-Object { $_.name -eq "string" })[0]
$itemType = @($typed.types | Where-Object { $_.name -eq "ProjectionItem" })[0]
if ( $null -eq $intType -or $null -eq $stringType -or $null -eq $itemType ) {
    throw "Typed XLIR persistence projection roots have no canonical payload TypeIds"
}

$expectedCollections = @(
    @("array", "array", $intType.id),
    @("list", "list-sequence", $stringType.id),
    @("set", "set-sequence", $intType.id),
    @("dict", "dict-object", $intType.id),
    @("array", "array", $itemType.id),
    @("list", "list-sequence", $itemType.id),
    @("set", "set-sequence", $itemType.id),
    @("dict", "dict-object", $itemType.id)
)
$collectionTypes = @()
foreach ( $expected in $expectedCollections ) {
    $match = @($typed.types | Where-Object {
        $_.collectionAbiName -eq $expected[0] -and
        $_.persistenceValueAbiName -eq $expected[1] -and
        $_.persistencePayloadType -eq $expected[2]
    })
    if ( $match.Count -ne 1 ) {
        throw "Typed XLIR persistence projection root is not canonical: $($expected -join '/')"
    }
    $collectionTypes += $match[0]
}

$persistenceIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "persistence-json-stringify" -or
    $_.kindName -eq "persistence-xlon-stringify"
})
if ( $persistenceIntrinsics.Count -ne 16 ) {
    throw "Typed XLIR persistence projection roots did not normalize every canonical stdlib call"
}
$intrinsicOrigins = @($persistenceIntrinsics | ForEach-Object { $_.origin })
if ( @($typed.callSites | Where-Object {
        $intrinsicOrigins -contains $_.origin
    }).Count -ne 0 ) {
    throw "Typed XLIR persistence projection roots leaked duplicate CallSites"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR persistence projection-root lowering failed"
}
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$jsonOps = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-json-stringify"
})
$xlonOps = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-xlon-stringify"
})
if ( $jsonOps.Count -ne 8 -or $xlonOps.Count -ne 8 ) {
    throw "Lowered XLIR persistence projection RuntimeOps are incomplete"
}
foreach ( $type in $collectionTypes ) {
    if ( @($jsonOps | Where-Object { $_.payloadType -eq $type.id }).Count -ne 1 -or
         @($xlonOps | Where-Object { $_.payloadType -eq $type.id }).Count -ne 1 ) {
        throw "Lowered XLIR persistence projection lost an exact collection payload TypeId"
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR persistence projection-root C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR persistence projection-root repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR persistence projection-root generated C is not deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $type in $collectionTypes ) {
    if ( ! $c.Contains("xlir_persist_json_project_t$($type.id)") -or
         ! $c.Contains("xlir_persist_xlon_project_t$($type.id)") ) {
        throw "XLIR persistence projection Cgen is missing a TypeId-selected helper"
    }
}
foreach ( $required in @(
    "xrtValueIterAdvance(iterator, NULL, &borrowed)",
    "xrtValueObjectAt(source, index, &key)",
    "xrtValueType(source) != 10",
    "xrtValueType(source) != 11",
    "xrtValueType(source) != 12"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR persistence projection Cgen is missing mechanical collection output: $required"
    }
}
foreach ( $forbidden in @(
    "xl6_xrt_json_stringify", "xl6_xrt_xlon_stringify",
    "xl6_xrt_typed_list_to_sequence_value",
    "xl6_xrt_typed_set_to_sequence_value",
    "xl6_xrt_typed_dict_to_xvalue"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR persistence projection generated C leaked a legacy semantic bridge: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_persistence_projection_roots.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\xlir_persistence_projection_roots.exe && build\test\xlir_persistence_projection_roots.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR persistence projection-root strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR persistence projection-root gate passed: array/list/set/dict scalar and class payloads use explicit recursive value ABIs, deterministic mechanical C, and strict runtime behavior."
