$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot -Parent
$xl = Join-Path $root "release\xl.exe"
$source = Join-Path $root "test\cases\xlir_avl_resource_abi.xl"
$classDefaultSource = Join-Path $root "test\cases\xlir_avl_class_default_abi.xl"
$hostSource = Join-Path $root "test\cases\xlir_avl_resource_abi_host.c"
$typedJson = Join-Path $root "build\tmp\xlir_avl_resource_abi.typed.json"
$loweredJson = Join-Path $root "build\tmp\xlir_avl_resource_abi.lowered.json"
$classTypedJson = Join-Path $root "build\tmp\xlir_avl_class_default_abi.typed.json"
$classLoweredJson = Join-Path $root "build\tmp\xlir_avl_class_default_abi.lowered.json"
$generatedC = Join-Path $root "build\tmp\xlir_avl_resource_abi.c"
$generatedCSecond = Join-Path $root "build\tmp\xlir_avl_resource_abi.second.c"
$classDefaultC = Join-Path $root "build\tmp\xlir_avl_class_default_abi.c"

foreach ( $path in @($xl, $source, $classDefaultSource, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "missing XLIR AvlTree resource gate input: $path"
    }
}
New-Item -ItemType Directory -Force (Join-Path $root "build\tmp") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "build\test") | Out-Null

function Read-Json([string]$path) {
    return [System.IO.File]::ReadAllText(
        $path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Type-By-Id($module, [int]$id) {
    return @($module.types | Where-Object { $_.id -eq $id })[0]
}

function Assert-Storage-Ref($module, $operand, [int]$payloadType, [string]$label) {
    $referenceType = Type-By-Id $module $operand.type
    if ( $operand.abiName -ne "storage-ref" -or $null -eq $referenceType -or
         $referenceType.params.Count -ne 1 -or
         $referenceType.params[0] -ne $payloadType ) {
        throw "XLIR AvlTree $label lost its exact storage-ref ABI"
    }
}

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR AvlTree Typed IR failed" }
$typed = Read-Json $typedJson

$treeTypes = @($typed.types | Where-Object {
    $_.runtimeResourceAbiName -eq "avl-tree" -and $_.params.Count -eq 2
})
if ( $treeTypes.Count -ne 3 -or @($treeTypes | Where-Object {
        $_.kind -ne 6 -or $_.cAbi -ne 26 -or $_.ownershipAbi -ne 2 -or
        $_.nullAbi -ne 2 -or $_.typeValueId -eq "0x0000000000000000"
    }).Count -ne 0 ) {
    throw "XLIR AvlTree<K,V> types lost exact managed xvalue identities"
}

$resourceInitializers = @($typed.valueInitializers | Where-Object {
    $_.abiName -eq "runtime-resource-default" -and
    $_.runtimeResourceOpName -eq "avl-tree-create"
})
if ( $resourceInitializers.Count -ne 1 -or
     $resourceInitializers[0].runtimeResourceKeyType -eq 0 -or
     $resourceInitializers[0].payloadType -eq 0 -or
     $resourceInitializers[0].runtimeResourceKeyPayloadAbiName -ne "string-copy" -or
     $resourceInitializers[0].runtimeResourceKeyOrderAbiName -ne "string" -or
     $resourceInitializers[0].runtimeResourcePayloadAbiName -ne "trivial-copy" -or
     $resourceInitializers[0].errorAbiName -ne "may-raise" ) {
    throw "XLIR default AvlTree<K,V> declaration has no exact key/value/effect ABI"
}
$resourceFields = @($typed.classFields | Where-Object {
    $_.initializerRuntimeResourceOpName -eq "avl-tree-create"
})
if ( $resourceFields.Count -ne 1 -or
     $resourceFields[0].initializerAbiName -ne "runtime-resource-default" -or
     $resourceFields[0].initializerRuntimeResourceKeyPayloadAbiName -ne "string-copy" -or
     $resourceFields[0].initializerRuntimeResourceKeyOrderAbiName -ne "string" -or
     $resourceFields[0].initializerRuntimeResourcePayloadAbiName -ne "string-copy" -or
     $resourceFields[0].initializerErrorAbiName -ne "may-raise" ) {
    throw "XLIR AvlTree<K,V> class field has no exact default resource contract"
}

$expectedTypedCounts = [ordered]@{
    "avl-tree-create" = 3
    "avl-tree-set" = 8
    "avl-tree-get-into" = 4
    "avl-tree-take-into" = 1
    "avl-tree-contains" = 2
    "avl-tree-remove" = 1
    "avl-tree-first-into" = 2
    "avl-tree-last-into" = 1
    "avl-tree-lower-bound-into" = 1
    "avl-tree-upper-bound-into" = 1
    "avl-tree-trim-to" = 1
    "avl-tree-equals" = 2
    "avl-tree-merge" = 2
    "avl-tree-clear" = 1
    "avl-tree-count" = 5
    "avl-tree-keys" = 1
    "avl-tree-values" = 1
    "avl-tree-trim" = 1
    "avl-tree-clone" = 1
}
$calls = @($typed.intrinsicCalls | Where-Object {
    $_.runtimeResourceOpName -like "avl-tree-*"
})
foreach ( $entry in $expectedTypedCounts.GetEnumerator() ) {
    if ( @($calls | Where-Object {
             $_.runtimeResourceOpName -eq $entry.Key
         }).Count -ne $entry.Value ) {
        throw "XLIR Typed AvlTree operation count changed: $($entry.Key)"
    }
}
if ( $calls.Count -ne ($expectedTypedCounts.Values | Measure-Object -Sum).Sum ) {
    throw "XLIR Typed AvlTree family contains unaccounted operations"
}

$noRaiseOps = @(
    "avl-tree-take-into", "avl-tree-contains", "avl-tree-remove",
    "avl-tree-trim-to", "avl-tree-equals", "avl-tree-clear",
    "avl-tree-count", "avl-tree-trim"
)
$pairOutputOps = @("avl-tree-first-into", "avl-tree-last-into")
$boundOps = @("avl-tree-lower-bound-into", "avl-tree-upper-bound-into")
foreach ( $call in $calls ) {
    if ( $call.kindName -ne "runtime-resource" -or
         $call.admissionName -ne "verified" -or
         $call.runtimeResourceKeyType -eq 0 -or $call.payloadType -eq 0 -or
         $call.runtimeResourceKeyPayloadAbiName -eq "invalid" -or
         $call.runtimeResourceKeyOrderAbiName -eq "invalid" -or
         $call.runtimeResourcePayloadAbiName -eq "invalid" ) {
        throw "XLIR Typed AvlTree call has ambiguous generic resource metadata"
    }
    $operands = @($typed.intrinsicOperands | Select-Object -Skip $call.firstOperand |
        Select-Object -First $call.operandCount)
    if ( $call.runtimeResourceOpName -eq "avl-tree-create" ) {
        $treeType = Type-By-Id $typed $call.resultType
        if ( $call.resultAbiName -ne "owned" -or
             $call.errorAbiName -ne "may-raise" -or
             $call.receiver -ne 0 -or $operands.Count -ne 0 -or
             $null -eq $treeType -or $treeType.params.Count -ne 2 -or
             $treeType.params[0] -ne $call.runtimeResourceKeyType -or
             $treeType.params[1] -ne $call.payloadType ) {
            throw "XLIR AvlTree create ABI is not exact"
        }
        continue
    }
    $treeType = Type-By-Id $typed $call.receiverType
    if ( $null -eq $treeType -or $treeType.params.Count -ne 2 -or
         $treeType.params[0] -ne $call.runtimeResourceKeyType -or
         $treeType.params[1] -ne $call.payloadType -or
         $operands.Count -eq 0 -or $operands[0].abiName -ne "borrow" -or
         $operands[0].type -ne $call.receiverType ) {
        throw "XLIR AvlTree member has no exact receiver/key/value identity"
    }
    $expectedError = if ( $call.runtimeResourceOpName -in $noRaiseOps ) {
        "no-raise"
    } else { "may-raise" }
    if ( $call.errorAbiName -ne $expectedError ) {
        throw "XLIR AvlTree effect contract changed: $($call.runtimeResourceOpName)"
    }
    $keyAbi = if ( $call.runtimeResourceKeyPayloadAbiName -eq "trivial-copy" ) {
        "value"
    } else { "borrow" }
    $valueAbi = if ( $call.runtimeResourcePayloadAbiName -eq "trivial-copy" ) {
        "value"
    } else { "borrow" }
    switch ( $call.runtimeResourceOpName ) {
        "avl-tree-set" {
            if ( $operands.Count -ne 3 -or $operands[1].type -ne $call.runtimeResourceKeyType -or
                 $operands[1].abiName -ne $keyAbi -or
                 $operands[2].type -ne $call.payloadType -or
                 $operands[2].abiName -ne $valueAbi ) {
                throw "XLIR AvlTree set key/value ABI is not exact"
            }
        }
        { $_ -in @("avl-tree-get-into", "avl-tree-take-into") } {
            if ( $operands.Count -ne 3 -or $operands[1].type -ne $call.runtimeResourceKeyType -or
                 $operands[1].abiName -ne $keyAbi ) {
                throw "XLIR AvlTree get/take key ABI is not exact"
            }
            Assert-Storage-Ref $typed $operands[2] $call.payloadType "get/take output"
        }
        { $_ -in @("avl-tree-contains", "avl-tree-remove") } {
            if ( $operands.Count -ne 2 -or $operands[1].type -ne $call.runtimeResourceKeyType -or
                 $operands[1].abiName -ne $keyAbi ) {
                throw "XLIR AvlTree contains/remove key ABI is not exact"
            }
        }
        { $_ -in $pairOutputOps } {
            if ( $operands.Count -ne 3 ) { throw "XLIR AvlTree endpoint arity changed" }
            Assert-Storage-Ref $typed $operands[1] $call.runtimeResourceKeyType "endpoint key output"
            Assert-Storage-Ref $typed $operands[2] $call.payloadType "endpoint value output"
        }
        { $_ -in $boundOps } {
            if ( $operands.Count -ne 4 -or $operands[1].type -ne $call.runtimeResourceKeyType -or
                 $operands[1].abiName -ne $keyAbi ) {
                throw "XLIR AvlTree bound key ABI is not exact"
            }
            Assert-Storage-Ref $typed $operands[2] $call.runtimeResourceKeyType "bound key output"
            Assert-Storage-Ref $typed $operands[3] $call.payloadType "bound value output"
        }
        "avl-tree-equals" {
            if ( $operands.Count -ne 2 -or $operands[1].abiName -ne "borrow" -or
                 $operands[1].type -ne $call.receiverType ) {
                throw "XLIR AvlTree equals peer ABI is not exact"
            }
        }
        "avl-tree-merge" {
            if ( $operands.Count -notin @(2, 3) -or
                 $operands[1].abiName -ne "borrow" -or
                 $operands[1].type -ne $call.receiverType -or
                 ($operands.Count -eq 3 -and $operands[2].abiName -ne "value") ) {
                throw "XLIR AvlTree merge source/replace ABI is not exact"
            }
        }
    }
}

& $xl ir $classDefaultSource --format json --emit $classTypedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR AvlTree class-default Typed IR failed" }
$classTyped = Read-Json $classTypedJson
$classDefaults = @($classTyped.valueInitializers | Where-Object {
    $_.abiName -eq "class-default"
})
$classFields = @($classTyped.classFields | Where-Object {
    $_.initializerRuntimeResourceOpName -eq "avl-tree-create"
})
if ( $classDefaults.Count -ne 1 -or
     $classDefaults[0].errorAbiName -ne "may-raise" -or
     $classFields.Count -ne 1 -or
     $classFields[0].initializerRuntimeResourceKeyPayloadAbiName -ne "string-copy" -or
     $classFields[0].initializerRuntimeResourceKeyOrderAbiName -ne "string" -or
     $classFields[0].initializerRuntimeResourcePayloadAbiName -ne "trivial-copy" ) {
    throw "XLIR class-default initializer does not aggregate its AvlTree field contract"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR AvlTree Lowered IR failed" }
$lowered = Read-Json $loweredJson
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -like "avl-tree-*"
})
$expectedLoweredCounts = [ordered]@{}
foreach ( $entry in $expectedTypedCounts.GetEnumerator() ) {
    $expectedLoweredCounts[$entry.Key] = if (
        $entry.Key -eq "avl-tree-create"
    ) { 5 } else { $entry.Value }
}
foreach ( $entry in $expectedLoweredCounts.GetEnumerator() ) {
    $matches = @($operations | Where-Object { $_.runtimeOpName -eq $entry.Key })
    if ( $matches.Count -ne $entry.Value -or @($matches | Where-Object {
            $_.kindName -ne "runtime-call" -or $_.targetSymbol -ne 0 -or
            $_.hasImmediate -ne 0 -or $_.runtimeResourceKeyType -eq 0 -or
            $_.payloadType -eq 0 -or
            $_.runtimeResourceKeyPayloadAbiName -eq "invalid" -or
            $_.runtimeResourceKeyOrderAbiName -eq "invalid" -or
            $_.runtimeResourcePayloadAbiName -eq "invalid"
        }).Count -ne 0 ) {
        throw "XLIR Lowered AvlTree RuntimeOp ABI is incomplete: $($entry.Key)"
    }
}
$merges = @($operations | Where-Object { $_.runtimeOpName -eq "avl-tree-merge" })
if ( @($merges | Where-Object { $_.operandCount -ne 3 }).Count -ne 0 ) {
    throw "XLIR Lowered AvlTree merge did not normalize the default replace operand"
}
$definitions = @{}
foreach ( $instruction in $lowered.instructions ) {
    if ( $instruction.result -ne 0 ) {
        $definitions[[string]$instruction.result] = $instruction
    }
}
$errorSources = @()
foreach ( $test in @($lowered.instructions | Where-Object {
        $_.kindName -eq "error-test"
    }) ) {
    $definition = $definitions[[string]$test.operands[0]]
    if ( $null -ne $definition -and $definition.runtimeOpName -like "avl-tree-*" ) {
        $errorSources += $definition
    }
}
if ( $errorSources.Count -ne 27 -or @($errorSources | Where-Object {
        $_.runtimeOpName -in $noRaiseOps
    }).Count -ne 0 ) {
    throw "XLIR AvlTree may-raise RuntimeOps lost exact error-test CFG edges"
}
$treeTypeIds = @($treeTypes.id)
$captures = @($lowered.closureCaptures | Where-Object { $_.type -in $treeTypeIds })
if ( $captures.Count -ne 1 -or $captures[0].abiName -ne "retain-xvalue" ) {
    throw "XLIR AvlTree closure capture does not retain its exact xvalue handle"
}

& $xl ir $classDefaultSource --lowered --format json --emit $classLoweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR AvlTree class-default Lowered IR failed" }
$classLowered = Read-Json $classLoweredJson
$classCreates = @($classLowered.instructions | Where-Object {
    $_.runtimeOpName -eq "avl-tree-create"
})
$classErrorTests = @($classLowered.instructions | Where-Object {
    $_.kindName -eq "error-test" -and $_.operands[0] -eq $classCreates[0].result
})
$classFailureInstructions = @()
if ( $classErrorTests.Count -eq 1 ) {
    $classFailureBlock = @($classLowered.blocks | Where-Object {
        $_.id -eq $classErrorTests[0].falseTarget
    })
    if ( $classFailureBlock.Count -eq 1 ) {
        $first = [int]$classFailureBlock[0].firstInstruction
        $count = [int]$classFailureBlock[0].instructionCount
        if ( $count -gt 0 ) {
            $classFailureInstructions = @(
                $classLowered.instructions[$first..($first + $count - 1)]
            )
        }
    }
}
$classFailureReleases = @($classFailureInstructions | Where-Object {
    $_.kindName -eq "release" -and $_.origin -eq $classCreates[0].origin
})
if ( $classCreates.Count -ne 1 -or $classErrorTests.Count -ne 1 -or
     $classFailureReleases.Count -ne 1 -or
     @($classFailureInstructions | Where-Object {
         $_.kindName -eq "error-unwind-begin"
     }).Count -ne 1 -or
     @($classFailureInstructions | Where-Object {
         $_.kindName -eq "throw"
     }).Count -ne 1 ) {
    throw "XLIR AvlTree class-field create lacks error CFG and partial-object cleanup"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR AvlTree C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR AvlTree repeat C generation failed" }
& $xl cgen $classDefaultSource --backend xlir --emit $classDefaultC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR AvlTree class-default C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
$second = [System.IO.File]::ReadAllText($generatedCSecond, [System.Text.Encoding]::UTF8)
if ( $c -cne $second ) {
    throw "XLIR AvlTree generated C is not byte-for-byte deterministic"
}
foreach ( $required in @(
    "typedef struct xlir_avl_handle", "xlir_avl_handle_ops",
    "handle->tree_type_id != treeTypeId", "handle->key_type_id != keyTypeId",
    "handle->value_type_id != valueTypeId", "handle->key_order_abi != keyOrderAbi",
    "handle->key_size != keySize", "handle->value_size != valueSize",
    "xrtTypedTreeCreate", "xlir_avl_create", "xlir_avl_set",
    "xlir_avl_get_or_take", "xlir_avl_has_or_remove", "xlir_avl_endpoint",
    "xlir_avl_bound", "xlir_avl_trim", "xlir_avl_equals", "xlir_avl_merge",
    "xlir_avl_clear", "xlir_avl_count", "xlir_avl_collect", "xlir_avl_clone"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR AvlTree mechanical C ABI emission is missing: $required"
    }
}
$helperCounts = [ordered]@{
    'v[0-9]+ = xlir_avl_create\(' = 5
    'v[0-9]+ = xlir_avl_set\(' = 8
    'v[0-9]+ = xlir_avl_get_or_take\(' = 5
    'v[0-9]+ = xlir_avl_has_or_remove\(' = 3
    'v[0-9]+ = xlir_avl_endpoint\(' = 3
    'v[0-9]+ = xlir_avl_bound\(' = 2
    'v[0-9]+ = xlir_avl_trim\(' = 2
    'v[0-9]+ = xlir_avl_equals\(' = 2
    'v[0-9]+ = xlir_avl_merge\(' = 2
    '(?m)^\s+xlir_avl_clear\(v' = 1
    'v[0-9]+ = xlir_avl_count\(' = 5
    'v[0-9]+ = xlir_avl_collect\(' = 2
    'v[0-9]+ = xlir_avl_clone\(' = 1
}
foreach ( $entry in $helperCounts.GetEnumerator() ) {
    if ( ([regex]::Matches($c, $entry.Key)).Count -ne $entry.Value ) {
        throw "XLIR AvlTree helper emission count changed: $($entry.Key)"
    }
}
foreach ( $forbidden in @(
    "#include <xrt.h>", "#include <xruntime.h>", "XL6_AST_", "xl6IrNodeGet",
    "xl6IrCgenLegacy", "xl6SpecialTree", "__xl6_special_tree_dispatch"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR AvlTree Cgen leaked internal/source/legacy dependency: $forbidden"
    }
}

$xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_avl_resource_abi.c test\cases\xlir_avl_resource_abi_host.c !XL6_CGEN_LINKS! -o build\test\xlir_avl_resource_abi.exe && build\test\xlir_avl_resource_abi.exe'
& $env:ComSpec /d /v:on /c $xlirCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR AvlTree strict-C/runtime gate failed" }
$classCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_avl_class_default_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\test\xlir_avl_class_default_abi.exe && build\test\xlir_avl_class_default_abi.exe'
& $env:ComSpec /d /v:on /c $classCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR AvlTree class-default strict-C/runtime gate failed"
}

Write-Output "XLIR AvlTree resource ABI gate passed: exact key/value/order identities, normalized merge, class-field defaults and cleanup, storage refs, retained capture, deterministic declaration-only C, and strict runtime."
exit 0
