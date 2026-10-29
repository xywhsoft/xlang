param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_multi_assign_index_abi.xl"
$generated = Join-Path $Root "build\test\xlir_multi_assign_index_generated.c"
$generatedRepeat = Join-Path $Root "build\test\xlir_multi_assign_index_generated_repeat.c"
$lowered = Join-Path $Root "build\test\xlir_multi_assign_index_lowered.json"
$callableSource = Join-Path $Root "test\cases\cgen_typed_container_multi_assign_index.xl"
$callableTyped = Join-Path $Root "build\test\xlir_multi_assign_callable.typed.json"
$callableLowered = Join-Path $Root "build\test\xlir_multi_assign_callable.lowered.json"
$callableGenerated = Join-Path $Root "build\test\xlir_multi_assign_callable.c"
$callableGeneratedRepeat = Join-Path $Root "build\test\xlir_multi_assign_callable.repeat.c"
foreach ( $path in @($xl, $source, $callableSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR multi-assignment index gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $generated) | Out-Null

& $xl cgen $source --backend xlir --emit $generated
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR multi-assignment index Cgen failed" }
& $xl cgen $source --backend xlir --emit $generatedRepeat
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR repeated multi-assignment index Cgen failed" }
if ( [IO.File]::ReadAllText($generated) -cne
     [IO.File]::ReadAllText($generatedRepeat) ) {
    throw "XLIR multi-assignment index Cgen is not deterministic"
}

& $xl ir $source --lowered --format json --emit $lowered
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR multi-assignment index lowering failed" }
$ir = Get-Content -LiteralPath $lowered -Raw | ConvertFrom-Json
$nodes = @{}
foreach ( $node in $ir.nodes ) { $nodes[[int]$node.id] = $node }
$writeOps = @(
    "xvalue-array-set-i64-new",
    "xvalue-int-map-set-i64-new",
    "xvalue-object-set-literal-new",
    "xvalue-object-set-string-new",
    "xvalue-index-set-xvalue-key-new"
)
$multiAssignments = @($ir.nodes | Where-Object {
    if ( $_.kindName -ne "assign" -or $_.opcodeName -ne "none" ) { return $false }
    $lhsId = [int]$ir.nodeChildren[[int]$_.firstChild]
    return $nodes[$lhsId].kindName -eq "list"
})
if ( $multiAssignments.Count -ne 4 ) {
    throw "expected 4 indexed product assignments, got $($multiAssignments.Count)"
}
foreach ( $assignment in $multiAssignments ) {
    $lhsId = [int]$ir.nodeChildren[[int]$assignment.firstChild]
    $lhs = $nodes[$lhsId]
    $targets = @()
    for ( $i = 0; $i -lt [int]$lhs.childCount; ++$i ) {
        $targets += [int]$ir.nodeChildren[[int]$lhs.firstChild + $i]
    }
    if ( $targets.Count -ne 2 ) {
        throw "multi-assignment $($assignment.id) does not have two target descriptors"
    }
    foreach ( $targetId in $targets ) {
        if ( $nodes[$targetId].kindName -ne "index" ) {
            throw "multi-assignment $($assignment.id) target $targetId is not an INDEX"
        }
        $projection = @($ir.instructions | Where-Object {
            [int]$_.origin -eq $targetId -and
            $_.kindName -in @("product-get", "product-take")
        })
        if ( $projection.Count -ne 1 ) {
            throw "multi-assignment target $targetId lacks one exact product projection"
        }
    }
    $writes = @($ir.instructions | Where-Object {
        [int]$_.origin -eq [int]$assignment.id -and $_.runtimeOpName -in $writeOps
    })
    if ( $writes.Count -ne $targets.Count ) {
        throw "multi-assignment $($assignment.id) lacks one write per indexed target"
    }
}
$takeCount = @($ir.instructions | Where-Object { $_.kindName -eq "product-take" }).Count
$getCount = @($ir.instructions | Where-Object { $_.kindName -eq "product-get" }).Count
if ( $takeCount -lt 3 -or $getCount -lt 3 ) {
    throw "multi-assignment fixture did not cover both managed PRODUCT_TAKE and trivial PRODUCT_GET"
}

# A product-to-index write may cross an erased container boundary after the
# tuple call has returned.  Typed XLIR owns that aggregate may-raise effect;
# Lowered XLIR owns each callable adapter and its adjacent propagation edge.
& $xl ir $callableSource --format json --emit $callableTyped
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR callable multi-assignment typed IR failed" }
& $xl ir $callableSource --lowered --format json --emit $callableLowered
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR callable multi-assignment lowering failed" }
$callableTypedIr = Get-Content -LiteralPath $callableTyped -Raw | ConvertFrom-Json
$callableAssignment = @($callableTypedIr.nodes | Where-Object {
    $_.sourceKindName -eq "multi_assign" -and [int]$_.line -eq 88
})
if ( $callableAssignment.Count -ne 1 -or
     $callableAssignment[0].implicitConversionRuntimeOpName -ne
         "xvalue-callable-from-closure" -or
     $callableAssignment[0].implicitConversionErrorAbiName -ne "may-raise" -or
     @($callableTypedIr.exitPlans | Where-Object {
         [int]$_.origin -eq [int]$callableAssignment[0].id -and [int]$_.kind -eq 4
     }).Count -ne 1 ) {
    throw "XLIR callable multi-assignment has no explicit checked contextual conversion and ExitPlan ABI"
}
$callableLoweredIr = Get-Content -LiteralPath $callableLowered -Raw | ConvertFrom-Json
$callableBoxes = @($callableLoweredIr.instructions | Where-Object {
    [int]$_.origin -eq [int]$callableAssignment[0].id -and
    $_.runtimeOpName -eq "xvalue-callable-from-closure"
})
if ( $callableBoxes.Count -ne 2 ) {
    throw "XLIR callable multi-assignment did not emit two exact callable adapters"
}
foreach ( $box in $callableBoxes ) {
    $test = @($callableLoweredIr.instructions | Where-Object {
        [int]$_.index -eq ([int]$box.index + 1) -and
        $_.kindName -eq "error-test" -and
        @($_.operands).Count -eq 1 -and
        [int]$_.operands[0] -eq [int]$box.result
    })
    if ( $test.Count -ne 1 -or [int]$test[0].trueTarget -eq 0 -or
         [int]$test[0].falseTarget -eq 0 ) {
        throw "XLIR callable multi-assignment adapter has no adjacent checked failure CFG"
    }
}
& $xl cgen $callableSource --backend xlir --emit $callableGenerated
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR callable multi-assignment Cgen failed" }
& $xl cgen $callableSource --backend xlir --emit $callableGeneratedRepeat
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR repeated callable multi-assignment Cgen failed" }
if ( [IO.File]::ReadAllText($callableGenerated) -cne
     [IO.File]::ReadAllText($callableGeneratedRepeat) ) {
    throw "XLIR callable multi-assignment Cgen is not deterministic"
}

Push-Location $Root
try {
    $command = "call test\suites\common.bat && " +
        "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror " +
        "-include build\test\xlir_multi_assign_index_generated.c " +
        "test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! " +
        "-o build\test\xlir_multi_assign_index.exe && " +
        "build\test\xlir_multi_assign_index.exe"
    & cmd.exe /v:on /d /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR multi-assignment index generated-C runtime failed"
    }

    $callableCommand = "call test\suites\common.bat && " +
        "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror " +
        "-DXLIR_GENERATED_MODULE_PREINCLUDED -include build\test\xlir_multi_assign_callable.c " +
        "test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c " +
        "!XL6_CGEN_LINKS! -o build\test\xlir_multi_assign_callable.exe && " +
        "build\test\xlir_multi_assign_callable.exe"
    & cmd.exe /v:on /d /c $callableCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR callable multi-assignment strict-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR multi-assignment index gate ok: product evaluation is shared, managed/trivial/callable projections are explicit, callable erasure owns a typed may-raise ExitPlan plus adjacent Lowered ERROR_TEST, every INDEX target has one exact write, generated C is deterministic, -Werror clean, and runtime semantics pass."
