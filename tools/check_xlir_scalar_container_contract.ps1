param(
    [ValidateSet("cgen_primitive_widths", "cgen_containers", "cgen_container_ops",
        "cgen_dynamic_container_reference", "cgen_static_typed_containers")]
    [string]$Name = "cgen_primitive_widths",
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\$Name.xl"
$outputDir = Join-Path $Root "build\tmp"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null
$jsonPath = Join-Path $outputDir "xlir_scalar_container_contract_$Name.json"
$cPath = Join-Path $outputDir "xlir_scalar_container_contract_$Name.c"
& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "Scalar/container contract lowering failed: $Name" }
& $xl cgen $source --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "Scalar/container contract Cgen failed: $Name" }
$module = [System.IO.File]::ReadAllText($jsonPath) | ConvertFrom-Json
$c = [System.IO.File]::ReadAllText($cPath)
if ( $c.Contains("__xl6_user_global_") -or $c.Contains("xl6_temp_managed_release") ) {
    throw "Scalar/container contract leaked a legacy emitter representation: $Name"
}

function Require-Operation([string]$Operation, [int]$Count = -1) {
    $ops = @($module.instructions | Where-Object {
        $_.kindName -eq "runtime-call" -and $_.runtimeOpName -eq $Operation
    })
    if ( $ops.Count -eq 0 -or ($Count -ge 0 -and $ops.Count -ne $Count) ) {
        throw "$Name lost its explicit $Operation contract (actual=$($ops.Count), expected=$Count)"
    }
    return $ops
}

if ( $Name -eq "cgen_primitive_widths" ) {
    # Language integer storage uses normalized i64, not foreign C integer
    # widths. Width semantics are an independent verified integerTarget domain.
    $globals = @($module.storages | Where-Object { $_.kindName -eq "global" })
    $expected = @(
        @("int8", "i8", "int64_t"), @("uint8", "u8", "int64_t"),
        @("int16", "i16", "int64_t"), @("uint16", "u16", "int64_t"),
        @("int32", "i32", "int64_t"), @("uint32", "u32", "int64_t"),
        @("int", "i64", "int64_t"), @("uint64", "u64", "uint64_t"),
        @("float32", "none", "float"), @("float", "none", "double")
    )
    if ( $globals.Count -ne $expected.Count ) { throw "Primitive storage count changed" }
    foreach ( $spec in $expected ) {
        $type = @($module.types | Where-Object { $_.name -eq $spec[0] })
        if ( $type.Count -ne 1 -or $type[0].integerTargetName -ne $spec[1] -or
             $type[0].ownershipAbi -ne 1 ) {
            throw "Primitive type lost its exact normalization domain: $($spec[0])"
        }
        $storage = @($globals | Where-Object { $_.type -eq $type[0].id })
        if ( $storage.Count -ne 1 -or
             ! $c.Contains("static $($spec[2]) s$($storage[0].symbol);") ) {
            throw "Primitive type lost its symbol-based physical storage: $($spec[0])"
        }
    }
    $wrapping = @(Require-Operation "integer-convert-wrapping" 7)
    foreach ( $domain in @("i8", "u8", "i16", "u16", "i32", "u32", "i64") ) {
        if ( @($wrapping | Where-Object { $_.integerTargetName -eq $domain }).Count -ne 1 ) {
            throw "Primitive fixture lost its explicit $domain normalization"
        }
    }
} elseif ( $Name -eq "cgen_containers" ) {
    foreach ( $entry in @(
        @("xvalue-array-new", 4), @("xvalue-array-append-new", 7),
        @("xvalue-int-map-new", 4), @("xvalue-int-map-set-i64-new", 5),
        @("xvalue-set-new", 4), @("xvalue-set-add-new", 7),
        @("xvalue-object-new", 6), @("xvalue-object-set-literal-new", 4),
        @("xvalue-iterator-item-string-copy", 2),
        @("xvalue-iterator-key-string-copy", 4), @("xvalue-type-bind", 14)
    ) ) { [void](Require-Operation $entry[0] $entry[1]) }
} elseif ( $Name -eq "cgen_container_ops" ) {
    foreach ( $operation in @(
        "xvalue-array-concat", "xvalue-array-remove-value",
        "xvalue-list-merge-right", "xvalue-list-without-i64-key",
        "xvalue-dict-merge-right", "xvalue-dict-without-string-key",
        "xvalue-set-union", "xvalue-set-intersection", "xvalue-set-difference",
        "xvalue-set-symmetric-difference"
    ) ) { [void](Require-Operation $operation 2) }
    foreach ( $operation in @("xvalue-set-subset", "xvalue-set-superset",
        "xvalue-set-proper-subset", "xvalue-set-proper-superset") ) {
        [void](Require-Operation $operation 1)
    }
    if ( @($module.instructions | Where-Object {
        $_.runtimeOpName -in @("xvalue-binary-add", "xvalue-binary-sub")
    }).Count -ne 0 ) { throw "Static container algebra fell back to dynamic arithmetic" }
} elseif ( $Name -eq "cgen_dynamic_container_reference" ) {
    foreach ( $operation in @("xvalue-retain", "xvalue-index-get-xvalue-key",
        "xvalue-index-set-xvalue-key-new", "xvalue-dynamic-field-get-literal",
        "xvalue-dynamic-field-set-literal-new") ) {
        [void](Require-Operation $operation)
    }
    if ( @($module.instructions | Where-Object {
        $_.runtimeOpName -in @("xvalue-clone", "xvalue-deep-clone")
    }).Count -ne 0 ) { throw "Reference assignment unexpectedly clones a dynamic container" }
} else {
    foreach ( $entry in @(
        @("xvalue-array-at-i64-integer", 3),
        @("xvalue-int-map-at-i64-string-copy", 1), @("xvalue-dict-get-literal-i64", 1),
        @("xvalue-array-at-i64-bool", 2), @("xvalue-int-map-at-i64-bool", 2),
        @("xvalue-dict-get-literal-bool", 2), @("xvalue-iterator-item-bool", 4),
        @("xvalue-iterator-key-string-copy", 5), @("xvalue-iterator-create", 10)
    ) ) { [void](Require-Operation $entry[0] $entry[1]) }
    foreach ( $projection in @($module.instructions | Where-Object {
        $_.runtimeOpName -in @("xvalue-array-at-i64-bool", "xvalue-int-map-at-i64-bool",
            "xvalue-dict-get-literal-bool", "xvalue-iterator-item-bool")
    }) ) {
        $type = @($module.types | Where-Object { $_.id -eq $projection.type })
        if ( $type.Count -ne 1 -or $type[0].name -ne "bool" -or $type[0].cAbi -ne 2 ) {
            throw "Typed bool projection lost its canonical language bool width"
        }
    }
}

# Every copied iterator key/item becomes an explicit owner and has a matching
# storage release. No C-local variable spelling or private temp-scope is used.
foreach ( $projection in @($module.instructions | Where-Object {
    $_.runtimeOpName -in @("xvalue-iterator-key-string-copy", "xvalue-iterator-item-string-copy")
}) ) {
    $stores = @($module.instructions | Where-Object {
        $_.kindName -eq "store" -and $_.operandCount -eq 1 -and
        $_.operands[0] -eq $projection.result
    })
    if ( $stores.Count -ne 1 -or @($module.instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $stores[0].targetSymbol -and
        $_.type -eq $projection.type
    }).Count -eq 0 ) { throw "Copied iterator key/item has no explicit owner release" }
}

Write-Output "XLIR scalar/container contract ok: $Name"
exit 0
