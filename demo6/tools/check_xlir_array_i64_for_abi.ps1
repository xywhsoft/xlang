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
$nonCanonicalReleaseSuccessors = @()
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
    foreach ( $release in @($releases | Where-Object {
            $instructionBlock[[string]$_.id] -ne $exitBlock
        }) ) {
        $releaseBlock = $instructionBlock[[string]$release.id]
        $releaseIndex = [long]$instructionIndex[[string]$release.id]
        $successor = $null
        for ( $candidateIndex = $releaseIndex + 1;
              $candidateIndex -lt $lowered.instructions.Count;
              ++$candidateIndex ) {
            $candidate = $lowered.instructions[$candidateIndex]
            if ( $instructionBlock[[string]$candidate.id] -ne $releaseBlock ) {
                break
            }
            if ( $candidate.kindName -in @("return", "throw", "jump") ) {
                $successor = $candidate
                break
            }
            if ( $candidate.kindName -ne "release" ) {
                break
            }
        }
        if ( $null -eq $successor ) {
            throw "Lowered XLIR array loop escape cleanup sequence does not terminate in its CFG transfer: n$loopId"
        }
        $nonCanonicalReleaseSuccessors += $successor.kindName
    }
    $snapshotReleaseCounts += $releases.Count
}
if ( @($snapshotReleaseCounts | Where-Object { $_ -eq 1 }).Count -ne 6 -or
     @($snapshotReleaseCounts | Where-Object { $_ -eq 2 }).Count -ne 5 -or
     @($nonCanonicalReleaseSuccessors | Where-Object { $_ -eq "return" }).Count -ne 2 -or
     @($nonCanonicalReleaseSuccessors | Where-Object { $_ -eq "throw" }).Count -ne 1 -or
     @($nonCanonicalReleaseSuccessors | Where-Object { $_ -eq "jump" }).Count -ne 2 ) {
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
    $renderedReleaseCount = ([regex]::Matches(
        $generated,
        "xrtValueRelease\(s$($snapshotStore.targetSymbol)\);\s*s$($snapshotStore.targetSymbol) = NULL;"
    )).Count
    if ( $renderedReleaseCount -ne $releaseCount ) {
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
