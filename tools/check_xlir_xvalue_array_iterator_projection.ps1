param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_xvalue_array_iterator_projection.xl"
$hostSource = Join-Path $Root "test\cases\xlir_xvalue_array_iterator_projection_host.c"
$outputDir = Join-Path $Root "build\test"
$temporaryDir = Join-Path $Root "build\tmp"
$typedPath = Join-Path $outputDir "xlir_xvalue_array_iterator_projection.typed.json"
$loweredPath = Join-Path $outputDir "xlir_xvalue_array_iterator_projection.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_xvalue_array_iterator_projection.c"
$repeatPath = Join-Path $temporaryDir "xlir_xvalue_array_iterator_projection.repeat.c"
$executablePath = Join-Path $temporaryDir "xlir_xvalue_array_iterator_projection.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR xvalue-array iterator gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir,$temporaryDir | Out-Null

& $xl ir $source --typed --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue-array iterator typed JSON failed" }
& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue-array iterator lowering failed" }

$typed = [System.IO.File]::ReadAllText(
    $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$loops = @($typed.nodes | Where-Object { $_.kind -eq 19 })
if ( $loops.Count -ne 10 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-key-value" -and
        $_.loopKeyAbiName -eq "ordinal-i64" -and
        $_.loopItemAbiName -eq "string-copy" -and
        $_.loopItemLifetimeAbiName -eq "owned-iteration-release"
     }).Count -ne 3 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-key-value" -and
        $_.loopKeyAbiName -eq "ordinal-i64" -and
        $_.loopItemAbiName -eq "u64" -and
        $_.loopItemLifetimeAbiName -eq "trivial-replace"
     }).Count -ne 1 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-key-value" -and
        $_.loopKeyAbiName -eq "ordinal-i64" -and
        $_.loopItemAbiName -eq "xvalue-retain" -and
        $_.loopItemLifetimeAbiName -eq "owned-iteration-release"
     }).Count -ne 1 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-value" -and
        $_.loopKeyAbiName -eq "none" -and
        $_.loopItemAbiName -eq "bool" -and
        $_.loopItemLifetimeAbiName -eq "trivial-replace"
     }).Count -ne 1 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-value" -and
        $_.loopKeyAbiName -eq "none" -and
        $_.loopItemAbiName -eq "f64" -and
        $_.loopItemLifetimeAbiName -eq "trivial-replace"
     }).Count -ne 1 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-value" -and
        $_.loopKeyAbiName -eq "none" -and
        $_.loopItemAbiName -eq "xvalue-retain" -and
        $_.loopItemLifetimeAbiName -eq "owned-iteration-release"
     }).Count -ne 3 -or
     @($loops | Where-Object {
        $_.loopErrorAbiName -ne "may-raise" -or
        $_.loopResourceAbiName -ne "xvalue-iterator-destroy"
     }).Count -ne 0 ) {
    throw "Typed XLIR xvalue-array loop projection/lifetime matrix is not exact"
}

$arrayTypes = @($typed.types | Where-Object {
    $_.kind -eq 6 -and $_.collectionAbiName -eq "array" -and
    $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and
    @($_.params).Count -eq 1
})
$cursorTypes = @($typed.types | Where-Object {
    $_.kind -eq 14 -and $_.cAbi -eq 37 -and $_.ownershipAbi -eq 2 -and
    $_.nullAbi -eq 1 -and @($_.params).Count -eq 1
})
if ( $arrayTypes.Count -ne 7 -or $cursorTypes.Count -ne 7 ) {
    throw "Typed XLIR xvalue-array cursor TypeId matrix is incomplete"
}
foreach ( $cursor in $cursorTypes ) {
    if ( @($arrayTypes | Where-Object { $_.id -eq $cursor.params[0] }).Count -ne 1 ) {
        throw "XLIR cursor TypeId is not parameterized by one exact array TypeId"
    }
}
foreach ( $loop in $loops ) {
    $cursor = @($cursorTypes | Where-Object { $_.id -eq $loop.loopResourceType })
    if ( $cursor.Count -ne 1 ) {
        throw "Typed XLIR xvalue-array loop has no exact cursor resource TypeId: n$($loop.id)"
    }
}

$expectedRuntimeOps = [ordered]@{
    "xvalue-iterator-create" = 10
    "xvalue-iterator-advance" = 10
    "xvalue-iterator-item-u64" = 1
    "xvalue-iterator-item-bool" = 1
    "xvalue-iterator-item-f64" = 1
    "xvalue-iterator-item-string-copy" = 3
    "xvalue-iterator-item-xvalue-retain" = 4
    "xvalue-iterator-key-ordinal-i64" = 5
}
foreach ( $entry in $expectedRuntimeOps.GetEnumerator() ) {
    $actual = @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq $entry.Key
    }).Count
    if ( $actual -ne $entry.Value ) {
        throw "Lowered XLIR xvalue-array iterator RuntimeOp count changed: $($entry.Key)=$actual"
    }
}
if ( @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-key-i64"
     }).Count -ne 0 ) {
    throw "Array ordinal projection was collapsed into the list integer-key ABI"
}

$itemOperationByAbi = @{
    "u64" = "xvalue-iterator-item-u64"
    "bool" = "xvalue-iterator-item-bool"
    "f64" = "xvalue-iterator-item-f64"
    "string-copy" = "xvalue-iterator-item-string-copy"
    "xvalue-retain" = "xvalue-iterator-item-xvalue-retain"
}
$itemResources = @()
$cursorResources = @()
$managedItemReleaseTotal = 0
$cursorReleaseTotal = 0
function Get-CursorProjection($module, $loop, [string]$operation) {
    $stores = @($module.instructions | Where-Object {
        $_.origin -eq $loop.id -and $_.kindName -eq 'store' -and $_.type -eq $loop.loopResourceType
    })
    if ($stores.Count -ne 1) { throw 'Iterator projection has no unique cursor owner' }
    $loads = @($module.instructions | Where-Object {
        $_.kindName -eq 'load' -and $_.targetSymbol -eq $stores[0].targetSymbol
    } | ForEach-Object { $_.result })
    return @($module.instructions | Where-Object {
        $_.runtimeOpName -eq $operation -and $_.operandCount -eq 1 -and $_.operands[0] -in $loads
    })
}
foreach ( $loop in $loops ) {
    $instructions = @($lowered.instructions | Where-Object { $_.origin -eq $loop.id })
    $itemProjections = @(Get-CursorProjection $lowered $loop $itemOperationByAbi[$loop.loopItemAbiName])
    $keyProjectionCount = @(Get-CursorProjection $lowered $loop 'xvalue-iterator-key-ordinal-i64').Count
    $expectedKeyProjectionCount = 0
    if ( $loop.loopKeyAbiName -eq "ordinal-i64" ) {
        $expectedKeyProjectionCount = 1
    }
    if ( @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-create"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-advance"
         }).Count -ne 1 -or
         $itemProjections.Count -ne 1 -or
         $keyProjectionCount -ne $expectedKeyProjectionCount ) {
        throw "Lowered XLIR xvalue-array iterator operations are not canonical for n$($loop.id)"
    }
    $itemType = [long]$itemProjections[0].type
    $itemStores = @($instructions | Where-Object {
        $_.kindName -eq "store" -and $_.type -eq $itemType -and
        [uint64]$_.targetSymbol -lt 4000000000
    })
    $cursorStores = @($instructions | Where-Object {
        $_.kindName -eq "store" -and $_.type -eq $loop.loopResourceType -and
        [uint64]$_.targetSymbol -ge 4000000000
    })
    if ( $itemStores.Count -lt 1 -or $cursorStores.Count -ne 1 ) {
        throw "Lowered XLIR xvalue-array loop target/cursor storage is unresolved: n$($loop.id)"
    }
    $itemSymbol = [long]$itemStores[-1].targetSymbol
    $cursorSymbol = [long]$cursorStores[0].targetSymbol
    $itemReleaseCount = @($lowered.instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $itemSymbol
    }).Count
    $cursorReleaseCount = @($lowered.instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $cursorSymbol
    }).Count
    if ( $loop.loopItemLifetimeAbiName -eq "owned-iteration-release" ) {
        if ( $itemReleaseCount -lt 1 ) {
            throw "Managed array item has an escaping path without release: n$($loop.id)"
        }
        $itemResources += [pscustomobject]@{
            Symbol = $itemSymbol
            Type = $itemType
            Abi = $loop.loopItemAbiName
            ReleaseCount = $itemReleaseCount
        }
        $managedItemReleaseTotal += $itemReleaseCount
    } elseif ( $itemReleaseCount -ne 0 ) {
        throw "Trivial array item unexpectedly acquired managed lifetime: n$($loop.id)"
    }
    if ( $cursorReleaseCount -lt 1 ) {
        throw "Array cursor has an escaping path without destroy: n$($loop.id)"
    }
    $cursorResources += [pscustomobject]@{
        Symbol = $cursorSymbol
        ReleaseCount = $cursorReleaseCount
    }
    $cursorReleaseTotal += $cursorReleaseCount
}
if ( $itemResources.Count -ne 7 -or $cursorResources.Count -ne 10 ) {
    throw "Lowered XLIR array item/cursor cleanup distribution changed"
}

$machines = @($lowered.asyncMachines)
if ( $machines.Count -ne 1 -or $machines[0].statusName -ne "analyzed" -or
     $machines[0].suspensionCount -ne 1 ) {
    throw "XLIR array iterator async-continuation coverage is not exact"
}
$machineInstructions = @()
foreach ( $block in @($lowered.asyncMachineBlocks | Where-Object {
        $_.machine -eq $machines[0].id
    }) ) {
    for ( $index = [long]$block.firstInstruction;
          $index -lt [long]$block.firstInstruction + [long]$block.instructionCount;
          ++$index ) {
        $machineInstructions += $lowered.instructions[$index]
    }
}
$machineIteratorCreates = @($machineInstructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-iterator-create"
})
if ( $machineIteratorCreates.Count -ne 1 ) {
    throw "Real async continuation does not own exactly one explicit array cursor"
}
$machineLoop = @($loops | Where-Object {
    $_.id -eq $machineIteratorCreates[0].origin -and
    $_.loopItemAbiName -eq "string-copy" -and
    $_.loopKeyAbiName -eq "ordinal-i64"
})
if ( $machineLoop.Count -ne 1 ) {
    throw "Async continuation iterator lost its Typed loop projection ABI"
}
$machineAwait = @($machineInstructions | Where-Object { $_.kindName -eq "await" })
$asyncCursorStores = @($lowered.instructions | Where-Object {
    $_.origin -eq $machineLoop[0].id -and $_.kindName -eq "store" -and
    $_.type -eq $machineLoop[0].loopResourceType -and
    [uint64]$_.targetSymbol -ge 4000000000
})
$asyncItemProjection = @(Get-CursorProjection $lowered $machineLoop[0] 'xvalue-iterator-item-string-copy')
$asyncItemStores = @($lowered.instructions | Where-Object {
    $_.origin -eq $machineLoop[0].id -and $_.kindName -eq "store" -and
    $asyncItemProjection.Count -eq 1 -and
    $_.type -eq $asyncItemProjection[0].type -and
    [uint64]$_.targetSymbol -lt 4000000000
})
if ( $machineAwait.Count -ne 1 -or $asyncCursorStores.Count -ne 1 -or
     $asyncItemStores.Count -lt 1 ) {
    throw "Async array loop did not materialize its cursor/item suspension inputs"
}
$cursorFrame = @($lowered.asyncMachineFrameSlots | Where-Object {
    $_.storageSymbol -eq $asyncCursorStores[0].targetSymbol
})
$itemFrame = @($lowered.asyncMachineFrameSlots | Where-Object {
    $_.storageSymbol -eq $asyncItemStores[-1].targetSymbol
})
if ( $cursorFrame.Count -ne 1 -or
     $cursorFrame[0].residenceName -ne "frame-storage" -or
     $cursorFrame[0].transferAbiName -ne "none" -or
     $cursorFrame[0].destroyAbiName -ne "destroy-value-iterator" -or
     [uint64]$cursorFrame[0].initBit -eq [uint32]::MaxValue -or
     $itemFrame.Count -ne 1 -or
     $itemFrame[0].residenceName -ne "frame-storage" -or
     $itemFrame[0].destroyAbiName -ne "free-string" -or
     [uint64]$itemFrame[0].initBit -eq [uint32]::MaxValue ) {
    throw "Async array cursor/item frame ownership ABI is not exact"
}
$destroyActions = @($lowered.asyncFrameActions | Where-Object {
    $_.kindName -eq "destroy-if-initialized"
})
foreach ( $frame in @($cursorFrame[0], $itemFrame[0]) ) {
    if ( @($destroyActions | Where-Object { $_.frameSlot -eq $frame.id }).Count -ne 1 ) {
        throw "Async array frame resource has no exact guarded destroy action"
    }
}

& $xl cgen $source --backend xlir --emit $generatedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue-array iterator Cgen failed" }
& $xl cgen $source --backend xlir --emit $repeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue-array iterator repeated Cgen failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($repeatPath)) ) {
    throw "XLIR xvalue-array iterator generated C is not byte-for-byte deterministic"
}
$generated = [System.IO.File]::ReadAllText(
    $generatedPath, [System.Text.Encoding]::UTF8)
$renderedCounts = [ordered]@{
    "=\s*xlir_xvalue_iterator_create\(v\d+\);" = 10
    # Shared async CFG emission renders each verified operation once, not once
    # per continuation entry that can reach it. Match the exact IR counts above.
    "=\s*xlir_xvalue_iterator_advance\(v\d+, &v\d+\);" = 10
    "=\s*xlir_xvalue_iterator_item_string_copy\(v\d+\);" = 3
    "=\s*xlir_xvalue_iterator_item_xvalue_retain\(v\d+\);" = 4
    "=\s*xlir_xvalue_iterator_key_ordinal_i64\(v\d+\);" = 5
    "static xvalue\* xlir_xvalue_iterator_item_xvalue_retain\(" = 1
    "static int64_t xlir_xvalue_iterator_key_ordinal_i64\(" = 1
}
foreach ( $entry in $renderedCounts.GetEnumerator() ) {
    $actual = ([regex]::Matches($generated, $entry.Key)).Count
    if ( $actual -ne $entry.Value ) {
        throw "Generated C iterator rendering count changed: $($entry.Key)=$actual"
    }
}
foreach ( $required in @(
    "state->key.Type != 1", "state->key.Index > INT64_MAX",
    "xrtValueRetain(state->item)", "xlir_async_machine_resume_"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "Generated C lost an explicit array cursor ABI token: $required"
    }
}
foreach ( $forbidden in @(
    "loopAbi", "loopKeyAbi", "loopItemAbi", "loopItemLifetimeAbi",
    "xvalue-iterator-key-i64"
) ) {
    if ( $generated.Contains($forbidden) ) {
        throw "Generated C recovered array-loop semantics outside verified XLIR: $forbidden"
    }
}
foreach ( $resource in $itemResources ) {
    $frame = @($lowered.asyncMachineFrameSlots | Where-Object {
        $_.storageSymbol -eq $resource.Symbol
    })
    $slot = if ( $frame.Count -eq 1 ) {
        $field = [long]$frame[0].id - [long]$machines[0].firstFrameSlot
        "frame->f$field"
    } else { "s$($resource.Symbol)" }
    $drop = if ($resource.Abi -eq 'string-copy') { 'xlir_string_drop' } else { 'xrtValueRelease' }
    $escaped = [regex]::Escape($slot)
    $pattern = "&\(xlir_released\), &\($escaped\), sizeof\(xlir_released\) \},\s*" +
        "\{ &\($escaped\), &\(xlir_empty\), sizeof\($escaped\) \}[\s\S]{0,400}?$drop\(xlir_released\);"
    $rendered = ([regex]::Matches($generated, $pattern)).Count
    $calls = if ($frame.Count -eq 1) {
        $helper = "xlir_async_machine_release_$($machines[0].targetSymbol)_$field"
        ([regex]::Matches($generated, ([regex]::Escape($helper) + '\(frame\);'))).Count
    } else { 0 }
    if ( ($frame.Count -eq 0 -and $rendered -ne $resource.ReleaseCount) -or
         ($frame.Count -eq 1 -and ($rendered -ne 1 -or $calls -ne $resource.ReleaseCount)) ) {
        throw "Generated C lost an owned array-item release: s$($resource.Symbol)"
    }
}
foreach ( $resource in $cursorResources ) {
    $frame = @($lowered.asyncMachineFrameSlots | Where-Object {
        $_.storageSymbol -eq $resource.Symbol
    })
    $slot = if ( $frame.Count -eq 1 ) {
        $field = [long]$frame[0].id - [long]$machines[0].firstFrameSlot
        "frame->f$field"
    } else { "s$($resource.Symbol)" }
    $escaped = [regex]::Escape($slot)
    $pattern = "&\(xlir_released\), &\($escaped\), sizeof\(xlir_released\) \},\s*" +
        "\{ &\($escaped\), &\(xlir_empty\), sizeof\($escaped\) \}[\s\S]{0,400}?xlir_xvalue_iterator_destroy\(xlir_released\);"
    $rendered = ([regex]::Matches($generated, $pattern)).Count
    $calls = if ($frame.Count -eq 1) {
        $helper = "xlir_async_machine_release_$($machines[0].targetSymbol)_$field"
        ([regex]::Matches($generated, ([regex]::Escape($helper) + '\(frame\);'))).Count
    } else { 0 }
    if ( ($frame.Count -eq 0 -and $rendered -ne $resource.ReleaseCount) -or
         ($frame.Count -eq 1 -and ($rendered -ne 1 -or $calls -ne $resource.ReleaseCount)) ) {
        throw "Generated C lost an array-cursor destroy edge: s$($resource.Symbol)"
    }
}
$program = [regex]::Match(
    $generated,
    "static void xlir_async_machine_program_\d+\([^\n]+\)\s*\{.*?(?=\nstatic void xlir_async_machine_resume_)",
    [System.Text.RegularExpressions.RegexOptions]::Singleline)
if ( ! $program.Success -or
     ! $program.Value.Contains("xlir_xvalue_iterator_create(") -or
     ! $program.Value.Contains("xlir_xvalue_iterator_advance(") -or
     ! $program.Value.Contains("xlir_xvalue_iterator_item_string_copy(") -or
     ! $program.Value.Contains("xlir_xvalue_iterator_key_ordinal_i64(") -or
     ! $program.Value.Contains("xrtUtf8Count(") -or
     ! $program.Value.Contains("xlir_async_machine_release_") -or
     ! $generated.Contains("xlir_xvalue_iterator_destroy(") ) {
    throw "Shared async CFG C did not mechanically emit the verified array cursor program"
}
$resume = [regex]::Match($generated,
    "static void xlir_async_machine_resume_\d+_0\(const xfutureresult\* input, xpromise\* output, void\* data\)\s*\{[^}]+\}")
if (!$resume.Success -or !$resume.Value.Contains("xlir_async_machine_program_") -or
    $resume.Value.Contains("xlir_xvalue_iterator_")) {
    throw "Async resume must select the shared CFG without recreating a cursor or duplicating its body"
}
$cursorField = [long]$cursorFrame[0].id - [long]$machines[0].firstFrameSlot
$itemField = [long]$itemFrame[0].id - [long]$machines[0].firstFrameSlot
foreach ( $required in @(
    "if (frame->i$($cursorFrame[0].initBit)) { xlir_xvalue_iterator* xlir_released;",
    "if (frame->i$($itemFrame[0].initBit)) { xlir_string xlir_released;",
    "xlir_async_machine_frame_$($machines[0].targetSymbol)* frame = xlir_async_machine_allocate_$($machines[0].targetSymbol)();",
    "return xl6ContinuationFrameLaunchV1(frame, xlir_continuation_executor, xlir_async_machine_entry_$($machines[0].targetSymbol),",
    "xlir_async_machine_program_$($machines[0].targetSymbol)(frame, NULL, frame->outer, 0);"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "Generated C lost an async frame/launch completion ABI token: $required"
    }
}

Push-Location $Root
try {
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$generatedPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$executablePath`" && `"$executablePath`""
    $runtimeOutput = @(& $env:ComSpec /d /v:on /c $command 2>&1)
    if ( $LASTEXITCODE -ne 0 ) {
        $runtimeOutput | ForEach-Object { Write-Output $_ }
        throw "XLIR xvalue-array iterator strict-C runtime failed"
    }
    $runtimeText = ($runtimeOutput | ForEach-Object { $_.ToString() }) -join "`n"
    if ( ! $runtimeText.Contains("xvalue array iterator projection ok") ) {
        throw "XLIR xvalue-array iterator runtime lost its completion contract"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR xvalue-array iterator gate ok: ordinal/u64/bool/f64/string/xvalue projections, exact TypeIds, snapshot stability, managed cleanup across exits, cursor/item frame ownership across await, launch/resume completion, deterministic mechanical C, strict compile and runtime semantics."
exit 0
