param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'xlir_iterator_gate_contract.ps1')

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

function Read-Utf8([string]$path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Assert-Deterministic([string]$first, [string]$second)
{
    if ( ! [System.Linq.Enumerable]::SequenceEqual(
            [byte[]][System.IO.File]::ReadAllBytes($first),
            [byte[]][System.IO.File]::ReadAllBytes($second)
        ) ) {
        throw "XLIR dynamic var iterator output is not deterministic: $first"
    }
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_dynamic_var_iterator_abi.xl"
$historicalSource = Join-Path $Root "test\cases\cgen_for_in_containers.xl"
$invalidSource = Join-Path $Root "test\cases\cgen_for_in_var_non_container.xl"
$hostSource = Join-Path $Root "test\cases\xlir_dynamic_var_iterator_abi_host.c"
$genericHostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_dynamic_var_iterator_abi.typed.json"
$typedRepeatPath = Join-Path $outputDir "xlir_dynamic_var_iterator_abi.typed.repeat.json"
$loweredPath = Join-Path $outputDir "xlir_dynamic_var_iterator_abi.lowered.json"
$loweredRepeatPath = Join-Path $outputDir "xlir_dynamic_var_iterator_abi.lowered.repeat.json"
$cPath = Join-Path $outputDir "xlir_dynamic_var_iterator_abi.c"
$cRepeatPath = Join-Path $outputDir "xlir_dynamic_var_iterator_abi.repeat.c"
$historicalCPath = Join-Path $outputDir "cgen_for_in_containers.dynamic.xlir.c"
$historicalCRepeatPath = Join-Path $outputDir "cgen_for_in_containers.dynamic.xlir.repeat.c"
$invalidCPath = Join-Path $outputDir "cgen_for_in_var_non_container.dynamic.xlir.c"
$invalidCRepeatPath = Join-Path $outputDir "cgen_for_in_var_non_container.dynamic.xlir.repeat.c"
$xlirExe = Join-Path $outputDir "xlir_dynamic_var_iterator_abi.xlir.exe"
$historicalExe = Join-Path $outputDir "cgen_for_in_containers.dynamic.xlir.exe"
$invalidExe = Join-Path $outputDir "cgen_for_in_var_non_container.dynamic.xlir.exe"

foreach ( $path in @(
    $xl, $source, $historicalSource, $invalidSource,
    $hostSource, $genericHostSource
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR dynamic var iterator gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic var iterator typed JSON failed" }
& $xl ir $source --typed --format json --emit $typedRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR repeated dynamic var iterator typed JSON failed" }
& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic var iterator lowering failed" }
& $xl ir $source --lowered --format json --emit $loweredRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR repeated dynamic var iterator lowering failed" }
& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic var iterator Cgen failed" }
& $xl cgen $source --backend xlir --emit $cRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR repeated dynamic var iterator Cgen failed" }
& $xl cgen $historicalSource --backend xlir --emit $historicalCPath
if ( $LASTEXITCODE -ne 0 ) { throw "Established dynamic container for-loop Cgen failed" }
& $xl cgen $historicalSource --backend xlir --emit $historicalCRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "Repeated established dynamic container for-loop Cgen failed" }
& $xl cgen $invalidSource --backend xlir --emit $invalidCPath
if ( $LASTEXITCODE -ne 0 ) { throw "Dynamic non-container for-loop Cgen failed" }
& $xl cgen $invalidSource --backend xlir --emit $invalidCRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "Repeated dynamic non-container for-loop Cgen failed" }

Assert-Deterministic $typedPath $typedRepeatPath
Assert-Deterministic $loweredPath $loweredRepeatPath
Assert-Deterministic $cPath $cRepeatPath
Assert-Deterministic $historicalCPath $historicalCRepeatPath
Assert-Deterministic $invalidCPath $invalidCRepeatPath

$typed = Read-Utf8 $typedPath | ConvertFrom-Json
$lowered = Read-Utf8 $loweredPath | ConvertFrom-Json
$instructions = @($lowered.instructions)

$allDynamicTypes = @($typed.types | Where-Object {
    $_.kind -eq 1 -and $_.name -eq "var"
})
$dynamicTypes = @($allDynamicTypes | Where-Object {
    $_.collectionAbiName -eq "none" -and $_.cAbi -eq 26 -and
    $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 2 -and
    @($_.params).Count -eq 0 -and
    $_.typeValueId -eq $typed.runtimeTypeMap.fallback
})
if ( $dynamicTypes.Count -lt 1 -or
     $dynamicTypes.Count -ne $allDynamicTypes.Count ) {
    throw "Typed XLIR has a non-canonical dynamic xvalue TypeId"
}
$dynamicTypeIds = @($dynamicTypes | ForEach-Object { $_.id })

$cursorTypes = @($typed.types | Where-Object {
    $_.kind -eq 14 -and $_.name -eq "value-iterator" -and
    $_.collectionAbiName -eq "none" -and $_.cAbi -eq 37 -and
    $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 1 -and
    @($_.params).Count -eq 1 -and $_.params[0] -in $dynamicTypeIds
})
if ( $cursorTypes.Count -ne 1 ) {
    throw "Typed XLIR dynamic var loops have no unique source-bound cursor TypeId"
}
$cursorType = $cursorTypes[0]
$dynamicType = @($dynamicTypes | Where-Object {
    $_.id -eq $cursorType.params[0]
})[0]

$loops = @($typed.nodes | Where-Object {
    $_.loopAbiName -like "xvalue-iterator-*"
})
$valueLoops = @($loops | Where-Object {
    $_.loopAbiName -eq "xvalue-iterator-value" -and
    $_.loopKeyAbiName -eq "none" -and
    $_.loopItemAbiName -eq "dynamic-logical-xvalue" -and
    $_.loopKeyLifetimeAbiName -eq "none" -and
    $_.loopItemLifetimeAbiName -eq "owned-iteration-release"
})
$keyValueLoops = @($loops | Where-Object {
    $_.loopAbiName -eq "xvalue-iterator-key-value" -and
    $_.loopKeyAbiName -eq "dynamic-xvalue" -and
    $_.loopItemAbiName -eq "xvalue-retain" -and
    $_.loopKeyLifetimeAbiName -eq "owned-iteration-release" -and
    $_.loopItemLifetimeAbiName -eq "owned-iteration-release"
})
$discardLoops = @($loops | Where-Object {
    $_.loopItemAbiName -eq "discard" -and
    $_.loopItemLifetimeAbiName -eq "none"
})
$discardPairLoops = @($discardLoops | Where-Object {
    $_.loopAbiName -eq "xvalue-iterator-key-value" -and
    $_.loopKeyAbiName -eq "discard" -and
    $_.loopKeyLifetimeAbiName -eq "none"
})
$discardItemLoops = @($discardLoops | Where-Object {
    $_.loopAbiName -eq "xvalue-iterator-value" -and
    $_.loopKeyAbiName -eq "none" -and
    $_.loopKeyLifetimeAbiName -eq "none"
})
if ( $loops.Count -ne 14 -or $valueLoops.Count -ne 7 -or
     $keyValueLoops.Count -ne 5 -or $discardLoops.Count -ne 2 -or
     $discardPairLoops.Count -ne 1 -or $discardItemLoops.Count -ne 1 -or
     @($loops | Where-Object {
        $_.loopErrorAbiName -ne "may-raise" -or
        $_.loopResourceAbiName -ne "xvalue-iterator-destroy" -or
        $_.loopResourceType -ne $cursorType.id
     }).Count -ne 0 ) {
    throw "Typed XLIR dynamic var loop shape, projection, effect or resource ABI is not exact"
}

$loopIds = @($loops | ForEach-Object { Get-XlirIteratorOrigins $typed $_ })
$loopInstructions = @($instructions | Where-Object { $_.origin -in $loopIds })
$expectedRuntimeOps = @{
    "xvalue-iterator-create-checked" = 14
    "xvalue-iterator-advance" = 14
    "xvalue-iterator-dynamic-logical-xvalue" = 7
    "xvalue-iterator-item-xvalue-retain" = 5
    "xvalue-iterator-key-dynamic-xvalue" = 5
}
foreach ( $entry in $expectedRuntimeOps.GetEnumerator() ) {
    if ( @($loopInstructions | Where-Object {
            $_.runtimeOpName -eq $entry.Key
         }).Count -ne $entry.Value ) {
        throw "Lowered XLIR dynamic var iterator operation count is not exact: $($entry.Key)"
    }
}

$projectionOperations = @($loopInstructions | Where-Object {
    $_.runtimeOpName -in @(
        "xvalue-iterator-dynamic-logical-xvalue",
        "xvalue-iterator-item-xvalue-retain",
        "xvalue-iterator-key-dynamic-xvalue"
    )
})
if ( @($projectionOperations | Where-Object {
        $_.type -ne $dynamicType.id -or $_.result -le 0
     }).Count -ne 0 ) {
    throw "Lowered XLIR dynamic key/item projection escaped the canonical managed var TypeId"
}

$cursorSymbols = @()
$ownedRvalueAcquisitions = 0
foreach ( $loop in $loops ) {
    Assert-XlirCheckedIteratorAdvance $lowered $loop
    $origins = @(Get-XlirIteratorOrigins $typed $loop)
    $perLoop = @($loopInstructions | Where-Object { $_.origin -in $origins })
    $creates = @($perLoop | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-create-checked"
    })
    $advances = @($perLoop | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-advance"
    })
    if ( $creates.Count -ne 1 -or $advances.Count -ne 1 -or
         $creates[0].type -ne $cursorType.id -or
         @($creates[0].operands).Count -ne 1 ) {
        throw "Lowered XLIR dynamic loop has no unique checked-create/advance pair: n$($loop.id)"
    }

    $create = $creates[0]
    $iteratorNodes = @($typed.nodes | Where-Object {
        $_.parent -eq $loop.id -and
        $_.sourceKindName -notin @("for_targets", "block")
    })
    $iteratorNode = $null
    if ( $iteratorNodes.Count -eq 1 ) {
        $iteratorNode = $iteratorNodes[0]
    }
    $logicalType = @($typed.types | Where-Object {
        $_.id -eq $create.payloadType
    })[0]
    if ( $null -eq $iteratorNode -or $null -eq $logicalType -or
         $iteratorNode.storageType -ne $dynamicType.id -or
         $create.payloadType -ne $iteratorNode.type -or
         ($iteratorNode.type -eq $dynamicType.id -and
          $logicalType.collectionAbiName -ne "none") -or
         ($iteratorNode.type -ne $dynamicType.id -and
          $logicalType.collectionAbiName -ne "array") ) {
        throw "Checked iterator lost its independent physical var and logical collection TypeIds: n$($loop.id)"
    }
    $createIndex = -1
    for ( $i = 0; $i -lt $instructions.Count; ++$i ) {
        if ( $instructions[$i].id -eq $create.id ) {
            $createIndex = $i
            break
        }
    }
    if ( $createIndex -lt 0 -or $createIndex + 1 -ge $instructions.Count ) {
        throw "Lowered XLIR dynamic checked-create is absent from instruction order"
    }
    $errorTest = $instructions[$createIndex + 1]
    if ( $errorTest.kindName -ne "error-test" -or
         @($errorTest.operands).Count -ne 1 -or
         $errorTest.operands[0] -ne $create.result ) {
        throw "Dynamic checked-create is not immediately consumed by its unique ERROR_TEST: n$($loop.id)"
    }
    if ( @($instructions | Where-Object {
            $_.kindName -eq "error-test" -and
            @($_.operands).Count -eq 1 -and
            $_.operands[0] -eq $create.result
         }).Count -ne 1 ) {
        throw "Dynamic checked-create result has no unique ERROR_TEST consumer: v$($create.result)"
    }

    $cursorStores = @(Get-XlirIteratorCursorStore $lowered $loop)
    if ( $cursorStores.Count -ne 1 ) {
        throw "Lowered XLIR dynamic loop has no unique compiler-owned cursor storage: n$($loop.id)"
    }
    $cursorSymbol = [long]$cursorStores[0].targetSymbol
    $cursorSymbols += $cursorSymbol
    if ( @($perLoop | Where-Object {
            $_.kindName -eq "release" -and $_.type -eq $cursorType.id -and
            [long]$_.targetSymbol -eq $cursorSymbol
         }).Count -lt 1 ) {
        throw "Lowered XLIR dynamic loop has an exit without cursor cleanup: n$($loop.id)"
    }

    $createOperand = $create.operands[0]
    $sourceDefinitions = @($perLoop | Where-Object { $_.result -eq $createOperand })
    if ( $sourceDefinitions.Count -eq 1 -and
         $sourceDefinitions[0].kindName -eq "load" -and
         [long]$sourceDefinitions[0].targetSymbol -ge 4026531840L ) {
        $sourceSymbol = [long]$sourceDefinitions[0].targetSymbol
        $sourceReleases = @($perLoop | Where-Object {
            $_.kindName -eq "release" -and $_.type -eq $dynamicType.id -and
            [long]$_.targetSymbol -eq $sourceSymbol
        })
        if ( $sourceReleases.Count -ne 2 -or
             @($perLoop | Where-Object {
                $_.runtimeOpName -eq "xvalue-clone"
             }).Count -ne 0 ) {
            throw "Dynamic owned-rvalue checked-create release is not symmetric on success/failure"
        }
        ++$ownedRvalueAcquisitions
    }
}
if ( @($cursorSymbols | Select-Object -Unique).Count -ne $loops.Count -or
     $ownedRvalueAcquisitions -ne 2 ) {
    throw "Dynamic loops do not own unique cursors or the fixture lost its owned-rvalue acquisition"
}

$generated = Read-Utf8 $cPath
$renderedCounts = @{
    "=\s*xlir_xvalue_iterator_create_checked\(v\d+,\s*0\);" = 13
    "=\s*xlir_xvalue_iterator_create_checked\(v\d+,\s*9\);" = 1
    "=\s*xlir_xvalue_iterator_advance\(v\d+, &v\d+\);" = 14
    "=\s*xlir_xvalue_iterator_dynamic_logical_xvalue\(v\d+\);" = 7
    "=\s*xlir_xvalue_iterator_item_xvalue_retain\(v\d+\);" = 5
    "=\s*xlir_xvalue_iterator_key_dynamic_xvalue\(v\d+\);" = 5
}
foreach ( $entry in $renderedCounts.GetEnumerator() ) {
    if ( ([regex]::Matches($generated, $entry.Key)).Count -ne $entry.Value ) {
        throw "Generated C does not mechanically render every verified dynamic iterator operation: $($entry.Key)"
    }
}
foreach ( $required in @(
    "static xlir_xvalue_iterator* xlir_xvalue_iterator_create_checked(const xvalue* source, int expectedCollectionTag)",
    "if (expectedCollectionTag < 9 || expectedCollectionTag > 12 || type != expectedCollectionTag) goto incompatible;",
    "} else if (type < 9 || type > 12) goto incompatible;",
    "for in value must be array, list, set or dict",
    "static xvalue* xlir_xvalue_iterator_key_dynamic_xvalue(",
    "if (state->key.Type == 0)",
    "else if (state->key.Type == 1)",
    "else if (state->key.Type == 2)",
    "else if (state->key.Type == 3)",
    "static xvalue* xlir_xvalue_iterator_dynamic_logical_xvalue(",
    "if (state->key.Type == 3) return xlir_xvalue_iterator_key_dynamic_xvalue(iterator);",
    "return xlir_xvalue_iterator_item_xvalue_retain(iterator);"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "Generated C dynamic iterator runtime contract is incomplete: $required"
    }
}
foreach ( $forbidden in @(
    "loopAbi", "loopKeyAbi", "loopItemAbi", "collectionAbiName",
    "runtimeIdentityType", "XL6_HIR", "xl6Hir"
) ) {
    if ( $generated.Contains($forbidden) ) {
        throw "Generated C leaked semantic metadata or HIR tracing: $forbidden"
    }
}
foreach ( $cursorSymbol in $cursorSymbols ) {
    $loweredReleaseCount = @($lowered.instructions | Where-Object {
        $_.kindName -eq "release" -and
        [long]$_.targetSymbol -eq $cursorSymbol
    }).Count
    $renderedReleaseCount = Get-XlirRenderedIteratorReleases $generated $cursorSymbol 'xlir_xvalue_iterator*' 'xlir_xvalue_iterator_destroy'
    if ( $renderedReleaseCount -ne $loweredReleaseCount ) {
        throw "Generated C does not preserve every dynamic cursor cleanup edge: s$cursorSymbol"
    }
}
$invalidGenerated = Read-Utf8 $invalidCPath
if ( ! $invalidGenerated.Contains("xlir_xvalue_iterator_create_checked") -or
     ! $invalidGenerated.Contains("for in value must be array, list, set or dict") ) {
    throw "Established dynamic non-container path does not use the checked iterator TypeError ABI"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$xlirExe`" && `"$xlirExe`" && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$historicalCPath`" `"$genericHostSource`" !XL6_CGEN_LINKS! -o `"$historicalExe`" && `"$historicalExe`" && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$invalidCPath`" `"$genericHostSource`" !XL6_CGEN_LINKS! -o `"$invalidExe`""
Push-Location $Root
try {
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Dynamic var iterator strict compile/runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR dynamic var iterator ABI gate ok: focused and established cgen_for_in_containers/cgen_for_in_var_non_container coverage, independent physical var/logical refined collection TypeIds, exact refined-family rejection, runtime-checked array/list/set/dict dispatch, explicit key/item discard without projection or storage, immediate ERROR_TEST, canonical managed key/item projections, source-bound cursors, owned-rvalue checked-create release, cleanup across continue/break/return/finally, exact TypeError, deterministic mechanical C, and strict compile/runtime."
exit 0
