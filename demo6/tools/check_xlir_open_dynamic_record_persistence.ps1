param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_object_var_field_lifetime.xl"
$nonPersistentSource = Join-Path $Root "test\cases\cgen_object_protocol.xl"
$closedFlowSource = Join-Path $Root "test\cases\cgen_class_implicit_upcast.xl"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_open_dynamic_record.typed.json"
$loweredJson = Join-Path $outputDir "xlir_open_dynamic_record.lowered.json"
$generatedC = Join-Path $outputDir "xlir_open_dynamic_record.c"
$generatedCSecond = Join-Path $outputDir "xlir_open_dynamic_record.second.c"
$nonPersistentJson = Join-Path $outputDir "xlir_open_dynamic_nonpersistent.lowered.json"
$closedFlowJson = Join-Path $outputDir "xlir_closed_class_flow.lowered.json"

foreach ( $path in @($xl, $source, $genericHost, $nonPersistentSource,
        $closedFlowSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR open dynamic record gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR open dynamic record typed IR failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$rootLayout = @($typed.classLayouts | Where-Object {
    $_.persistenceObjectAbiName -eq "open-dynamic-root"
})
$schemaLayout = @($typed.classLayouts | Where-Object {
    $_.persistenceObjectAbiName -eq "open-dynamic-schema" -and
    $_.persistenceFormatMask -eq 7
})
if ( $rootLayout.Count -ne 1 -or $schemaLayout.Count -ne 1 ) {
    throw "Typed XLIR does not contain one open-object root and one persisted open schema"
}
if ( $rootLayout[0].persistenceDynamicField -lt 0 -or
     $rootLayout[0].persistenceDynamicField -ge $typed.classFieldCount -or
     $rootLayout[0].persistenceDynamicHandleField -lt 0 -or
     $rootLayout[0].persistenceDynamicHandleField -ge $typed.classFieldCount -or
     $schemaLayout[0].persistenceDynamicField -ne
         $rootLayout[0].persistenceDynamicField -or
     $schemaLayout[0].persistenceDynamicHandleField -ne
         $rootLayout[0].persistenceDynamicHandleField ) {
    throw "Typed XLIR open schemas do not share one exact DynamicFields storage path"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR open dynamic record lowering failed"
}
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$dynamicGets = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-dynamic-field-get-literal"
})
$dynamicSets = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-dynamic-field-set-literal-new"
})
if ( $dynamicGets.Count -lt 4 -or $dynamicSets.Count -lt 4 -or
     @($dynamicGets | Where-Object {
        $_.operandCount -ne 1 -or $_.type -eq 0
     }).Count -ne 0 -or
     @($dynamicSets | Where-Object {
        $_.operandCount -ne 2 -or $_.type -ne 1
     }).Count -ne 0 ) {
    throw "Lowered XLIR open dynamic record RuntimeOp contracts are incomplete"
}
$staticProtocolSets = @($lowered.instructions | Where-Object {
    $_.sourceLine -in @(14, 15) -and
    $_.kindName -in @("call", "virtual-call")
})
if ( $staticProtocolSets.Count -ne 2 ) {
    throw "Lowered XLIR lost the statically resolved object member/index setter calls"
}

# Runtime openness is independent of persistence opt-in.  An `object` subclass
# with no json/xson modifier still owns the inherited DynamicFields path, while
# a flow-refined closed class must continue to use its fixed object-field ABI.
& $xl ir $nonPersistentSource --lowered --format json --emit $nonPersistentJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR non-persistent open-object lowering failed"
}
$nonPersistent = [System.IO.File]::ReadAllText(
    $nonPersistentJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$actorType = @($nonPersistent.types | Where-Object { $_.name -eq "Actor" })
$objectType = @($nonPersistent.types | Where-Object { $_.name -eq "object" })
$actorLayout = @($nonPersistent.classLayouts | Where-Object {
    $actorType.Count -eq 1 -and $_.type -eq $actorType[0].id
})
$objectLayout = @($nonPersistent.classLayouts | Where-Object {
    $objectType.Count -eq 1 -and $_.type -eq $objectType[0].id
})
$actorProtocolSets = @($nonPersistent.instructions | Where-Object {
    $_.sourceLine -in @(12, 13, 14) -and
    $_.kindName -in @("call", "virtual-call")
})
if ( $actorType.Count -ne 1 -or $objectType.Count -ne 1 -or
     $actorLayout.Count -ne 1 -or $objectLayout.Count -ne 1 -or
     $actorLayout[0].persistenceFormatMask -ne 0 -or
     $actorLayout[0].persistenceObjectAbiName -ne "open-dynamic-schema" -or
     $actorLayout[0].persistenceDynamicField -ne
         $objectLayout[0].persistenceDynamicField -or
     $actorLayout[0].persistenceDynamicHandleField -ne
         $objectLayout[0].persistenceDynamicHandleField -or
     $actorProtocolSets.Count -ne 3 ) {
    throw "XLIR incorrectly coupled open dynamic runtime storage to persistence formats"
}

& $xl ir $closedFlowSource --lowered --format json --emit $closedFlowJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR flow-refined closed-class lowering failed"
}
$closedFlow = [System.IO.File]::ReadAllText(
    $closedFlowJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$holderType = @($closedFlow.types | Where-Object { $_.name -eq "UpcastHolder" })
$holderLayout = @($closedFlow.classLayouts | Where-Object {
    $holderType.Count -eq 1 -and $_.type -eq $holderType[0].id
})
$closedSet = @($closedFlow.instructions | Where-Object {
    $_.sourceLine -eq 46 -and
    $_.runtimeOpName -eq "xvalue-object-set-literal-new"
})
$wrongDynamicSet = @($closedFlow.instructions | Where-Object {
    $_.sourceLine -eq 46 -and
    $_.runtimeOpName -eq "xvalue-dynamic-field-set-literal-new"
})
if ( $holderType.Count -ne 1 -or $holderLayout.Count -ne 1 -or
     $holderLayout[0].persistenceFormatMask -ne 0 -or
     $holderLayout[0].persistenceObjectAbiName -ne "none" -or
     $closedSet.Count -ne 1 -or $wrongDynamicSet.Count -ne 0 ) {
    throw "XLIR flow-refined closed class was misclassified as dynamic storage"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR open dynamic record C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR open dynamic record repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR open dynamic record generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_dynamic_field_get_or_null",
    "xlir_xvalue_dynamic_field_set_new",
    "xrtDynamicFieldsGet",
    "xrtDynamicFieldsSetNew",
    "xrtDynamicFieldsToValue",
    "xrtDynamicFieldsFromValue",
    "xrtDynamicFieldsMerge",
    "xrtStrView(`"__fields`")",
    "xrtStrView(`"handle`")"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR open dynamic record Cgen is missing mechanical ABI output: $required"
    }
}
foreach ( $forbidden in @(
    "__xl_std_json_stringify", "__xl_std_json_parse",
    "XL6_Hir", "XL6_Ast", "callee->text"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR open dynamic record Cgen reconstructs source semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_open_dynamic_record.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\xlir_open_dynamic_record.exe && build\test\xlir_open_dynamic_record.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR open dynamic record strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR open dynamic record persistence gate passed: explicit root/schema storage indices, distinct object versus dynamic-field RuntimeOps, deterministic mechanical C, strict runtime behavior, and JSON round-trip."
