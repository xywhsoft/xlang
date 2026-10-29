param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_typed_native_handle_index_abi.xl"
$support = Join-Path $Root "test\cases\cgen_class_host.c"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$loweredJson = Join-Path $outputDir "xlir_typed_native_handle_index_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_typed_native_handle_index_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_typed_native_handle_index_abi.second.c"

foreach ( $path in @($xl, $source, $support, $genericHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR managed native-backed class index ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed native-handle index lowering failed"
}
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$sourceOps = @(
    "xvalue-array-at-i64",
    "xvalue-int-map-at-i64",
    "xvalue-object-get-literal"
)
$seenSourceOps = @{}
$nativeType = @($lowered.types | Where-Object { $_.name -eq "NativeProbe" })[0]
if ( $null -eq $nativeType -or $nativeType.cAbi -ne 26 -or
     $nativeType.ownershipAbi -ne 2 -or $nativeType.nullAbi -ne 2 -or
     $nativeType.nominalSymbol -eq 0 -or
     $nativeType.typeValueId -eq "0x0000000000000000" ) {
    throw "XLIR native-backed class is not a nominal managed xvalue owner"
}
$lookups = @($lowered.instructions | Where-Object {
    $sourceOps -contains $_.runtimeOpName -and $_.type -eq $nativeType.id
})
if ( $lookups.Count -ne 3 ) {
    throw "XLIR managed native-backed class lookup count is incomplete: $($lookups.Count)"
}
foreach ( $lookup in $lookups ) {
    $retains = @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-retain" -and
        $_.type -eq $nativeType.id -and
        $_.operandCount -eq 1 -and $_.operands[0] -eq $lookup.result
    })
    if ( $lookup.type -ne $nativeType.id -or $retains.Count -ne 1 ) {
        throw "XLIR managed native-backed class lookup does not retain the exact nominal object"
    }
    $seenSourceOps[$lookup.runtimeOpName] = $true
}
foreach ( $sourceOp in $sourceOps ) {
    if ( ! $seenSourceOps.ContainsKey($sourceOp) ) {
        throw "XLIR typed native-handle index is missing source lookup coverage: $sourceOp"
    }
}
$fieldReads = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-object-get-literal-raw-pointer"
})
if ( $fieldReads.Count -ne 3 ) {
    throw "XLIR managed native-backed class index is missing exact raw handle field reads: $($fieldReads.Count)"
}
if ( @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-project-native-handle-borrow"
    }).Count -ne 0 ) {
    throw "XLIR managed native-backed class index regressed to a bare native-handle projection"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed native-handle index C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed native-handle index repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR typed native-handle index generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "extern xvalue* xrtValueRetain(const xvalue*);",
    "static void* xlir_xvalue_object_get_literal_raw_pointer(const xvalue* object, xlir_string_view key)",
    " = xrtValueRetain(v",
    " = xlir_xvalue_object_get_literal_raw_pointer(v"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR managed native-backed class index generated C is missing mechanical retain/field access: $required"
    }
}
if ( ([regex]::Matches($c,
        ' = xlir_xvalue_object_get_literal_raw_pointer\(v')).Count -ne 3 ) {
    throw "XLIR managed native-backed class handle-field access is not use-directed and exact"
}
foreach ( $forbidden in @(
    "xlir_xvalue_project_native_handle_borrow",
    "xrtValueGetPointer(source, &handle)",
    "xrtValueGetHandle(source",
    "xlir_xvalue_project_closure_try_copy",
    "xlir_xvalue_project_nullable_string_copy",
    "xl6_index_native",
    "xl6_array_native_get"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR typed native-handle index Cgen emitted an unrelated or ownership-ambiguous helper: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_typed_native_handle_index_abi.c -include test\cases\cgen_class_host.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\xlir_typed_native_handle_index_abi.exe && build\test\xlir_typed_native_handle_index_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed native-handle index strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR managed native-backed class index ABI gate passed: exact array/list/dict nominal object retention, reference identity, raw handle field access, deterministic C, and strict runtime."
