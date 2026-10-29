param(
    [ValidateSet("cgen_typed_container_for_in_returned", "cgen_typed_container_box_to_var",
        "cgen_type_desc_dedupe", "cgen_typed_container_copy_assign",
        "cgen_typed_container_copy_objects_functions", "cgen_typed_container_functions",
        "cgen_typed_container_objects", "cgen_typed_container_function_field_objects")]
    [string]$Name = "cgen_typed_container_for_in_returned",
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\$Name.xl"
$outputDir = Join-Path $Root "build\tmp"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null
$prefix = Join-Path $outputDir "xlir_container_ownership_contract_$Name"
& $xl ir $source --lowered --format json --emit "$prefix.json"
if ( $LASTEXITCODE -ne 0 ) { throw "Container ownership lowering failed: $Name" }
& $xl cgen $source --emit "$prefix.c"
if ( $LASTEXITCODE -ne 0 ) { throw "Container ownership default Cgen failed: $Name" }
& $xl cgen $source --backend xlir --emit "$prefix.repeat.c"
if ( $LASTEXITCODE -ne 0 ) { throw "Container ownership explicit Cgen failed: $Name" }
$c = [System.IO.File]::ReadAllText("$prefix.c")
if ( $c -cne [System.IO.File]::ReadAllText("$prefix.repeat.c") ) {
    throw "Container ownership Cgen is not deterministic: $Name"
}
if ( $c.Contains("__xl6_user_global_") -or $c.Contains("xl6_temp_managed_") -or
     $c.Contains("<__xl_runtime/xl_runtime.h>") -or $c.Contains("({ XL6_Function") ) {
    throw "Container ownership leaked legacy storage or closure fallbacks: $Name"
}
$module = [System.IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
$types = @{}
foreach ( $type in $module.types ) { $types[[int]$type.id] = $type }

function Require-Counts([hashtable]$Expected) {
    foreach ( $entry in $Expected.GetEnumerator() ) {
        $actual = @($module.instructions | Where-Object { $_.runtimeOpName -eq $entry.Key }).Count
        if ( $actual -ne $entry.Value ) {
            throw "$Name lost $($entry.Key) coverage: $actual != $($entry.Value)"
        }
    }
}

switch ( $Name ) {
    "cgen_typed_container_for_in_returned" {
        # These are source loop locations and resource exit counts, not C-local
        # names. Nested return/throw must destroy both resources in reverse order.
        $expectedExits = @{
            18=2;25=2;32=2;39=2;46=2;47=2;59=1;65=1;76=2;91=1;101=1;112=2;
            126=1;132=1;143=2;157=1;163=1;173=2;187=1;197=1;198=1;210=2;211=2;
            226=1;239=1;246=1;252=1
        }
        $loops = @($module.nodes | Where-Object { $_.sourceKindName -eq "for" })
        if ( $loops.Count -ne $expectedExits.Count ) { throw "Returned-container loop coverage changed" }
        foreach ( $loop in $loops ) {
            $ops = @($module.instructions | Where-Object { $_.origin -eq $loop.id })
            $stores = @($ops | Where-Object {
                $_.kindName -eq "store" -and $_.type -eq $loop.loopResourceType -and
                $_.targetSymbol -ge 0xF0000000
            })
            if ( $stores.Count -ne 1 -or $loop.loopErrorAbiName -ne "no-raise" ) {
                throw "Returned-container loop has no unique owned resource: line $($loop.line)"
            }
            $symbol = $stores[0].targetSymbol
            $releases = @($ops | Where-Object {
                $_.kindName -eq "release" -and $_.targetSymbol -eq $symbol -and
                $_.type -eq $loop.loopResourceType
            })
            if ( $releases.Count -ne $expectedExits[[int]$loop.line] ) {
                throw "Returned-container exit coverage changed: line $($loop.line)"
            }
            if ( $loop.loopResourceAbiName -eq "xvalue-iterator-destroy" ) {
                $destroy = "xlir_xvalue_iterator_destroy"
                foreach ( $op in @("xvalue-iterator-create", "xvalue-iterator-advance") ) {
                    if ( @($ops | Where-Object { $_.runtimeOpName -eq $op }).Count -ne 1 ) {
                        throw "Returned-container snapshot cursor lost $op"
                    }
                }
            } elseif ( $loop.loopResourceAbiName -in @(
                "xvalue-owned-iterator-release", "xvalue-shallow-snapshot-release") ) {
                $destroy = "xrtValueRelease"
            } else { throw "Returned-container has an ambiguous resource lifetime" }
            $pattern = $destroy + '\(s' + $symbol + '\);\s*s' + $symbol + ' = NULL;'
            if ( [regex]::Matches($c, $pattern).Count -ne $releases.Count ) {
                throw "Generated C does not preserve every verified resource exit: line $($loop.line)"
            }
        }
        Require-Counts @{
            "xvalue-array-at-i64-integer"=8;"xvalue-iterator-create"=19;
            "xvalue-iterator-key-string-copy"=11;"xvalue-iterator-item-string-copy"=4;
            "xvalue-object-get-literal-closure"=3;"xvalue-object-set-literal-closure"=1
        }
        foreach ( $collection in @("array", "list", "set", "dict") ) {
            if ( @($module.functions | Where-Object {
                $types[[int]$_.returnType].collectionAbiName -eq $collection
            }).Count -eq 0 ) { throw "Returned $collection lost its concrete return TypeId" }
        }
    }
    "cgen_typed_container_box_to_var" {
        Require-Counts @{"xvalue-type-bind"=4;"xvalue-type-id"=4;"xvalue-retain"=7}
        foreach ( $collection in @("array", "list", "set", "dict") ) {
            $type = @($module.types | Where-Object { $_.collectionAbiName -eq $collection })
            if ( $type.Count -ne 1 -or $type[0].cAbi -ne 26 -or $type[0].ownershipAbi -ne 2 ) {
                throw "Container erasure lost its managed shared-handle ABI: $collection"
            }
        }
    }
    "cgen_type_desc_dedupe" {
        $array = @($module.types | Where-Object { $_.collectionAbiName -eq "array" })
        $binds = @($module.instructions | Where-Object { $_.runtimeOpName -eq "xvalue-type-bind" })
        if ( $array.Count -ne 1 -or $array[0].typeValueId -eq "0x0000000000000000" -or
             $binds.Count -ne 2 -or @($binds | Where-Object {
                 $_.runtimeIdentityType -ne $array[0].id -or $_.type -ne $array[0].id
             }).Count -ne 0 ) { throw "Equivalent containers lost their one canonical runtime TypeId" }
        Require-Counts @{"xvalue-type-id"=2;"xvalue-retain"=2}
    }
    "cgen_typed_container_copy_assign" {
        Require-Counts @{
            "xvalue-retain"=8;"xvalue-clone"=1;"xvalue-deep-clone"=1;
            "xvalue-count"=4;"xvalue-contains"=6;"xvalue-set-has"=6;
            "xvalue-array-set-i64-new"=3;"xvalue-int-map-set-i64-new"=6
        }
    }
    "cgen_typed_container_copy_objects_functions" {
        Require-Counts @{
            "xvalue-retain"=47;"xvalue-project-closure-copy"=3;
            "xvalue-class-identity-bind"=6;"xvalue-array-set-i64-new"=5;
            "xvalue-object-set-literal-closure"=2;"xvalue-object-get-literal-closure"=4
        }
    }
    "cgen_typed_container_functions" {
        Require-Counts @{
            "xvalue-callable-from-closure"=5;"xvalue-project-closure-copy"=8;
            "xvalue-iterator-create"=4;"xvalue-iterator-item-borrow"=4;
            "xvalue-iterator-key-string-copy"=1;"xvalue-dynamic-call"=1
        }
    }
    "cgen_typed_container_objects" {
        Require-Counts @{
            "xvalue-class-identity-bind"=9;"xvalue-raw-pointer-borrow"=3;
            "xvalue-iterator-item-xvalue-retain"=2;"xvalue-set-add-new"=8
        }
    }
    "cgen_typed_container_function_field_objects" {
        Require-Counts @{
            "xvalue-class-identity-bind"=8;"xvalue-iterator-item-xvalue-retain"=3;
            "xvalue-object-set-literal-closure"=6;"xvalue-object-get-literal-closure"=12
        }
    }
}

# Closure fields/projections carry a concrete function TypeId, never a var-call
# fallback. Managed loop projections must have storage and explicit destruction.
foreach ( $operation in @($module.instructions | Where-Object {
    $_.runtimeOpName -in @("xvalue-object-set-literal-closure", "xvalue-object-get-literal-closure",
        "xvalue-project-closure-copy", "xvalue-callable-from-closure")
}) ) {
    $signature = $types[[int]$operation.calleeType]
    if ( $signature.kind -ne 4 -or $signature.cAbi -ne 31 -or $signature.ownershipAbi -ne 2 ) {
        throw "Callable container operation lost its exact signature: $Name"
    }
}
foreach ( $projection in @($module.instructions | Where-Object {
    $_.runtimeOpName -in @("xvalue-iterator-key-string-copy", "xvalue-iterator-item-string-copy",
        "xvalue-iterator-item-xvalue-retain", "xvalue-project-closure-copy")
}) ) {
    $stores = @($module.instructions | Where-Object {
        $_.kindName -eq "store" -and $_.operandCount -eq 1 -and
        $_.operands[0] -eq $projection.result -and $_.type -eq $projection.type
    })
    if ( $stores.Count -ne 1 ) { throw "Managed container projection has no unique owner: $Name" }
    $storage = @($module.storages | Where-Object { $_.symbol -eq $stores[0].targetSymbol })
    if ( $storage.Count -ne 1 -or ($storage[0].kindName -ne "global" -and
         @($module.instructions | Where-Object {
             $_.kindName -eq "release" -and $_.targetSymbol -eq $storage[0].symbol -and
             $_.type -eq $projection.type
         }).Count -eq 0) ) { throw "Managed container projection lost explicit cleanup: $Name" }
}
foreach ( $layout in $module.classLayouts ) {
    if ( $layout.hashAbiName -ne "runtime-reference" -or $layout.equalityAbiName -ne "runtime-reference" ) {
        throw "Shared class handle accidentally acquired payload-copy identity: $Name"
    }
}
Write-Output "XLIR container ownership contract ok: $Name"
exit 0
