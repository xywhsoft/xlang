param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_index_write_abi.xl"
$genericSource = Join-Path $Root "test\cases\xlir_index_write_generic_abi.xl"
$hostPath = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$irSourcePath = Join-Path $Root "src\ir\xl6_ir.c"
$lowerSourcePath = Join-Path $Root "src\ir\xl6_ir_lower.c"
$cgenSourcePath = Join-Path $Root "src\cgen\xl6_ir_cgen.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_index_write_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_index_write_abi.lowered.json"
$cPath = Join-Path $outputDir "xlir_index_write_abi.c"
$exePath = Join-Path $outputDir "xlir_index_write_abi_host.exe"
$genericTypedPath = Join-Path $outputDir "xlir_index_write_generic_abi.typed.json"
$genericLoweredPath = Join-Path $outputDir "xlir_index_write_generic_abi.lowered.json"
$genericCPath = Join-Path $outputDir "xlir_index_write_generic_abi.c"
$genericExePath = Join-Path $outputDir "xlir_index_write_generic_abi_host.exe"

foreach ( $path in @($xl, $source, $genericSource, $hostPath,
                      $irSourcePath, $lowerSourcePath, $cgenSourcePath) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR index write ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-XlirDump([string]$sourcePath, [string]$outputPath, [bool]$lowered) {
    if ( $lowered ) {
        & $xl ir $sourcePath --lowered --format json --emit $outputPath
    } else {
        & $xl ir $sourcePath --format json --emit $outputPath
    }
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR index write JSON dump failed: $sourcePath"
    }
    return [System.IO.File]::ReadAllText(
        $outputPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Assert-ExactNames($items, [hashtable]$expected, [string]$property, [string]$label) {
    $actual = @($items | Group-Object -Property $property)
    if ( $actual.Count -ne $expected.Count ) {
        throw "$label identity count changed: $($actual.Count) != $($expected.Count)"
    }
    foreach ( $entry in $expected.GetEnumerator() ) {
        $group = @($actual | Where-Object { $_.Name -eq $entry.Key })
        if ( $group.Count -ne 1 -or $group[0].Count -ne $entry.Value ) {
            throw "$label distribution changed for $($entry.Key)"
        }
    }
}

$typed = Invoke-XlirDump $source $typedPath $false
$typedWrites = @($typed.nodes | Where-Object { $_.indexWriteAbiName -ne "none" })
Assert-ExactNames $typedWrites @{
    "xvalue-array-i64-set-new" = 5
    "xvalue-list-i64-set-new" = 7
    "xvalue-object-literal-set-new" = 1
    "xvalue-object-string-set-new" = 1
    "xvalue-key-dispatch-set-new" = 1
} "indexWriteAbiName" "Typed XLIR IndexWriteAbi"
if ( @($typed.nodes | Where-Object {
        $_.kindName -eq "assign" -and
        $_.indexWriteErrorAbiName -eq "may-raise"
    }).Count -ne $typedWrites.Count ) {
    throw "Typed XLIR index assignments have no one-to-one recoverable failure ABI"
}

$lowered = Invoke-XlirDump $source $loweredPath $true
$setOps = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -in @(
        "xvalue-array-set-i64-new",
        "xvalue-int-map-set-i64-new",
        "xvalue-object-set-literal-new",
        "xvalue-object-set-string-new",
        "xvalue-index-set-xvalue-key-new")
})
Assert-ExactNames $setOps @{
    "xvalue-array-set-i64-new" = 5
    "xvalue-int-map-set-i64-new" = 7
    "xvalue-object-set-literal-new" = 1
    "xvalue-object-set-string-new" = 1
    "xvalue-index-set-xvalue-key-new" = 1
} "runtimeOpName" "Lowered XLIR consuming index RuntimeOp"
foreach ( $operation in $setOps ) {
    $literalShape = $operation.runtimeOpName -eq
        "xvalue-object-set-literal-new"
    $expectedOperandCount = if ( $literalShape ) { 2 } else { 3 }
    $expectedImmediate = if ( $literalShape ) { 1 } else { 0 }
    if ( $operation.kindName -ne "runtime-call" -or
         $operation.operandCount -ne $expectedOperandCount -or
         $operation.hasImmediate -ne $expectedImmediate -or
         $operation.result -eq 0 ) {
        throw "Lowered XLIR index write has no exact key/item/result ABI"
    }
    $branches = @($lowered.instructions | Where-Object {
        $_.kindName -eq "branch" -and
        $_.origin -eq $operation.origin -and
        $_.operandCount -eq 1 -and
        $_.operands[0] -eq $operation.result
    })
    if ( $branches.Count -ne 1 -or $branches[0].trueTarget -eq 0 -or
         $branches[0].falseTarget -eq 0 -or
         $branches[0].trueTarget -eq $branches[0].falseTarget ) {
        throw "Lowered XLIR index write result has no exact success/failure CFG edge"
    }
    $failureBlock = @($lowered.blocks | Where-Object {
        $_.id -eq $branches[0].falseTarget
    })
    if ( $failureBlock.Count -ne 1 ) {
        throw "Lowered XLIR index write failure block is unresolved"
    }
    $failureStart = [int]$failureBlock[0].firstInstruction
    $failureEnd = $failureStart + [int]$failureBlock[0].instructionCount
    $failureInstructions = @($lowered.instructions | Where-Object {
        $_.index -ge $failureStart -and $_.index -lt $failureEnd
    })
    $raises = @($failureInstructions | Where-Object {
        $_.kindName -eq "error-raise" -and $_.origin -eq $operation.origin -and
        $_.hasImmediate -eq 1 -and $_.operandCount -eq 1
    })
    $throws = @($failureInstructions | Where-Object {
        $_.kindName -eq "throw" -and $_.origin -eq $operation.origin
    })
    if ( $raises.Count -ne 1 -or $throws.Count -ne 1 ) {
        throw "Lowered XLIR index write failure is not one recoverable RangeError transfer"
    }
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR index write Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($generated,
        "\s+v[0-9]+ = xlir_xvalue_array_set_i64_new\(v[0-9]+, v[0-9]+, v[0-9]+\);")).Count -ne 5 -or
     ([regex]::Matches($generated,
        "\s+v[0-9]+ = xrtValueIntMapSetNew\(v[0-9]+, v[0-9]+, v[0-9]+\);")).Count -ne 7 -or
     ([regex]::Matches($generated,
        "\s+v[0-9]+ = xlir_xvalue_index_set_xvalue_key_new\(v[0-9]+, v[0-9]+, v[0-9]+\);")).Count -ne 1 ) {
    throw "Generated C does not mechanically preserve array/list/dynamic write selection"
}
foreach ( $required in @(
    "static bool xlir_xvalue_array_set_i64_new",
    "extern bool xrtValueIntMapSetNew(xvalue*, int64_t, xvalue*);",
    "xrtValueObjectSetNew(", "xrtValueUInt(",
    "static bool xlir_xvalue_index_set_xvalue_key_new"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "Generated C is missing mechanical index-write emission: $required"
    }
}
foreach ( $forbidden in @(
    "indexWriteAbi", "collectionAbi", "lvalueKind", "sourceKind",
    "xvalue-list-i64-set-new") ) {
    if ( $generated.Contains($forbidden) ) {
        throw "Generated C leaked Typed index semantics: $forbidden"
    }
}

$genericTyped = Invoke-XlirDump $genericSource $genericTypedPath $false
if ( @($genericTyped.nodes | Where-Object {
        $_.indexWriteAbiName -ne "none"
    }).Count -ne 0 ) {
    throw "Generic template invented a concrete IndexWriteAbi before specialization"
}
$genericLowered = Invoke-XlirDump $genericSource $genericLoweredPath $true
$genericWrites = @($genericLowered.nodes | Where-Object {
    $_.indexWriteAbiName -ne "none"
})
Assert-ExactNames $genericWrites @{
    "xvalue-array-i64-set-new" = 2
    "xvalue-list-i64-set-new" = 2
} "indexWriteAbiName" "Specialized XLIR IndexWriteAbi refresh"
if ( @($genericLowered.nodes | Where-Object {
        $_.kindName -eq "assign" -and
        $_.indexWriteErrorAbiName -eq "may-raise"
    }).Count -ne $genericWrites.Count ) {
    throw "Specialized XLIR index assignments lost their recoverable failure ABI"
}
$genericSetOps = @($genericLowered.instructions | Where-Object {
    $_.runtimeOpName -in @(
        "xvalue-array-set-i64-new", "xvalue-int-map-set-i64-new")
})
Assert-ExactNames $genericSetOps @{
    "xvalue-array-set-i64-new" = 2
    "xvalue-int-map-set-i64-new" = 2
} "runtimeOpName" "Specialized XLIR index RuntimeOp"

& $xl cgen $genericSource --backend xlir --emit $genericCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic index write Cgen failed" }

$irSource = [System.IO.File]::ReadAllText($irSourcePath, [System.Text.Encoding]::UTF8)
$lowerSource = [System.IO.File]::ReadAllText($lowerSourcePath, [System.Text.Encoding]::UTF8)
$cgenSource = [System.IO.File]::ReadAllText($cgenSourcePath, [System.Text.Encoding]::UTF8)
foreach ( $include in [regex]::Matches(
    $cgenSource, '#include "(?<path>src/cgen/xl6_ir_cgen[^\"]+\.c)"') ) {
    $path = Join-Path $Root ($include.Groups["path"].Value -replace '/', '\')
    $cgenSource += "`n" + [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}
foreach ( $required in @(
    "xl6IrClassifyIndexWriteAbi", "xl6IrClassifyIndexWriteErrorAbi",
    "xl6IrRefreshIndexWriteAbis", "indexWriteErrorAbi",
    "typed XLIR index mutation has no exact canonical write and failure ABI",
    "arrayType->collectionAbi != XL6_IR_COLLECTION_ARRAY",
    "mapType->collectionAbi != XL6_IR_COLLECTION_LIST"
) ) {
    if ( ! $irSource.Contains($required) ) {
        throw "XLIR index write verifier contract is incomplete: $required"
    }
}
foreach ( $required in @(
    "switch ( lhs->indexWriteAbi )",
    "XL6_IR_INDEX_WRITE_XVALUE_LIST_I64_SET_NEW",
    "XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW",
    "xl6IrLowerEmitCheckedIndexWrite", "XL6_IR_INSTR_ERROR_RAISE",
    "container index assignment failed", "RangeError",
    "XLIR index write has no explicit commit result or exception ABI",
    "XLIR index assignment has no verified write ABI"
) ) {
    if ( ! $lowerSource.Contains($required) ) {
        throw "XLIR index write lowering contract is incomplete: $required"
    }
}
if ( $cgenSource.Contains("XL6_IR_INDEX_WRITE_") ) {
    throw "XLIR Cgen must not consume Typed IndexWriteAbi"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostPath`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR index write strict generated-C runtime failed" }

$genericCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$genericCPath`" `"$hostPath`" !XL6_CGEN_LINKS! -o `"$genericExePath`" && `"$genericExePath`""
& $env:ComSpec /d /v:on /c $genericCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic index write strict generated-C runtime failed" }

Write-Output "XLIR index write ABI gate ok: first-class verified IndexWriteAbi and recoverable failure ABI, exact array/list/object/dynamic consuming RuntimeOps, uint64 boxing, null-aware list reuse, generic specialization refresh, one RangeError CFG edge per write, mechanical C, -Werror and runtime semantics."
exit 0
