param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_typed_container_index_assign_objects_functions.xl"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$support = Join-Path $Root "test\cases\cgen_class_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_typed_container_index_assign_objects_functions.typed.json"
$loweredJson = Join-Path $outputDir "xlir_typed_container_index_assign_objects_functions.lowered.json"
$generatedC = Join-Path $outputDir "xlir_typed_container_index_assign_objects_functions.c"
$generatedCSecond = Join-Path $outputDir "xlir_typed_container_index_assign_objects_functions.second.c"
$legacyC = Join-Path $outputDir "xlir_typed_container_index_assign_objects_functions.legacy.c"

foreach ( $path in @($xl, $source, $genericHost, $support) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR typed nominal index-assignment ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

function Assert-Distribution($items, [hashtable]$expected,
                             [string]$property, [string]$label) {
    $groups = @($items | Group-Object -Property $property)
    if ( $groups.Count -ne $expected.Count ) {
        throw "$label identity count changed: $($groups.Count) != $($expected.Count)"
    }
    foreach ( $entry in $expected.GetEnumerator() ) {
        $group = @($groups | Where-Object { $_.Name -eq $entry.Key })
        if ( $group.Count -ne 1 -or $group[0].Count -ne $entry.Value ) {
            $count = if ( $group.Count -eq 1 ) { $group[0].Count } else { 0 }
            throw "$label distribution changed for $($entry.Key): $count != $($entry.Value)"
        }
    }
}

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed nominal index-assignment Typed IR construction failed"
}
& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed nominal index-assignment lowering failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$writes = @($typed.nodes | Where-Object {
    $_.kindName -eq "index" -and $_.indexWriteAbiName -ne "none"
})
Assert-Distribution $writes @{
    "xvalue-array-i64-set-new" = 3
    "xvalue-list-i64-set-new" = 4
    "xvalue-object-literal-set-new" = 3
    "xvalue-key-dispatch-set-new" = 4
} "indexWriteAbiName" "Typed XLIR nominal IndexWriteAbi"

$writeLines = @(37, 38, 39, 60, 61, 62, 82, 83, 84, 85, 108, 109, 110, 111)
if ( @($writes | Where-Object { $_.line -notin $writeLines }).Count -ne 0 -or
     @($writeLines | Where-Object {
        $line = $_
        @($writes | Where-Object { $_.line -eq $line }).Count -ne 1
     }).Count -ne 0 ) {
    throw "Typed XLIR nominal IndexWriteAbi is not attached to the exact source writes"
}

$callableWriteLines = @(108, 109, 110, 111)
$writeOrigins = @($typed.nodes | Where-Object {
    $_.kindName -eq "assign" -and $_.line -in $writeLines
} | ForEach-Object { $_.id })
if ( $writeOrigins.Count -ne $writeLines.Count ) {
    throw "Typed XLIR nominal index writes have no unique assignment origins"
}
$callableAssignments = @($typed.nodes | Where-Object {
    $_.kindName -eq "assign" -and $_.line -in $callableWriteLines
})
if ( $callableAssignments.Count -ne 4 ) {
    throw "Typed XLIR callable index-assignment effect coverage is incomplete"
}
foreach ( $assignment in $callableAssignments ) {
    if ( $assignment.implicitConversionErrorAbiName -ne "may-raise" ) {
        throw "Typed XLIR callable index assignment lost its checked boxing effect"
    }
    $exit = @($typed.exitPlans | Where-Object {
        $_.origin -eq $assignment.id -and $_.kind -eq 4
    })
    if ( $exit.Count -ne 1 ) {
        throw "Typed XLIR callable index assignment has no unique THROW ExitPlan"
    }
}

$setOps = @($lowered.instructions | Where-Object {
    $_.origin -in $writeOrigins -and $_.sourceLine -in $writeLines -and
    $_.runtimeOpName -in @(
        "xvalue-array-set-i64-new",
        "xvalue-int-map-set-i64-new",
        "xvalue-object-set-literal-new",
        "xvalue-index-set-xvalue-key-new")
})
Assert-Distribution $setOps @{
    "xvalue-array-set-i64-new" = 3
    "xvalue-int-map-set-i64-new" = 4
    "xvalue-object-set-literal-new" = 3
    "xvalue-index-set-xvalue-key-new" = 4
} "runtimeOpName" "Lowered XLIR consuming nominal index write"
foreach ( $operation in $setOps ) {
    $literalShape = $operation.runtimeOpName -eq
        "xvalue-object-set-literal-new"
    $expectedOperandCount = if ( $literalShape ) { 2 } else { 3 }
    $expectedImmediate = if ( $literalShape ) { 1 } else { 0 }
    if ( $operation.kindName -ne "runtime-call" -or
         $operation.operandCount -ne $expectedOperandCount -or
         $operation.hasImmediate -ne $expectedImmediate -or
         $operation.result -eq 0 ) {
        throw "Lowered XLIR nominal index write has no exact key/item/result ABI"
    }
    $branches = @($lowered.instructions | Where-Object {
        $_.kindName -eq "branch" -and
        $_.origin -eq $operation.origin -and
        $_.operandCount -eq 1 -and $_.operands[0] -eq $operation.result -and
        $_.trueTarget -ne 0 -and $_.falseTarget -ne 0 -and
        $_.trueTarget -ne $_.falseTarget
    })
    $traps = @($lowered.instructions | Where-Object {
        $_.kindName -eq "trap-if-false" -and
        $_.origin -eq $operation.origin -and
        $_.operandCount -eq 1 -and $_.operands[0] -eq $operation.result
    })
    if ( $branches.Count -ne 1 -or $traps.Count -ne 0 ) {
        throw "Lowered XLIR nominal index write result has no unique recoverable failure branch"
    }
    $failureBlock = @($lowered.blocks | Where-Object {
        $_.id -eq $branches[0].falseTarget
    })
    if ( $failureBlock.Count -ne 1 ) {
        throw "Lowered XLIR nominal index write failure branch has no basic block"
    }
    $failureFirst = $failureBlock[0].firstInstruction
    $failureEnd = $failureFirst + $failureBlock[0].instructionCount
    $failureInstructions = @($lowered.instructions | Where-Object {
        $_.index -ge $failureFirst -and $_.index -lt $failureEnd
    })
    $message = @($failureInstructions | Where-Object {
        $_.kindName -eq "runtime-call" -and $_.runtimeOpName -eq "string-dup" -and
        $_.origin -eq $operation.origin -and $_.result -ne 0
    })
    $raise = @($failureInstructions | Where-Object {
        $_.kindName -eq "error-raise" -and $_.origin -eq $operation.origin -and
        $_.operandCount -eq 1 -and $message.Count -eq 1 -and
        $_.operands[0] -eq $message[0].result
    })
    $throw = @($failureInstructions | Where-Object {
        $_.kindName -eq "throw" -and $_.origin -eq $operation.origin
    })
    if ( $message.Count -ne 1 -or $raise.Count -ne 1 -or $throw.Count -ne 1 ) {
        throw "Lowered XLIR nominal index write failure is not an owned RangeError/THROW edge"
    }
}

$pair = @($typed.types | Where-Object {
    $_.kind -eq 2 -and $_.name -eq "Pair" -and $_.cAbi -eq 26 -and
    $_.ownershipAbi -eq 2 -and $_.typeValueId -ne "0x0000000000000000"
})
$payload = @($typed.types | Where-Object {
    $_.kind -eq 3 -and $_.name -eq "Payload" -and $_.cAbi -eq 38 -and
    $_.typeValueId -ne "0x0000000000000000"
})
$native = @($typed.types | Where-Object {
    $_.kind -eq 2 -and $_.name -eq "Native" -and $_.cAbi -eq 26 -and
    $_.ownershipAbi -eq 2 -and $_.nominalSymbol -ne 0 -and
    $_.typeValueId -ne "0x0000000000000000"
})
$callable = @($typed.types | Where-Object {
    $_.kind -eq 4 -and $_.cAbi -eq 31 -and $_.ownershipAbi -eq 2 -and
    $_.callableErasureAbiName -eq "fixed" -and $_.params.Count -eq 2
})
if ( $pair.Count -ne 1 -or $payload.Count -ne 1 -or
     $native.Count -ne 1 -or $callable.Count -ne 1 ) {
    throw "XLIR typed nominal element identities are not unique"
}

$pairRetains = @($lowered.instructions | Where-Object {
    $_.sourceLine -in @(37, 38, 39) -and $_.runtimeOpName -eq "xvalue-retain" -and
    $_.type -eq $pair[0].id
})
$unionBoxes = @($lowered.instructions | Where-Object {
    $_.sourceLine -in @(60, 61, 62) -and
    $_.runtimeOpName -eq "xvalue-union-box"
})
$nativeRetains = @($lowered.instructions | Where-Object {
    $_.sourceLine -in @(82, 83, 84, 85) -and
    $_.runtimeOpName -eq "xvalue-retain" -and
    $_.type -eq $native[0].id
})
$callableBoxes = @($lowered.instructions | Where-Object {
    $_.sourceLine -in $callableWriteLines -and
    $_.runtimeOpName -eq "xvalue-callable-from-closure"
})
if ( $pairRetains.Count -ne 3 -or $unionBoxes.Count -ne 3 -or
     $nativeRetains.Count -ne 4 -or $callableBoxes.Count -ne 4 ) {
    throw "XLIR nominal index write boxing coverage is incomplete"
}
foreach ( $box in $unionBoxes ) {
    if ( $box.payloadType -ne $payload[0].id -or $box.operandCount -ne 1 ) {
        throw "XLIR union write box lost its exact payload layout"
    }
}
foreach ( $retain in $nativeRetains ) {
    if ( $retain.type -ne $native[0].id -or
         $retain.operandCount -ne 1 ) {
        throw "XLIR native managed-object write lost its stable nominal TypeId or copy ABI"
    }
}
foreach ( $box in $callableBoxes ) {
    if ( $box.calleeType -ne $callable[0].id -or
         $box.callableErasureAbiName -ne "fixed" -or
         $box.operandCount -ne 1 ) {
        throw "XLIR callable write box lost its exact signature ABI"
    }
    $check = @($lowered.instructions | Where-Object {
        $_.kindName -eq "error-test" -and $_.origin -eq $box.origin -and
        $_.operandCount -eq 1 -and $_.operands[0] -eq $box.result
    })
    if ( $check.Count -ne 1 ) {
        throw "XLIR callable write box is not followed by one checked error edge"
    }
    $conversionFailureBlock = @($lowered.blocks | Where-Object {
        $_.id -eq $check[0].falseTarget
    })
    if ( $conversionFailureBlock.Count -ne 1 ) {
        throw "XLIR callable write box error edge has no failure basic block"
    }
    $conversionFailureFirst = $conversionFailureBlock[0].firstInstruction
    $conversionFailureEnd = $conversionFailureFirst +
        $conversionFailureBlock[0].instructionCount
    $conversionThrows = @($lowered.instructions | Where-Object {
        $_.index -ge $conversionFailureFirst -and
        $_.index -lt $conversionFailureEnd -and
        $_.kindName -eq "throw" -and $_.origin -eq $box.origin
    })
    if ( $conversionThrows.Count -ne 1 ) {
        throw "XLIR callable write box failure block has no unique THROW"
    }
}

$pairProjection = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-xvalue-retain" -and
    $_.payloadType -eq $pair[0].id
})
$unionProjections = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-union-unbox" -and
    $_.sourceLine -in @(64, 65, 66)
})
$nativeProjections = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-xvalue-retain" -and
    $_.payloadType -eq $native[0].id
})
$nativeDirectRetains = @($lowered.instructions | Where-Object {
    $_.sourceLine -in @(87, 88, 89) -and
    $_.runtimeOpName -eq "xvalue-retain" -and
    $_.type -eq $native[0].id
})
$closureProjections = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-closure-copy"
})
if ( $pairProjection.Count -ne 1 -or $unionProjections.Count -ne 3 -or
     $nativeDirectRetains.Count -ne 3 -or $nativeProjections.Count -ne 1 -or
     $closureProjections.Count -ne 4 ) {
    throw "XLIR nominal index read projection coverage is incomplete"
}
if ( $pairProjection[0].type -ne $pair[0].id -or
     $pairProjection[0].payloadType -ne $pair[0].id -or
     $pairProjection[0].runtimeIdentityType -ne $pair[0].id -or
     $pairProjection[0].operandCount -ne 1 ) {
    throw "XLIR non-null Pair projection has no exact static and runtime identity"
}
foreach ( $projection in $unionProjections ) {
    if ( $projection.payloadType -ne $payload[0].id -or
         $projection.operandCount -ne 1 ) {
        throw "XLIR union read projection lost its exact payload layout"
    }
}
foreach ( $projection in $nativeProjections ) {
    if ( $projection.type -ne $native[0].id -or
         $projection.runtimeIdentityType -ne $native[0].id -or
         $projection.operandCount -ne 1 ) {
        throw "XLIR native read projection lost its stable nominal TypeId"
    }
}
foreach ( $retain in $nativeDirectRetains ) {
    if ( $retain.type -ne $native[0].id -or $retain.operandCount -ne 1 ) {
        throw "XLIR direct native managed-object read lost its nominal copy ABI"
    }
}
foreach ( $projection in $closureProjections ) {
    if ( $projection.type -ne $callable[0].id -or
         $projection.calleeType -ne $callable[0].id -or
         $projection.operandCount -ne 1 ) {
        throw "XLIR callable read projection lost its exact signature"
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed nominal index-assignment C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed nominal index-assignment repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR typed nominal index-assignment generated C is not deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static xvalue* xlir_xvalue_project_retain(",
    "source == NULL || xrtValueType(source) == 0",
    "xlir_xvalue_type_id(source) != exactTypeId",
    " = xlir_xvalue_project_retain(v",
    "xlir_xvalue_union_box(", "xlir_xvalue_union_unbox(",
    "xlir_closure_to_xvalue_scalar(",
    "xlir_xvalue_project_closure_try_copy(",
    "xlir_xvalue_index_set_xvalue_key_new("
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR nominal index-assignment C is missing mechanical IR emission: $required"
    }
}
foreach ( $forbidden in @(
    "indexWriteAbi", "collectionAbi", "lvalueKind", "sourceKind",
    "xl6_pair_index_assign", "xl6_union_index_assign",
    "xl6_native_index_assign", "xl6_function_index_assign",
    "xl6_infer_index_item_type"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR Cgen reconstructed typed index semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_typed_container_index_assign_objects_functions.c test\cases\xlir_generated_module_diagnostic_host.c test\cases\cgen_class_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_typed_container_index_assign_objects_functions.exe && build\test\xlir_typed_container_index_assign_objects_functions.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed nominal index-assignment strict-C/runtime gate failed"
    }

    & $xl cgen $source --backend legacy --emit $legacyC
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed nominal index-assignment legacy C generation failed"
    }
    $legacyCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror build\tmp\xlir_typed_container_index_assign_objects_functions.legacy.c test\cases\cgen_class_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_typed_container_index_assign_objects_functions.legacy.exe && build\test\xlir_typed_container_index_assign_objects_functions.legacy.exe'
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed nominal index-assignment legacy differential gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR typed nominal index-assignment ABI gate passed: exact Pair/union/native-managed/callable write boxing, dynamic-key mutation, checked callable effects, non-null read projections, deterministic strict C, runtime assertions, and legacy differential semantics."
