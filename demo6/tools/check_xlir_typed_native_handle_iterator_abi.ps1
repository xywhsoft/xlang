param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_typed_native_handle_iterator_abi.xl"
$support = Join-Path $Root "test\cases\cgen_class_host.c"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_typed_native_handle_iterator_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_typed_native_handle_iterator_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_typed_native_handle_iterator_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_typed_native_handle_iterator_abi.second.c"

foreach ( $path in @($xl, $source, $support, $genericHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR managed native-backed class iterator ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Read-Json($path) {
    return [System.IO.File]::ReadAllText(
        $path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}
function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR typed native-handle iterator Typed IR failed" }
$typed = Read-Json $typedJson
$loops = @($typed.nodes | Where-Object {
    $_.loopItemAbiName -eq "xvalue-retain"
})
if ( $loops.Count -ne 1 -or
     $loops[0].loopAbiName -ne "xvalue-iterator-value" -or
     $loops[0].loopKeyAbiName -ne "none" -or
     $loops[0].loopItemLifetimeAbiName -ne "owned-iteration-release" -or
     $loops[0].loopResourceAbiName -ne "xvalue-iterator-destroy" -or
     $loops[0].loopErrorAbiName -ne "no-raise" -or
     $loops[0].loopResourceType -eq 0 ) {
    throw "Typed XLIR native-backed class loop has no exact cursor, retained managed item, owned target lifetime, and cursor cleanup ABI"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR typed native-handle iterator lowering failed" }
$lowered = Read-Json $loweredJson
$nativeType = @($lowered.types | Where-Object { $_.name -eq "NativeProbe" })[0]
$retainedItems = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-iterator-item-xvalue-retain"
})
if ( $null -eq $nativeType -or $nativeType.cAbi -ne 26 -or
     $nativeType.ownershipAbi -ne 2 -or $nativeType.nullAbi -ne 2 -or
     $nativeType.nominalSymbol -eq 0 -or
     $nativeType.typeValueId -eq "0x0000000000000000" -or
     $retainedItems.Count -ne 1 -or
     $retainedItems[0].type -ne $nativeType.id -or
     $retainedItems[0].operandCount -ne 1 ) {
    throw "Lowered XLIR native-backed class loop has no exact retained nominal managed item"
}
if ( @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-item-borrow" -or
        $_.runtimeOpName -eq "xvalue-project-native-handle-borrow"
    }).Count -ne 0 ) {
    throw "Lowered XLIR native-backed class loop regressed to borrowed bare-handle projection"
}
$fieldReads = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-object-get-literal-raw-pointer"
})
if ( $fieldReads.Count -ne 2 ) {
    throw "Lowered XLIR native-backed class loop is missing exact raw handle field reads: $($fieldReads.Count)"
}
foreach ( $operation in @("xvalue-iterator-create", "xvalue-iterator-advance") ) {
    if ( @($lowered.instructions | Where-Object {
            $_.runtimeOpName -eq $operation
        }).Count -ne 1 ) {
        throw "Lowered XLIR native-handle loop is missing canonical cursor operation: $operation"
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR typed native-handle iterator C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR typed native-handle iterator repeat C generation failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR typed native-handle iterator generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_iterator_create(", "xlir_xvalue_iterator_advance(",
    "xlir_xvalue_iterator_item_xvalue_retain(",
    "xlir_xvalue_object_get_literal_raw_pointer(",
    "xlir_xvalue_iterator_destroy("
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR managed native-backed class iterator generated C is missing mechanical cursor/retain/field code: $required"
    }
}
if ( ([regex]::Matches($c,
        ' = xlir_xvalue_iterator_item_xvalue_retain\(v')).Count -ne 1 -or
     ([regex]::Matches($c,
        ' = xlir_xvalue_object_get_literal_raw_pointer\(v')).Count -ne 2 -or
     $c.Contains("xlir_xvalue_project_native_handle_borrow") -or
     $c.Contains("xrtValueGetHandle(source") ) {
    throw "XLIR managed native-backed class iterator Cgen did not preserve exact owned item and field projection ABI"
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_typed_native_handle_iterator_abi.c -include test\cases\cgen_class_host.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\xlir_typed_native_handle_iterator_abi.exe && build\test\xlir_typed_native_handle_iterator_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed native-handle iterator strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR managed native-backed class iterator ABI gate passed: explicit retained LoopItemAbi, owned per-iteration lifetime, raw handle field access, deterministic C, and strict runtime."
