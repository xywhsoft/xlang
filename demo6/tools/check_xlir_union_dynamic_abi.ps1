param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_union_dynamic_box_abi.xl"
$iteratorSource = Join-Path $Root "test\cases\cgen_union_var_box.xl"
$classSource = Join-Path $Root "test\cases\export_class_union_field.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$classHostSource = Join-Path $Root "test\cases\xlir_union_class_field_export_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_union_dynamic_box_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_union_dynamic_box_abi.lowered.xlir"
$cPath = Join-Path $outputDir "xlir_union_dynamic_box_abi.generated.c"
$exePath = Join-Path $outputDir "xlir_union_dynamic_box_abi.exe"
$iteratorLoweredPath = Join-Path $outputDir "xlir_union_iterator_abi.lowered.xlir"
$iteratorCPath = Join-Path $outputDir "xlir_union_iterator_abi.generated.c"
$iteratorExePath = Join-Path $outputDir "xlir_union_iterator_abi.exe"
$classTypedPath = Join-Path $outputDir "xlir_union_class_field_write.typed.json"
$classLoweredPath = Join-Path $outputDir "xlir_union_class_field_write.lowered.xlir"
$classCPath = Join-Path $outputDir "xlir_union_class_field_write.generated.c"
$classHeaderPath = Join-Path $outputDir "export_class_union_field.h"
$classExePath = Join-Path $outputDir "xlir_union_class_field_write.exe"

foreach ( $path in @($xl, $source, $iteratorSource, $classSource,
                      $hostSource, $classHostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR union dynamic ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR union dynamic Typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
if ( @($typed.nodes | Where-Object {
         $_.memberAbiName -eq 'union-field-direct-storage-write'
     }).Count -ne 1 ) {
    throw "Typed XLIR does not select the exact direct-storage union write-back ABI"
}

& $xl ir $source --lowered --format text --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR union dynamic lowering failed" }
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8)
foreach ( $operation in @("runtime=xvalue-union-box", "runtime=xvalue-union-unbox") ) {
    if ( ([regex]::Matches($lowered, [regex]::Escape($operation))).Count -lt 1 ) {
        throw "XLIR union dynamic lowering omitted $operation"
    }
}
if ( ! $lowered.Contains("payload=t") ) {
    throw "XLIR union dynamic operations have no explicit payload TypeId"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR union dynamic Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static xvalue* xlir_xvalue_union_box(const void* source, size_t size, uint64_t typeId)",
    "static void xlir_xvalue_union_unbox(const xvalue* source, uint64_t typeId, void* destination, size_t size)",
    "bytes.Size != size",
    "xlir_xvalue_type_id(source) != typeId"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR union dynamic C emission is incomplete: $required"
    }
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR union dynamic strict-C/runtime gate failed"
}

& $xl ir $iteratorSource --lowered --format text --emit $iteratorLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR union iterator lowering failed" }
$iteratorLowered = [System.IO.File]::ReadAllText(
    $iteratorLoweredPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "item=union-copy/trivial-replace",
    "runtime=xvalue-iterator-item-borrow",
    "runtime=xvalue-union-unbox"
) ) {
    if ( ! $iteratorLowered.Contains($required) ) {
        throw "XLIR union iterator omitted its explicit projection ABI: $required"
    }
}

& $xl cgen $iteratorSource --backend xlir --emit $iteratorCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR union iterator Cgen failed" }
$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$iteratorCPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$iteratorExePath`" && `"$iteratorExePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR union iterator strict-C/runtime gate failed"
}

& $xl ir $classSource --typed --format json --emit $classTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR union class-field Typed IR failed" }
$classTyped = [System.IO.File]::ReadAllText(
    $classTypedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$classMembers = @($classTyped.nodes | Where-Object { $_.kindName -eq 'member' })
if ( @($classMembers | Where-Object {
         $_.memberAbiName -eq 'class-field-union-copy'
     }).Count -ne 2 -or
     @($classMembers | Where-Object {
         $_.memberAbiName -eq 'union-field-class-field-write'
     }).Count -ne 1 -or
     @($classMembers | Where-Object {
         $_.memberAbiName -eq 'union-field'
     }).Count -ne 1 ) {
    throw "Typed XLIR does not distinguish union class-field copy, nested write-back, and read ABIs"
}

& $xl ir $classSource --lowered --format text --emit $classLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR union class-field write lowering failed" }
$classLowered = [System.IO.File]::ReadAllText(
    $classLoweredPath, [System.Text.Encoding]::UTF8)
foreach ( $operation in @(
    "runtime=xvalue-object-get-literal",
    "runtime=xvalue-union-unbox",
    "union-set",
    "runtime=xvalue-union-box",
    "runtime=xvalue-object-set-literal-new",
    "trap-if-false"
) ) {
    if ( ([regex]::Matches($classLowered, [regex]::Escape($operation))).Count -lt 1 ) {
        throw "XLIR union class-field write omitted explicit operation: $operation"
    }
}

& $xl cgen $classSource --backend xlir --emit $classCPath --emit-header $classHeaderPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR union class-field write Cgen failed" }
$classHeader = [System.IO.File]::ReadAllText(
    $classHeaderPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef union XL_Union_",
    "xvalue* xl6_box_make(int64_t value);",
    "int64_t xl6_box_pick(xvalue* box);"
) ) {
    if ( ! $classHeader.Contains($required) ) {
        throw "XLIR union class export header is incomplete: $required"
    }
}
$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$classCPath`" `"$classHostSource`" !XL6_CGEN_LINKS! -o `"$classExePath`" && `"$classExePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR union class-field write strict-C/runtime gate failed"
}

Write-Output "XLIR union dynamic ABI gate ok: exact trivial layout, stable TypeId, owned byte box, checked index/iterator/class-field unbox, explicit direct/class write-back ABI, strict C, and runtime."
exit 0
