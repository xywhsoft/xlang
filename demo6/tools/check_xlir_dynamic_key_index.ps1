param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_xvalue_dynamic_key_index.xl"
$hostSource = Join-Path $Root "test\cases\xlir_xvalue_dynamic_key_index_host.c"
$setSource = Join-Path $Root "test\cases\xlir_xvalue_dynamic_key_index_set.xl"
$setHostSource = Join-Path $Root "test\cases\xlir_xvalue_dynamic_key_index_set_host.c"
$typedDictSource = Join-Path $Root "test\cases\xlir_typed_dict_scalar_set.xl"
$typedDictHostSource = Join-Path $Root "test\cases\xlir_typed_dict_scalar_set_host.c"
    $temporarySource = Join-Path $Root "test\cases\xlir_xvalue_dynamic_key_temporary_receiver.xl"
    $temporaryHostSource = Join-Path $Root "test\cases\xlir_xvalue_dynamic_key_temporary_receiver_host.c"
$outputDir = Join-Path $Root "build\test"
$textPath = Join-Path $outputDir "xlir_xvalue_dynamic_key_index.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_xvalue_dynamic_key_index.lowered.json"
$cPath = Join-Path $outputDir "xlir_xvalue_dynamic_key_index_generated.c"
$setTextPath = Join-Path $outputDir "xlir_xvalue_dynamic_key_index_set.lowered.xlir"
$setJsonPath = Join-Path $outputDir "xlir_xvalue_dynamic_key_index_set.lowered.json"
$setCPath = Join-Path $outputDir "xlir_xvalue_dynamic_key_index_set_generated.c"
$typedDictTextPath = Join-Path $outputDir "xlir_typed_dict_scalar_set.lowered.xlir"
$typedDictCPath = Join-Path $outputDir "xlir_typed_dict_scalar_set_generated.c"
    $temporaryTextPath = Join-Path $outputDir "xlir_xvalue_dynamic_key_temporary_receiver.lowered.xlir"
    $temporaryJsonPath = Join-Path $outputDir "xlir_xvalue_dynamic_key_temporary_receiver.lowered.json"
    $temporaryCPath = Join-Path $outputDir "xlir_xvalue_dynamic_key_temporary_receiver_generated.c"
$exePath = Join-Path $outputDir "xlir_xvalue_dynamic_key_index_host.exe"
$setExePath = Join-Path $outputDir "xlir_xvalue_dynamic_key_index_set_host.exe"
    $typedDictExePath = Join-Path $outputDir "xlir_typed_dict_scalar_set_host.exe"
    $temporaryExePath = Join-Path $outputDir "xlir_xvalue_dynamic_key_temporary_receiver_host.exe"

foreach ( $path in @($xl, $source, $hostSource, $setSource, $setHostSource,
                      $typedDictSource, $typedDictHostSource, $temporarySource,
                      $temporaryHostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR dynamic-key index gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format text --emit $textPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key index text IR failed" }
$text = [System.IO.File]::ReadAllText($textPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($text, "runtime=xvalue-index-get-xvalue-key")).Count -ne 6 ) {
    throw "XLIR dynamic-key index lowering did not emit six exact runtime operations"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key index JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$ops = @($json.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-index-get-xvalue-key"
})
if ( $ops.Count -ne 6 -or @($ops | Where-Object {
        $_.kindName -ne "runtime-call" -or $_.operandCount -ne 2 -or
        $_.immediateText -ne $null
    }).Count -ne 0 ) {
    throw "XLIR dynamic-key index JSON has no exact two-borrow operation ABI"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key index Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static xvalue* xlir_xvalue_index_get_xvalue_key_or_null",
    "int collectionType = xrtValueType(collection); int keyType = xrtValueType(key);",
    "if (keyType == 2 && xrtValueGetInt(key, &index))",
    "if (collectionType == 9) return xlir_xvalue_array_at_i64_or_null(collection, index);",
    "else if (collectionType == 10) item = xrtValueIntMapGet(collection, index);",
    "if (keyType == 4 && xrtValueGetString(key, &text))",
    "if (collectionType != 12) return xrtValueNull();",
    "item = xrtValueObjectGet(collection, text);",
    "return item != NULL ? item : xrtValueNull();"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR dynamic-key index C rendering is incomplete: $required"
    }
}

& $xl ir $typedDictSource --lowered --format text --emit $typedDictTextPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR typed dict scalar write text IR failed" }
$typedDictText = [System.IO.File]::ReadAllText($typedDictTextPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "runtime=xvalue-i64",
    "runtime=xvalue-object-set-literal-new",
    "runtime=xvalue-object-set-string-new",
    "runtime=xvalue-dict-get-literal-i64",
    "trap-if-false"
) ) {
    if ( ! $typedDictText.Contains($required) ) {
        throw "XLIR typed dict scalar write lowering is incomplete: $required"
    }
}

& $xl cgen $typedDictSource --backend xlir --emit $typedDictCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR typed dict scalar write Cgen failed" }
$typedDictGenerated = [System.IO.File]::ReadAllText($typedDictCPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtValueObjectSetNew(",
    "xrtValueInt(",
    "xlir_xvalue_object_get_literal_i64("
) ) {
    if ( ! $typedDictGenerated.Contains($required) ) {
        throw "XLIR typed dict scalar write C rendering is incomplete: $required"
    }
}

& $xl ir $setSource --lowered --format text --emit $setTextPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key index write text IR failed" }
$setText = [System.IO.File]::ReadAllText($setTextPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($setText, "runtime=xvalue-index-set-xvalue-key-new")).Count -ne 3 -or
     ([regex]::Matches($setText, "runtime=xvalue-index-get-xvalue-key")).Count -ne 3 -or
     ([regex]::Matches($setText, "trap-if-false")).Count -lt 3 ) {
    throw "XLIR dynamic-key index write lowering has no exact consume/trap contract"
}

& $xl ir $setSource --lowered --format json --emit $setJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key index write JSON IR failed" }
$setJson = [System.IO.File]::ReadAllText($setJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$setOps = @($setJson.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-index-set-xvalue-key-new"
})
if ( $setOps.Count -ne 3 -or @($setOps | Where-Object {
        $_.kindName -ne "runtime-call" -or $_.operandCount -ne 3 -or
        $_.immediateText -ne $null
    }).Count -ne 0 ) {
    throw "XLIR dynamic-key index write JSON has no exact borrowed-key/consumed-item ABI"
}

& $xl cgen $setSource --backend xlir --emit $setCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key index write Cgen failed" }
$setGenerated = [System.IO.File]::ReadAllText($setCPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static bool xlir_xvalue_index_set_xvalue_key_new",
    "if (collectionType == 9) return xlir_xvalue_array_set_i64_new(collection, index, item);",
    "if (collectionType == 10) return xrtValueIntMapSetNew(collection, index, item);",
    "keyType == 4 && collectionType == 12",
    "return xrtValueObjectSetNew(collection, text, item);",
    "xrtValueRelease(item);"
) ) {
    if ( ! $setGenerated.Contains($required) ) {
        throw "XLIR dynamic-key index write C rendering is incomplete: $required"
    }
}

& $xl ir $temporarySource --lowered --format text --emit $temporaryTextPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key temporary receiver text IR failed" }
$temporaryText = [System.IO.File]::ReadAllText($temporaryTextPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "kind=temporary",
    "runtime=xvalue-index-get-xvalue-key",
    "runtime=xvalue-retain",
    "release"
) ) {
    if ( ! $temporaryText.Contains($required) ) {
        throw "XLIR dynamic-key temporary receiver lowering is incomplete: $required"
    }
}

& $xl ir $temporarySource --lowered --format json --emit $temporaryJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key temporary receiver JSON IR failed" }
$temporaryJson = [System.IO.File]::ReadAllText($temporaryJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$temporaryStorages = @($temporaryJson.storages | Where-Object { $_.kindName -eq "temporary" })
if ( $temporaryStorages.Count -ne 1 ) {
    throw "XLIR dynamic-key temporary receiver has no unique owned temporary storage"
}
$temporaryOrigin = $temporaryStorages[0].declaration
$temporaryOps = @($temporaryJson.instructions | Where-Object { $_.origin -eq $temporaryOrigin } | Sort-Object index)
$temporarySequence = @($temporaryOps | ForEach-Object {
    if ( $_.kindName -eq "runtime-call" ) { return "$($_.kindName)/$($_.runtimeOpName)" }
    return $_.kindName
}) -join ","
$expectedTemporarySequence = "store,load,runtime-call/xvalue-index-get-xvalue-key,runtime-call/xvalue-retain,release"
if ( $temporarySequence -ne $expectedTemporarySequence -or
     $temporaryOps[2].operands[0] -ne $temporaryOps[1].result -or
     $temporaryOps[3].operands[0] -ne $temporaryOps[2].result -or
     $temporaryOps[4].targetSymbol -ne $temporaryStorages[0].symbol ) {
    throw "XLIR dynamic-key temporary receiver has no exact store/load/borrow/retain/release ownership chain: $temporarySequence"
}

& $xl cgen $temporarySource --backend xlir --emit $temporaryCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key temporary receiver Cgen failed" }
$temporaryGenerated = [System.IO.File]::ReadAllText($temporaryCPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_index_get_xvalue_key_or_null(",
    "xrtValueRetain(",
    "xrtValueRelease("
) ) {
    if ( ! $temporaryGenerated.Contains($required) ) {
        throw "XLIR dynamic-key temporary receiver C rendering is incomplete: $required"
    }
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key index strict generated-C runtime failed" }

$setCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $setHostSource !XL6_CGEN_LINKS! -o $setExePath && $setExePath"
& $env:ComSpec /d /v:on /c $setCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key index write strict generated-C runtime failed" }

$typedDictCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $typedDictHostSource !XL6_CGEN_LINKS! -o $typedDictExePath && $typedDictExePath"
& $env:ComSpec /d /v:on /c $typedDictCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR typed dict scalar write strict generated-C runtime failed" }

$temporaryCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $temporaryHostSource !XL6_CGEN_LINKS! -o $temporaryExePath && $temporaryExePath"
& $env:ComSpec /d /v:on /c $temporaryCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic-key temporary receiver strict generated-C runtime failed" }

Write-Output "XLIR dynamic-key index gate ok: verified borrowed reads, consuming writes, typed dict scalar boxing, runtime i64/string dispatch, null sentinel, explicit temporary store/load/borrow/retain/release ownership, strict C and runtime."
exit 0
