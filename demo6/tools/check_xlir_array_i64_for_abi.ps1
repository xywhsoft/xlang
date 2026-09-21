param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_array_i64_for_abi.xl"
$rejectSource = Join-Path $Root "test\cases\cgen_xlir_array_i64_for_escape_reject.xl"
$compatSource = Join-Path $Root "test\cases\cgen_export_typed_container_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedJsonPath = Join-Path $outputDir "xlir_array_i64_for_abi.typed.json"
$loweredJsonPath = Join-Path $outputDir "xlir_array_i64_for_abi.lowered.json"
$rejectTypedJsonPath = Join-Path $outputDir "xlir_array_i64_for_escape_reject.typed.json"
$rejectCPath = Join-Path $outputDir "xlir_array_i64_for_escape_reject.c"
$cPath = Join-Path $outputDir "xlir_array_i64_for_abi_generated.c"
$compatCPath = Join-Path $outputDir "cgen_export_typed_container_abi_xlir.c"
$exePath = Join-Path $outputDir "xlir_array_i64_for_abi_host.exe"

foreach ( $path in @($xl, $source, $rejectSource, $compatSource, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR array<i64> for ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR array<i64> for typed JSON failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$arrayLoops = @($typed.nodes | Where-Object {
    $_.loopAbiName -like "xvalue-array-i64-*"
})
if ( $arrayLoops.Count -ne 11 -or
     @($arrayLoops | Where-Object {
         $_.loopAbiName -eq "xvalue-array-i64-value" -and
         $_.loopErrorAbiName -eq "no-raise"
     }).Count -ne 10 -or
     @($arrayLoops | Where-Object {
         $_.loopAbiName -eq "xvalue-array-i64-index-value" -and
         $_.loopErrorAbiName -eq "no-raise"
     }).Count -ne 1 ) {
    throw "Typed XLIR array<i64> loop target layout or error effect is not exact"
}
$arrayTypes = @($typed.types | Where-Object {
    $_.kind -eq 6 -and $_.collectionAbiName -eq "array" -and
    $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and
    @($_.params).Count -eq 1
})
$i64Types = @($typed.types | Where-Object { $_.cAbi -eq 3 })
if ( $arrayTypes.Count -ne 1 -or $i64Types.Count -ne 1 -or
     $arrayTypes[0].params[0] -ne $i64Types[0].id ) {
    throw "Typed XLIR array loop is not tied to the exact managed array<i64> ABI"
}
if ( @($arrayLoops | Where-Object {
        $_.loopResourceAbiName -notin @(
            "xvalue-shallow-snapshot-release",
            "xvalue-owned-iterator-release") -or
        $_.loopResourceType -ne $arrayTypes[0].id
    }).Count -ne 0 -or
     @($arrayLoops | Where-Object {
        $_.loopResourceAbiName -eq "xvalue-shallow-snapshot-release"
    }).Count -ne 9 -or
     @($arrayLoops | Where-Object {
        $_.loopResourceAbiName -eq "xvalue-owned-iterator-release"
    }).Count -ne 2 ) {
    throw "Typed XLIR array loop does not carry its exact snapshot resource ABI"
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR array<i64> for lowered JSON failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$loopIds = @($arrayLoops | ForEach-Object { $_.id })
$loopInstructions = @($lowered.instructions | Where-Object {
    $_.origin -in $loopIds
})
$instructionBlock = @{}
$instructionIndex = @{}
foreach ( $block in @($lowered.blocks) ) {
    for ( $index = [long]$block.firstInstruction;
          $index -lt [long]$block.firstInstruction + [long]$block.instructionCount;
          ++$index ) {
        $instruction = $lowered.instructions[$index]
        $instructionBlock[[string]$instruction.id] = [long]$block.id
        $instructionIndex[[string]$instruction.id] = $index
    }
}
$snapshotReleaseCounts = @()
$transactionSuccessors = @()
$transactionFailureSuccessors = @()
$errorUnwindSuccessors = @()
foreach ( $loopId in $loopIds ) {
    $typedLoop = @($arrayLoops | Where-Object { $_.id -eq $loopId })[0]
    $instructions = @($loopInstructions | Where-Object { $_.origin -eq $loopId })
    $clones = @($instructions | Where-Object {
        $_.kindName -eq "runtime-call" -and
        $_.runtimeOpName -eq "xvalue-clone" -and $_.operandCount -eq 1
    })
    $counts = @($instructions | Where-Object {
        $_.kindName -eq "runtime-call" -and
        $_.runtimeOpName -eq "xvalue-count" -and $_.operandCount -eq 1
    })
    $items = @($instructions | Where-Object {
        $_.kindName -eq "runtime-call" -and
        $_.runtimeOpName -eq "xvalue-array-at-i64-integer" -and
        $_.operandCount -eq 2
    })
    $expectedCloneCount = if ( $typedLoop.loopResourceAbiName -eq
        "xvalue-shallow-snapshot-release" ) { 1 } else { 0 }
    if ( $clones.Count -ne $expectedCloneCount -or
         $counts.Count -ne 1 -or $items.Count -ne 1 ) {
        throw "Lowered XLIR array loop does not match its clone-or-transfer/count/typed-item contract: n$loopId"
    }
    $snapshotStores = @($instructions | Where-Object {
        $_.kindName -eq "store" -and $_.type -eq $arrayTypes[0].id -and
        $_.operandCount -eq 1 -and $_.targetSymbol -ge 0xF0000000
    })
    if ( $snapshotStores.Count -ne 1 -or
         ($expectedCloneCount -eq 1 -and
          $snapshotStores[0].operands[0] -ne $clones[0].result) ) {
        throw "Lowered XLIR array loop acquisition is not owned by compiler resource storage: n$loopId"
    }
    $snapshotSymbol = $snapshotStores[0].targetSymbol
    $releases = @($instructions | Where-Object {
        $_.kindName -eq "release" -and $_.type -eq $arrayTypes[0].id -and
        $_.targetSymbol -eq $snapshotSymbol -and $_.operandCount -eq 0
    })
    $loopBranch = @($instructions | Where-Object {
        $_.kindName -eq "branch" -and $_.falseTarget -ne 0
    })
    if ( $loopBranch.Count -ne 1 ) {
        throw "Lowered XLIR array loop has no unique condition branch: n$loopId"
    }
    $exitBlock = [long]$loopBranch[0].falseTarget
    $canonicalReleases = @($releases | Where-Object {
        $instructionBlock[[string]$_.id] -eq $exitBlock
    })
    if ( $canonicalReleases.Count -ne 1 -or $releases.Count -lt 1 ) {
        throw "Lowered XLIR array loop has no unique canonical exit release: n$loopId"
    }
    foreach ( $release in $releases ) {
        $releaseBlock = $instructionBlock[[string]$release.id]
        $block = @($lowered.blocks | Where-Object { $_.id -eq $releaseBlock })
        if ( $block.Count -ne 1 ) {
            throw "Lowered XLIR array loop release does not belong to one block: n$loopId"
        }
        $blockInstructions = @($lowered.instructions |
            Select-Object -Skip ([int]$block[0].firstInstruction) |
            Select-Object -First ([int]$block[0].instructionCount))
        $releaseOffset = [int]$instructionIndex[[string]$release.id] -
            [int]$block[0].firstInstruction
        $terminal = $blockInstructions[-1]
        if ( $terminal.kindName -eq "error-test" ) {
            $beginOffset = -1
            for ( $offset = 0; $offset -lt $releaseOffset; ++$offset ) {
                if ( $blockInstructions[$offset].kindName -eq "cleanup-begin" ) {
                    $beginOffset = $offset
                }
            }
            $captureOffset = $releaseOffset + 1
            while ( $captureOffset -lt $blockInstructions.Count -and
                    $blockInstructions[$captureOffset].kindName -eq "release" ) {
                ++$captureOffset
            }
            if ( $beginOffset -lt 0 -or $captureOffset + 2 -ne $blockInstructions.Count - 1 -or
                 $blockInstructions[$captureOffset].kindName -ne "cleanup-capture" -or
                 $blockInstructions[$captureOffset + 1].kindName -ne "cleanup-end" -or
                 $terminal.trueTarget -eq 0 -or $terminal.falseTarget -eq 0 ) {
                throw "Lowered XLIR array loop release is not enclosed by one complete cleanup transaction: n$loopId"
            }
            for ( $offset = $beginOffset + 1; $offset -lt $captureOffset; ++$offset ) {
                if ( $blockInstructions[$offset].kindName -ne "release" ) {
                    throw "Lowered XLIR array loop cleanup transaction contains a non-release action: n$loopId"
                }
            }
            $successBlock = @($lowered.blocks | Where-Object { $_.id -eq $terminal.trueTarget })
            $failureBlock = @($lowered.blocks | Where-Object { $_.id -eq $terminal.falseTarget })
            if ( $successBlock.Count -ne 1 -or $failureBlock.Count -ne 1 ) {
                throw "Lowered XLIR array loop cleanup transaction has an unresolved CFG successor: n$loopId"
            }
            $successTerminal = $lowered.instructions[
                [int]$successBlock[0].firstInstruction + [int]$successBlock[0].instructionCount - 1]
            $failureTerminal = $lowered.instructions[
                [int]$failureBlock[0].firstInstruction + [int]$failureBlock[0].instructionCount - 1]
            if ( $successTerminal.kindName -notin @("return", "throw", "jump") -or
                 $failureTerminal.kindName -notin @("return", "throw", "jump") ) {
                throw "Lowered XLIR array loop cleanup transaction does not rejoin an explicit CFG transfer: n$loopId"
            }
            $transactionSuccessors += $successTerminal.kindName
            $transactionFailureSuccessors += $failureTerminal.kindName
        }
        elseif ( $terminal.kindName -in @("return", "throw", "jump") ) {
            if ( $blockInstructions.Count -lt 4 -or
                 $blockInstructions[$blockInstructions.Count - 2].kindName -ne "error-unwind-end" ) {
                throw "Lowered XLIR array loop release does not terminate a complete error unwind: n$loopId"
            }
            $endOffset = $blockInstructions.Count - 2
            $beginOffset = -1
            for ( $offset = 0; $offset -lt $releaseOffset; ++$offset ) {
                if ( $blockInstructions[$offset].kindName -eq "error-unwind-begin" ) {
                    $beginOffset = $offset
                }
            }
            if ( $beginOffset -lt 0 ) {
                throw "Lowered XLIR array loop error release has no unwind begin marker: n$loopId"
            }
            for ( $offset = $beginOffset + 1; $offset -lt $endOffset; ++$offset ) {
                if ( $blockInstructions[$offset].kindName -ne "release" ) {
                    throw "Lowered XLIR array loop error unwind contains a non-release action: n$loopId"
                }
            }
            $errorUnwindSuccessors += $terminal.kindName
        }
        else {
            throw "Lowered XLIR array loop release has no verified cleanup/unwind protocol: n$loopId"
        }
    }
    $snapshotReleaseCounts += $releases.Count
}
if ( @($snapshotReleaseCounts | Where-Object { $_ -lt 1 }).Count -ne 0 -or
     @($transactionSuccessors | Where-Object { $_ -eq "return" }).Count -ne 2 -or
     @($transactionFailureSuccessors | Where-Object { $_ -eq "throw" }).Count -lt 1 -or
     @($transactionFailureSuccessors | Where-Object { $_ -eq "jump" }).Count -lt 1 -or
     @($errorUnwindSuccessors | Where-Object { $_ -eq "throw" }).Count -lt 1 -or
     @($errorUnwindSuccessors | Where-Object { $_ -eq "jump" }).Count -ne 2 ) {
    throw "Lowered XLIR array-loop resource exits do not cover return, exception and finally routes exactly"
}

& $xl ir $rejectSource --typed --format json --emit $rejectTypedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic non-container typed JSON failed" }
$rejectedTyped = [System.IO.File]::ReadAllText(
    $rejectTypedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$rejectedLoops = @($rejectedTyped.nodes | Where-Object { $_.kind -eq 19 })
if ( $rejectedLoops.Count -ne 1 -or @($rejectedLoops | Where-Object {
        $_.loopAbiName -ne "xvalue-iterator-value" -or
        $_.loopKeyAbiName -ne "none" -or
        $_.loopItemAbiName -ne "dynamic-logical-xvalue" -or
        $_.loopErrorAbiName -ne "may-raise" -or
        $_.loopResourceAbiName -ne "xvalue-iterator-destroy" -or
        $_.loopResourceType -le 0
    }).Count -ne 0 ) {
    throw "Typed XLIR did not exclude dynamic var from the static array ABI"
}
& $xl cgen $rejectSource --backend xlir --emit $rejectCPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR dynamic non-container checked iterator Cgen failed"
}
$dynamicGenerated = [System.IO.File]::ReadAllText(
    $rejectCPath, [System.Text.Encoding]::UTF8)
if ( ! $dynamicGenerated.Contains("xlir_xvalue_iterator_create_checked") -or
     ! $dynamicGenerated.Contains("for in value must be array, list, set or dict") -or
     $dynamicGenerated.Contains("xlir_xvalue_array_at_i64_integer") ) {
    throw "XLIR non-container source was not routed exclusively to the checked dynamic TypeError ABI"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR array<i64> for Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($generated, "=\s*xrtValueClone\(v\d+\);")).Count -ne 9 -or
     ([regex]::Matches($generated, "=\s*\(int64_t\)xrtValueCount\(v\d+\);")).Count -ne 11 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_array_at_i64_integer\(v\d+, v\d+\);")).Count -ne 12 -or
     ([regex]::Matches($generated, "\bfor\s*\(")).Count -ne 0 -or
     $generated.Contains("loopAbi") ) {
    throw "XLIR array<i64> loop C rendering is not mechanical from verified CFG"
}
foreach ( $loopId in $loopIds ) {
    $clone = @($loopInstructions | Where-Object {
        $_.origin -eq $loopId -and $_.runtimeOpName -eq "xvalue-clone"
    })
    $snapshotStore = @($loopInstructions | Where-Object {
        $_.origin -eq $loopId -and $_.kindName -eq "store" -and
        $_.type -eq $arrayTypes[0].id -and $_.operandCount -eq 1 -and
        $_.targetSymbol -ge 0xF0000000
    })[0]
    $releaseCount = @($loopInstructions | Where-Object {
        $_.origin -eq $loopId -and $_.kindName -eq "release" -and
        $_.targetSymbol -eq $snapshotStore.targetSymbol
    }).Count
    $symbol = $snapshotStore.targetSymbol
    $renderedReleasePattern =
        [regex]::Escape("{ &(xlir_released), &(s$symbol), sizeof(xlir_released) },") +
        "\s*" +
        [regex]::Escape("{ &(s$symbol), &(xvalue*){0}, sizeof(s$symbol) },") +
        "[\s\S]{0,256}?" + [regex]::Escape("xrtValueRelease(xlir_released);")
    $renderedReleaseCount = ([regex]::Matches($generated, $renderedReleasePattern)).Count
    if ( $renderedReleaseCount -ne $releaseCount -or
         $generated.Contains("xrtValueRelease(s$symbol);") ) {
        throw "Generated C does not mechanically preserve every array-loop resource exit: n$loopId"
    }
}

& $xl cgen $compatSource --backend xlir --emit $compatCPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR established exported array<int> parameter loop compatibility Cgen failed"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR array<i64> for strict generated-C runtime failed"
}

Write-Output "XLIR array<i64> for ABI gate ok: exact named and owned-rvalue admission, first-class shallow-snapshot and owned-iterator LoopResourceAbi, clone-or-transfer acquisition, count/index CFG, typed i64 projection, call/return admission, local and outward exception routing, return/throw/finally cleanup, continue/break canonical release, non-container excluded from array ABI and routed to checked dynamic TypeError, mechanical C, strict compile, and runtime snapshot/transfer semantics."
exit 0
