param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_persistence_json_abi.xl"
$support = Join-Path $Root "test\cases\cgen_class_host.c"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$cgenSource = Join-Path $Root "src\cgen\xl6_ir_cgen.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_persistence_json_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_persistence_json_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_persistence_json_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_persistence_json_abi.second.c"
$legacyC = Join-Path $outputDir "xlir_persistence_json_abi.legacy.c"

foreach ( $path in @($xl, $source, $support, $genericHost, $cgenSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR JSON persistence ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR JSON persistence typed IR failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$stringType = @($typed.types | Where-Object {
    $_.name -eq "string" -and $_.persistenceValueAbiName -eq "xrt-clone"
})[0]
$intType = @($typed.types | Where-Object {
    $_.name -eq "int" -and $_.persistenceValueAbiName -eq "xrt-clone"
})[0]
if ( $null -eq $stringType -or $null -eq $intType ) {
    throw "Typed XLIR persistence lacks stable scalar/string semantic TypeIds"
}
$profileLayout = @($typed.classLayouts | Where-Object {
    $_.baseType -ne 0 -and $_.persistenceFormatMask -eq 7
})[0]
if ( $null -eq $profileLayout ) {
    throw "Typed XLIR JSON persistence has no explicit derived layout schema"
}
$profileFields = @($typed.classFields | Where-Object {
    $_.ownerType -eq $profileLayout.type
})
if ( $profileFields.Count -ne 5 -or
     @($profileFields | Where-Object { $_.jsonPersistenceFlags -eq 0 }).Count -ne 1 -or
     @($profileFields | Where-Object { $_.jsonPersistenceFlags -eq 5 }).Count -ne 1 -or
     @($profileFields | Where-Object { $_.jsonRenamed -eq 1 }).Count -ne 2 -or
     @($profileFields | Where-Object { $_.xsonPersistenceFlags -eq 0 }).Count -ne 1 -or
     @($profileFields | Where-Object { $_.xsonPersistenceFlags -eq 1 }).Count -ne 4 -or
     @($profileFields | Where-Object { $_.xsonRenamed -eq 1 }).Count -ne 1 ) {
    throw "Typed XLIR JSON/XSON persistence field schema is incomplete"
}
$intrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "persistence-json-stringify"
})
if ( $intrinsics.Count -ne 4 -or
     @($intrinsics | Where-Object {
        $_.admissionName -eq "verified" -and
        $_.payloadType -eq $profileLayout.type -and
        $_.resultAbiName -eq "owned" -and
        $_.errorAbiName -eq "no-raise"
     }).Count -ne 2 -or
     @($intrinsics | Where-Object {
        $_.admissionName -eq "verified" -and
        $_.payloadType -eq $stringType.id -and $_.operandCount -eq 1
     }).Count -ne 1 -or
     @($intrinsics | Where-Object {
        $_.admissionName -eq "verified" -and
        $_.payloadType -eq $intType.id -and $_.operandCount -eq 1
     }).Count -ne 1 -or
     @($intrinsics | Where-Object { $_.operandCount -eq 1 }).Count -ne 3 -or
     @($intrinsics | Where-Object { $_.operandCount -eq 2 }).Count -ne 1 ) {
    throw "Typed XLIR JSON persistence intrinsic ABI is incomplete"
}
$parseIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "persistence-json-parse"
})
if ( $parseIntrinsics.Count -ne 1 -or
     $parseIntrinsics[0].admissionName -ne "verified" -or
     $parseIntrinsics[0].payloadType -ne $profileLayout.type -or
     $parseIntrinsics[0].resultType -ne $profileLayout.type -or
     $parseIntrinsics[0].resultAbiName -ne "owned" -or
     $parseIntrinsics[0].operandCount -ne 1 ) {
    throw "Typed XLIR JSON parse persistence intrinsic ABI is incomplete"
}
$xsonIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "persistence-xson-stringify"
})
if ( $xsonIntrinsics.Count -ne 2 -or
     @($xsonIntrinsics | Where-Object {
        $_.admissionName -eq "verified" -and
        $_.payloadType -eq $profileLayout.type -and
        $_.resultAbiName -eq "owned" -and $_.operandCount -eq 1
     }).Count -ne 1 -or
     @($xsonIntrinsics | Where-Object {
        $_.admissionName -eq "verified" -and
        $_.payloadType -eq $stringType.id -and
        $_.resultAbiName -eq "owned" -and $_.operandCount -eq 1
     }).Count -ne 1 ) {
    throw "Typed XLIR XSON persistence stringify intrinsic ABI is incomplete"
}
$xsonParseIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "persistence-xson-parse"
})
if ( $xsonParseIntrinsics.Count -ne 1 -or
     $xsonParseIntrinsics[0].admissionName -ne "verified" -or
     $xsonParseIntrinsics[0].payloadType -ne $profileLayout.type -or
     $xsonParseIntrinsics[0].resultType -ne $profileLayout.type -or
     $xsonParseIntrinsics[0].resultAbiName -ne "owned" -or
     $xsonParseIntrinsics[0].operandCount -ne 1 ) {
    throw "Typed XLIR XSON persistence parse intrinsic ABI is incomplete"
}
$jsonWriteIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "persistence-json-write"
})
$jsonReadIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "persistence-json-read"
})
$xsonWriteIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "persistence-xson-write"
})
$xsonReadIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "persistence-xson-read"
})
if ( $jsonWriteIntrinsics.Count -ne 2 -or
     @($jsonWriteIntrinsics | Where-Object {
        $_.admissionName -eq "verified" -and
        $_.payloadType -eq $profileLayout.type -and
        $_.resultType -eq 1 -and $_.resultAbiName -eq "value" -and
        $_.operandCount -eq 3
     }).Count -ne 1 -or
     @($jsonWriteIntrinsics | Where-Object {
        $_.admissionName -eq "verified" -and
        $_.payloadType -eq $stringType.id -and
        $_.resultType -eq 1 -and $_.resultAbiName -eq "value" -and
        $_.operandCount -eq 2
     }).Count -ne 1 -or
     $jsonReadIntrinsics.Count -ne 1 -or
     $jsonReadIntrinsics[0].admissionName -ne "verified" -or
     $jsonReadIntrinsics[0].payloadType -ne $profileLayout.type -or
     $jsonReadIntrinsics[0].resultType -ne $profileLayout.type -or
     $jsonReadIntrinsics[0].resultAbiName -ne "owned" -or
     $jsonReadIntrinsics[0].operandCount -ne 1 ) {
    throw "Typed XLIR JSON persistence file ABI is incomplete"
}
if ( $xsonWriteIntrinsics.Count -ne 1 -or
     $xsonWriteIntrinsics[0].admissionName -ne "verified" -or
     $xsonWriteIntrinsics[0].payloadType -ne $profileLayout.type -or
     $xsonWriteIntrinsics[0].resultType -ne 1 -or
     $xsonWriteIntrinsics[0].resultAbiName -ne "value" -or
     $xsonWriteIntrinsics[0].operandCount -ne 2 -or
     $xsonReadIntrinsics.Count -ne 1 -or
     $xsonReadIntrinsics[0].admissionName -ne "verified" -or
     $xsonReadIntrinsics[0].payloadType -ne $profileLayout.type -or
     $xsonReadIntrinsics[0].resultType -ne $profileLayout.type -or
     $xsonReadIntrinsics[0].resultAbiName -ne "owned" -or
     $xsonReadIntrinsics[0].operandCount -ne 1 ) {
    throw "Typed XLIR XSON persistence file ABI is incomplete"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR JSON persistence lowering failed"
}
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-json-stringify"
})
if ( $operations.Count -ne 4 -or
     @($operations | Where-Object {
        $_.operandCount -eq 2 -and
        $_.payloadType -eq $profileLayout.type -and
        $_.kindName -eq "runtime-call"
     }).Count -ne 2 -or
     @($operations | Where-Object {
        $_.operandCount -eq 2 -and $_.payloadType -eq $stringType.id -and
        $_.kindName -eq "runtime-call"
     }).Count -ne 1 -or
     @($operations | Where-Object {
        $_.operandCount -eq 2 -and $_.payloadType -eq $intType.id -and
        $_.kindName -eq "runtime-call"
     }).Count -ne 1 ) {
    throw "Lowered XLIR JSON persistence RuntimeOp ABI is incomplete"
}
$parseOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-json-parse"
})
if ( $parseOperations.Count -ne 1 -or
     $parseOperations[0].operandCount -ne 2 -or
     $parseOperations[0].payloadType -ne $profileLayout.type -or
     $parseOperations[0].type -ne $profileLayout.type ) {
    throw "Lowered XLIR JSON parse persistence RuntimeOp ABI is incomplete"
}
$xsonOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-xson-stringify"
})
$xsonParseOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-xson-parse"
})
if ( $xsonOperations.Count -ne 2 -or
     @($xsonOperations | Where-Object {
        $_.operandCount -eq 2 -and
        $_.payloadType -eq $profileLayout.type -and
        $_.kindName -eq "runtime-call"
     }).Count -ne 1 -or
     @($xsonOperations | Where-Object {
        $_.operandCount -eq 2 -and
        $_.payloadType -eq $stringType.id -and
        $_.kindName -eq "runtime-call"
     }).Count -ne 1 -or
     $xsonParseOperations.Count -ne 1 -or
     $xsonParseOperations[0].operandCount -ne 2 -or
     $xsonParseOperations[0].payloadType -ne $profileLayout.type -or
     $xsonParseOperations[0].type -ne $profileLayout.type ) {
    throw "Lowered XLIR XSON persistence RuntimeOp ABIs are incomplete"
}
$jsonWriteOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-json-write"
})
$jsonReadOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-json-read"
})
$xsonWriteOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-xson-write"
})
$xsonReadOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "persistence-xson-read"
})
if ( $jsonWriteOperations.Count -ne 2 -or
     @($jsonWriteOperations | Where-Object {
        $_.operandCount -eq 3 -and
        $_.payloadType -eq $profileLayout.type -and $_.type -eq 1
     }).Count -ne 1 -or
     @($jsonWriteOperations | Where-Object {
        $_.operandCount -eq 3 -and
        $_.payloadType -eq $stringType.id -and $_.type -eq 1
     }).Count -ne 1 -or
     $jsonReadOperations.Count -ne 1 -or
     $jsonReadOperations[0].operandCount -ne 2 -or
     $jsonReadOperations[0].payloadType -ne $profileLayout.type -or
     $jsonReadOperations[0].type -ne $profileLayout.type -or
     $xsonWriteOperations.Count -ne 1 -or
     $xsonWriteOperations[0].operandCount -ne 3 -or
     $xsonWriteOperations[0].payloadType -ne $profileLayout.type -or
     $xsonWriteOperations[0].type -ne 1 -or
     $xsonReadOperations.Count -ne 1 -or
     $xsonReadOperations[0].operandCount -ne 2 -or
     $xsonReadOperations[0].payloadType -ne $profileLayout.type -or
     $xsonReadOperations[0].type -ne $profileLayout.type ) {
    throw "Lowered XLIR JSON/XSON persistence file RuntimeOp ABIs are incomplete"
}

# A non-xvalue semantic payload must cross one explicit box/store/load/use/release
# transaction.  The RuntimeOp retains the original payload TypeId while its
# physical operand is the canonical dynamic xvalue type.
$boxedOperations = @($operations + $xsonOperations + $jsonWriteOperations |
    Where-Object { $_.payloadType -eq $stringType.id -or
                   $_.payloadType -eq $intType.id })
if ( $boxedOperations.Count -ne 4 ) {
    throw "Lowered XLIR persistence scalar/string projection matrix is incomplete"
}
foreach ( $operation in $boxedOperations ) {
    $valueIndex = if ( $operation.runtimeOpName -like "*write" ) { 1 } else { 0 }
    $physicalValue = [int]@($operation.operands)[$valueIndex]
    $loads = @($lowered.instructions | Where-Object {
        $_.result -eq $physicalValue -and $_.kindName -eq "load"
    })
    if ( $loads.Count -ne 1 ) {
        throw "Persistence boxed payload has no unique physical xvalue load"
    }
    $storage = [long]$loads[0].targetSymbol
    $stores = @($lowered.instructions | Where-Object {
        $_.kindName -eq "store" -and $_.targetSymbol -eq $storage -and
        $_.type -eq $loads[0].type -and $_.operandCount -eq 1
    })
    $releases = @($lowered.instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $storage -and
        $_.type -eq $loads[0].type
    })
    $box = @(if ( $stores.Count -eq 1 ) {
        @($lowered.instructions | Where-Object {
            $_.result -eq [int]@($stores[0].operands)[0] -and
            $_.kindName -eq "runtime-call"
        })
    } else { @() })
    $expectedBox = if ( $operation.payloadType -eq $stringType.id ) {
        "xvalue-string-copy"
    } else { "xvalue-i64" }
    if ( $stores.Count -ne 1 -or $releases.Count -ne 1 -or $box.Count -ne 1 -or
         $box[0].runtimeOpName -ne $expectedBox -or
         $box[0].id -ge $stores[0].id -or $stores[0].id -ge $loads[0].id -or
         $loads[0].id -ge $operation.id -or $operation.id -ge $releases[0].id ) {
        throw ("Persistence semantic payload lost its exact box/store/load/use/release transaction: " +
            "op=$($operation.id), payload=$($operation.payloadType), " +
            "stores=$($stores.Count), loads=$($loads.Count), " +
            "boxes=$($box.Count), releases=$($releases.Count)")
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR JSON persistence C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR JSON persistence repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR JSON persistence generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtJsonStringify(value, pretty, NULL)",
    "xrtJsonParse(xrtStrView(text))",
    "xrtJsonStringifyFile(path, value, pretty)",
    "xrtJsonParseFile(path)",
    "xrtXsonStringify(value, pretty, NULL)",
    "xrtXsonParse(xrtStrView(text))",
    "xrtXsonStringifyFile(path, value, pretty)",
    "xrtXsonParseFile(path)",
    "xlir_persist_json_value_t",
    "xlir_persist_json_assign_t",
    "xlir_persist_json_parse_t",
    "xlir_persist_json_write_t",
    "xlir_persist_json_read_t",
    "xlir_persist_xson_value_t",
    "xlir_persist_xson_assign_t",
    "xlir_persist_xson_parse_t",
    "xlir_persist_xson_write_t",
    "xlir_persist_xson_read_t",
    "xrtStrView(`"display_name`")",
    "xrtStrView(`"secret_name`")",
    ("xlir_persist_json_stringify_t{0}" -f $stringType.id),
    ("xlir_persist_json_stringify_t{0}" -f $intType.id),
    ("xlir_persist_xson_stringify_t{0}" -f $stringType.id),
    ("xlir_persist_json_write_t{0}" -f $stringType.id)
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR JSON persistence Cgen is missing mechanical schema output: $required"
    }
}
$cgen = [System.IO.File]::ReadAllText($cgenSource, [System.Text.Encoding]::UTF8)
foreach ( $include in [regex]::Matches(
    $cgen, '#include "(?<path>src/cgen/xl6_ir_cgen[^\"]+\.c)"') ) {
    $path = Join-Path $Root ($include.Groups["path"].Value -replace '/', '\')
    $cgen += "`n" + [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}
foreach ( $forbidden in @(
    "__xl_std_json_stringify", "__xl_std_json_parse",
    "__xl_std_json_write", "__xl_std_json_read",
    "__xl_std_xson_stringify", "__xl_std_xson_parse",
    "__xl_std_xson_write", "__xl_std_xson_read",
    "json.stringify", "json.parse", "json.write", "json.read",
    "xson.stringify", "xson.parse", "xson.write", "xson.read",
    "XL6_Hir", "XL6_Ast",
    "callee->text"
) ) {
    if ( $cgen.Contains($forbidden) ) {
        throw "XLIR JSON persistence Cgen reconstructs source semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_persistence_json_abi.c -include test\cases\cgen_class_host.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\xlir_persistence_json_abi.exe && build\test\xlir_persistence_json_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR JSON persistence strict-C/runtime gate failed"
    }

    & $xl cgen $source --backend legacy --emit $legacyC
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR JSON persistence legacy C generation failed"
    }
    $legacyCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror build\tmp\xlir_persistence_json_abi.legacy.c test\cases\cgen_class_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_persistence_json_abi.legacy.exe && build\test\xlir_persistence_json_abi.legacy.exe'
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR JSON persistence legacy differential runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR JSON/XSON persistence ABI gate passed: one format-parametric schema model, exact stringify/parse/write/read intrinsic and RuntimeOp contracts, explicit scalar/string boxing lifetimes, explicit default-instance reconstruction, format-specific inherited/nested projection, deterministic mechanical C, strict file/runtime behavior, and legacy differential semantics."
