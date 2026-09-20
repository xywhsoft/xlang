param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$managedSource = Join-Path $Root "test\cases\xlir_protocol_iterator_managed_item.xl"
$managedHost = Join-Path $Root "test\cases\xlir_protocol_iterator_managed_item_host.c"
$nativeSource = Join-Path $Root "test\cases\extlib_native_iter_protocol_min.xl"
$nativeHost = Join-Path $Root "test\cases\xlir_protocol_iterator_native_host.c"
$scalarSource = Join-Path $Root "test\cases\cgen_protocol_iter.xl"
$legacyHost = Join-Path $Root "test\cases\xlir_protocol_iterator_legacy_host.c"
$fileProbeSource = Join-Path $Root "test\cases\stdlib_buffer_file_runtime.xl"
$outputDir = Join-Path $Root "build\test\xlir_protocol_iterator_abi"

foreach ( $path in @(
    $xl, $managedSource, $managedHost, $nativeSource, $nativeHost,
    $scalarSource, $legacyHost, $fileProbeSource
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR protocol iterator ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl {
    param([string[]]$Arguments, [string]$Label)
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR protocol iterator $Label failed"
    }
}

function Assert-SourceClosedOnce {
    param($Ir, [long]$Entry, [long]$Symbol)
    $pending = New-Object 'System.Collections.Generic.List[object]'
    $seen = New-Object 'System.Collections.Generic.HashSet[string]'
    $pending.Add(@($Entry, 0))
    $terminals = 0
    while ($pending.Count -ne 0) {
        $state = $pending[$pending.Count - 1]
        $pending.RemoveAt($pending.Count - 1)
        $blockId = [long]$state[0]
        $closed = [int]$state[1]
        if (!$seen.Add("${blockId}:$closed")) { continue }
        if ($blockId -le 0 -or $blockId -gt $Ir.blocks.Count) {
            throw 'Protocol source lifetime reached an invalid block'
        }
        $block = $Ir.blocks[$blockId - 1]
        if ($block.instructionCount -eq 0) { throw 'Protocol source lifetime reached an empty block' }
        $end = [long]$block.firstInstruction + [long]$block.instructionCount
        for ($index = [long]$block.firstInstruction; $index -lt $end; ++$index) {
            $instruction = $Ir.instructions[$index]
            if ($instruction.kindName -ne 'release' -or [long]$instruction.targetSymbol -ne $Symbol) { continue }
            ++$closed
            if ($closed -ne 1 -or $instruction.errorScope -le 0 -or
                $instruction.errorScope -gt $Ir.instructions.Count -or
                $Ir.instructions[$instruction.errorScope - 1].kindName -notin @('cleanup-begin', 'error-unwind-begin')) {
                throw 'Protocol source was released twice or outside its outcome-preserving cleanup scope'
            }
        }
        $last = $Ir.instructions[$end - 1]
        if ($last.kindName -in @('return', 'throw')) {
            if ($closed -ne 1) { throw 'Protocol source escaped a terminal without exactly one release' }
            ++$terminals
            continue
        }
        $successors = @(@($last.trueTarget, $last.falseTarget) | Where-Object { $_ -gt 0 } | Select-Object -Unique)
        if ($successors.Count -eq 0) { throw 'Protocol source lifetime has an unaccounted control-flow exit' }
        foreach ($successor in $successors) { $pending.Add(@([long]$successor, $closed)) }
    }
    if ($terminals -eq 0) { throw 'Protocol source lifetime has no reachable terminal proof' }
}

function Emit-ProtocolCase {
    param([string]$Name, [string]$Source, [bool]$Legacy)
    $typedPath = Join-Path $outputDir "$Name.typed.json"
    $loweredPath = Join-Path $outputDir "$Name.lowered.json"
    $cPath = Join-Path $outputDir "$Name.xlir.c"
    $repeatPath = Join-Path $outputDir "$Name.xlir.repeat.c"
    Invoke-Xl @("ir", $Source, "--format", "json", "--emit", $typedPath) "$Name typed JSON"
    Invoke-Xl @("ir", $Source, "--lowered", "--format", "json", "--emit", $loweredPath) "$Name lowered JSON"
    Invoke-Xl @("cgen", $Source, "--backend", "xlir", "--emit", $cPath) "$Name Cgen"
    Invoke-Xl @("cgen", $Source, "--backend", "xlir", "--emit", $repeatPath) "$Name repeat Cgen"
    if ( ! [System.Linq.Enumerable]::SequenceEqual(
            [byte[]][System.IO.File]::ReadAllBytes($cPath),
            [byte[]][System.IO.File]::ReadAllBytes($repeatPath)
        ) ) {
        throw "XLIR protocol iterator $Name C is not deterministic"
    }
    $legacyPath = ""
    if ( $Legacy ) {
        $legacyPath = Join-Path $outputDir "$Name.legacy.c"
        Invoke-Xl @("cgen", $Source, "--backend", "legacy", "--emit", $legacyPath) "$Name legacy Cgen"
    }
    return [pscustomobject]@{
        Name = $Name
        Source = $Source
        TypedPath = $typedPath
        LoweredPath = $loweredPath
        CPath = $cPath
        LegacyPath = $legacyPath
        Typed = ([System.IO.File]::ReadAllText(
            $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json)
        Lowered = ([System.IO.File]::ReadAllText(
            $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json)
    }
}

function Assert-ProtocolLoops {
    param(
        $Case,
        [int]$ExpectedCount,
        [int]$ExpectedIndexedCount,
        [string]$ExpectedItemAbi
    )
    $loops = @($Case.Typed.nodes | Where-Object {
        $_.loopAbiName -eq "protocol-iterator-value" -or
        $_.loopAbiName -eq "protocol-iterator-index-value"
    })
    if ( $loops.Count -ne $ExpectedCount -or
         @($loops | Where-Object {
             $_.loopAbiName -eq "protocol-iterator-index-value"
         }).Count -ne $ExpectedIndexedCount ) {
        throw "Typed XLIR $($Case.Name) has no exact protocol iterator value/index ABI set"
    }
    foreach ( $loop in $loops ) {
        if ( $loop.loopItemAbiName -ne $ExpectedItemAbi -or
             $loop.loopResourceAbiName -ne "protocol-iterator-release" -or
             $loop.loopErrorAbiName -eq "invalid" -or
             $loop.loopProtocolIterTarget -eq 0 -or
             $loop.loopProtocolNextTarget -eq 0 -or
             $loop.loopProtocolIterableReceiverType -eq 0 -or
             $loop.loopProtocolIteratorReceiverType -ne $loop.loopProtocolIteratorType -or
             $loop.loopProtocolIteratorType -ne $loop.loopResourceType -or
             $loop.loopProtocolNextResultType -eq 0 -or
             $loop.loopProtocolItemType -eq 0 -or
             $loop.PSObject.Properties.Name -contains "loopResourceDestroySymbol" ) {
            throw "Typed XLIR $($Case.Name) loop n$($loop.id) has an incomplete protocol contract"
        }
        $iterTargets = @($Case.Lowered.callTargets | Where-Object {
            $_.symbol -eq $loop.loopProtocolIterTarget
        })
        $nextTargets = @($Case.Lowered.callTargets | Where-Object {
            $_.symbol -eq $loop.loopProtocolNextTarget
        })
        if ( $iterTargets.Count -ne 1 -or $nextTargets.Count -ne 1 -or
             $iterTargets[0].kind -ne 1 -or $nextTargets[0].kind -ne 1 -or
             $iterTargets[0].returnType -ne $loop.loopProtocolIteratorType -or
             $nextTargets[0].returnType -ne $loop.loopProtocolNextResultType -or
             $iterTargets[0].parameterCount -ne 1 -or
             $nextTargets[0].parameterCount -ne 1 -or
             @($iterTargets[0].parameterTypes).Count -ne 1 -or
             @($nextTargets[0].parameterTypes).Count -ne 1 -or
             $iterTargets[0].parameterTypes[0] -ne $loop.loopProtocolIterableReceiverType -or
             $nextTargets[0].parameterTypes[0] -ne $loop.loopProtocolIteratorReceiverType -or
             $iterTargets[0].parameterAbis[0] -ne 2 -or
             $nextTargets[0].parameterAbis[0] -ne 2 ) {
            throw "Lowered XLIR $($Case.Name) loop n$($loop.id) has no exact borrowed direct CALL targets"
        }
        $origin = @($Case.Lowered.instructions | Where-Object {
            $_.origin -eq $loop.id
        })
        if ( @($origin | Where-Object {
                 $_.kindName -eq "call" -and
                 $_.targetSymbol -eq $loop.loopProtocolIterTarget
             }).Count -ne 1 -or
             @($origin | Where-Object {
                 $_.kindName -eq "call" -and
                 $_.targetSymbol -eq $loop.loopProtocolNextTarget
             }).Count -ne 1 -or
             @($origin | Where-Object { $_.kindName -eq "product-get" }).Count -lt 1 ) {
            throw "Lowered XLIR $($Case.Name) loop n$($loop.id) did not materialize iter/next/product CFG"
        }
        if ( $ExpectedItemAbi -eq "protocol-product-take" ) {
            if ( @($origin | Where-Object {
                     $_.kindName -eq "product-take" -and
                     $_.type -eq $loop.loopProtocolItemType
                 }).Count -ne 1 -or
                 @($origin | Where-Object {
                     $_.kindName -eq "release" -and
                     $_.type -eq $loop.loopProtocolItemType
                 }).Count -lt 1 ) {
                throw "Lowered XLIR $($Case.Name) loop n$($loop.id) lost managed product TAKE/release"
            }
        } else {
            if ( @($origin | Where-Object {
                     $_.kindName -eq "product-get" -and
                     $_.type -eq $loop.loopProtocolItemType
                 }).Count -ne 1 ) {
                throw "Lowered XLIR $($Case.Name) loop n$($loop.id) lost trivial product projection"
            }
        }
        if ( @($origin | Where-Object {
                 $_.kindName -eq "release" -and
                 $_.type -eq $loop.loopProtocolIteratorType
             }).Count -lt 1 ) {
            throw "Lowered XLIR $($Case.Name) loop n$($loop.id) lost iterator release"
        }
    }
    return $loops
}

$scalar = Emit-ProtocolCase "scalar" $scalarSource $true
$managed = Emit-ProtocolCase "managed" $managedSource $true
$native = Emit-ProtocolCase "native" $nativeSource $true
$fileProbe = Emit-ProtocolCase "buffer_file_probe" $fileProbeSource $false

$scalarLoops = Assert-ProtocolLoops $scalar 2 1 "protocol-product-value"
$managedLoops = Assert-ProtocolLoops $managed 7 1 "protocol-product-take"
$nativeLoops = Assert-ProtocolLoops $native 1 0 "protocol-product-value"
$fileLoops = Assert-ProtocolLoops $fileProbe 1 0 "protocol-product-value"

$managedItem = @($managed.Typed.types | Where-Object {
    $_.id -eq $managedLoops[0].loopProtocolItemType
})
$managedIterator = @($managed.Typed.types | Where-Object {
    $_.id -eq $managedLoops[0].loopProtocolIteratorType
})
$nativeIterator = @($native.Typed.types | Where-Object {
    $_.id -eq $nativeLoops[0].loopProtocolIteratorType
})
$nativeIterable = @($native.Typed.types | Where-Object {
    $_.id -eq $nativeLoops[0].loopProtocolIterableReceiverType
})
if ( $managedItem.Count -ne 1 -or $managedItem[0].cAbi -ne 25 -or
     $managedItem[0].ownershipAbi -ne 2 -or
     $managedIterator.Count -ne 1 -or $managedIterator[0].cAbi -ne 26 -or
     $managedIterator[0].ownershipAbi -ne 2 -or
     $nativeIterator.Count -ne 1 -or $nativeIterator[0].cAbi -ne 26 -or
     $nativeIterator[0].ownershipAbi -ne 2 -or
     $nativeIterable.Count -ne 1 -or $nativeIterable[0].cAbi -ne 26 -or
     $nativeIterable[0].ownershipAbi -ne 2 ) {
    throw "Protocol iterator fixture types do not prove managed string, class, and iterator ownership ABIs"
}

$ownedSourceLoops = @($managedLoops | Where-Object {
    $loop = $_
    @($managed.Typed.nodes | Where-Object {
        $_.parent -eq $loop.id -and $_.kindName -eq "call" -and
        $_.valueCategoryName -eq "rvalue" -and $_.ownershipName -eq "move"
    }).Count -eq 1
})
if ( $ownedSourceLoops.Count -ne 1 ) {
    throw "Typed XLIR managed protocol fixture lost its owned temporary source"
}
$ownedLoop = $ownedSourceLoops[0]
$ownedOrigin = @($managed.Lowered.instructions | Where-Object {
    $_.origin -eq $ownedLoop.id
})
$iterCall = @($ownedOrigin | Where-Object {
    $_.kindName -eq "call" -and
    $_.targetSymbol -eq $ownedLoop.loopProtocolIterTarget
})
if ( $iterCall.Count -ne 1 -or @($iterCall[0].operands).Count -ne 1 ) {
    throw "Lowered XLIR owned protocol source has no unique borrowed __iter__ call"
}
$sourceLoad = @($ownedOrigin | Where-Object {
    $_.result -eq $iterCall[0].operands[0] -and $_.kindName -eq "load" -and
    [long]$_.targetSymbol -ge 4026531840L
})
if ( $sourceLoad.Count -ne 1 ) {
    throw "Lowered XLIR owned protocol source was not materialized as a hidden borrow"
}
$sourceTemporary = [long]$sourceLoad[0].targetSymbol
if ( @($ownedOrigin | Where-Object {
         $_.kindName -eq "store" -and
         [long]$_.targetSymbol -eq $sourceTemporary
     }).Count -ne 1 ) {
    throw "Lowered XLIR owned protocol source has no unique owner initialization"
}
$outcome = $managed.Lowered.instructions[$iterCall[0].id]
if ($outcome.kindName -ne 'error-test' -or $outcome.targetSymbol -ne $iterCall[0].targetSymbol -or
    @($outcome.operands).Count -ne 1 -or $outcome.operands[0] -ne $iterCall[0].result -or
    $outcome.trueTarget -le 0 -or $outcome.falseTarget -le 0 -or $outcome.trueTarget -eq $outcome.falseTarget) {
    throw 'Protocol __iter__ outcome must branch before caller-source cleanup'
}
# Success and failure contain distinct releases, but every reachable terminal
# must cross exactly one. Static instruction counts cannot prove that contract.
Assert-SourceClosedOnce $managed.Lowered $outcome.trueTarget $sourceTemporary
Assert-SourceClosedOnce $managed.Lowered $outcome.falseTarget $sourceTemporary

function Assert-RejectedSourceLifetime {
    try {
        Assert-SourceClosedOnce $managed.Lowered $outcome.trueTarget $sourceTemporary
        Assert-SourceClosedOnce $managed.Lowered $outcome.falseTarget $sourceTemporary
    } catch { return }
    throw 'Protocol source lifetime proof accepted a missing, unisolated or repeated release'
}
$sourceReleases = @($managed.Lowered.instructions | Where-Object {
    $_.kindName -eq 'release' -and [long]$_.targetSymbol -eq $sourceTemporary
})
foreach ($release in $sourceReleases) {
    $savedKind = $release.kindName
    try { $release.kindName = 'invalid-omitted-release'; Assert-RejectedSourceLifetime }
    finally { $release.kindName = $savedKind }
    $savedScope = $release.errorScope
    try { $release.errorScope = 0; Assert-RejectedSourceLifetime }
    finally { $release.errorScope = $savedScope }
}
$successBlock = $managed.Lowered.blocks[$outcome.trueTarget - 1]
$successEnd = $managed.Lowered.instructions[$successBlock.firstInstruction + $successBlock.instructionCount - 1]
$savedEdge = @($successEnd.kindName, $successEnd.trueTarget, $successEnd.falseTarget)
try {
    $successEnd.kindName = 'jump'; $successEnd.trueTarget = $outcome.trueTarget; $successEnd.falseTarget = 0
    Assert-RejectedSourceLifetime
} finally {
    $successEnd.kindName = $savedEdge[0]; $successEnd.trueTarget = $savedEdge[1]; $successEnd.falseTarget = $savedEdge[2]
}
Write-Host "Protocol source lifetime: both call outcomes close once; $($sourceReleases.Count * 2 + 1) missing/unisolated/repeated-release mutations rejected"

$fileTell = @($fileProbe.Lowered.callTargets | Where-Object {
    $_.cName -eq "xl6_xrt_file_tell"
})
if ( $fileTell.Count -ne 1 -or $fileTell[0].kind -ne 2 -or
     $fileTell[0].parameterCount -ne 1 -or
     $fileTell[0].parameterAbis[0] -ne 2 ) {
    throw "stdlib_buffer_file_runtime.xl extern property is not one EXTERN_C borrowed-receiver target"
}

foreach ( $case in @($scalar, $managed, $native, $fileProbe) ) {
    $generated = [System.IO.File]::ReadAllText(
        $case.CPath, [System.Text.Encoding]::UTF8)
    foreach ( $forbidden in @(
        "__iter__", "__next__", "loopProtocol", "loopItemAbi",
        "XL6_HIR", "xl6Hir"
    ) ) {
        if ( $generated.Contains($forbidden) ) {
            throw "Generated $($case.Name) C recovered source/semantic protocol metadata: $forbidden"
        }
    }
}

$scalarXlirExe = Join-Path $outputDir "scalar.xlir.exe"
$managedXlirExe = Join-Path $outputDir "managed.xlir.exe"
$nativeXlirExe = Join-Path $outputDir "native.xlir.exe"
$legacyHostObject = Join-Path $outputDir "legacy.host.o"
$compileCommands = @(
    "call test\suites\common.bat",
    ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include "{0}" "{1}" !XL6_CGEN_LINKS! -o "{2}"' -f $scalar.CPath, $managedHost, $scalarXlirExe),
    ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include "{0}" "{1}" !XL6_CGEN_LINKS! -o "{2}"' -f $managed.CPath, $managedHost, $managedXlirExe),
    ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -DXLIR_GENERATED_OWNS_BUFFER_RUNTIME -include "{0}" "{1}" !XL6_CGEN_LINKS! -o "{2}"' -f $native.CPath, $nativeHost, $nativeXlirExe),
    ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -ffunction-sections -c "{0}" -o "{1}"' -f $legacyHost, $legacyHostObject)
)
foreach ( $case in @($scalar, $managed, $native) ) {
    $legacyObject = Join-Path $outputDir "$($case.Name).legacy.o"
    $legacyExe = Join-Path $outputDir "$($case.Name).legacy.exe"
    $compileCommands += ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -ffunction-sections -Dmain=xlir_legacy_generated_main -c "{0}" -o "{1}"' -f $case.LegacyPath, $legacyObject)
    $compileCommands += ('!XL6_CGEN_CC! -Wl,--gc-sections "{0}" "{1}" !XL6_CGEN_LINKS! -o "{2}"' -f $legacyObject, $legacyHostObject, $legacyExe)
}

Push-Location $Root
try {
    & $env:ComSpec /d /v:on /c ($compileCommands -join " && ")
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Protocol iterator strict generated-C/legacy compile failed"
    }
    foreach ( $name in @("scalar", "managed", "native") ) {
        $xlirExe = Join-Path $outputDir "$name.xlir.exe"
        $legacyExe = Join-Path $outputDir "$name.legacy.exe"
        $legacyOutput = @(& $legacyExe 2>&1)
        $legacyExit = $LASTEXITCODE
        $xlirOutput = @(& $xlirExe 2>&1)
        $xlirExit = $LASTEXITCODE
        $legacyText = ($legacyOutput -join "`n").Trim()
        $xlirText = ($xlirOutput -join "`n").Trim()
        if ( $legacyExit -ne 0 -or $xlirExit -ne 0 -or
             $legacyText -ne $xlirText ) {
            throw "Protocol iterator legacy differential runtime failed for ${name}: legacy=$legacyExit '$legacyText', XLIR=$xlirExit '$xlirText'"
        }
    }
} finally {
    Pop-Location
}

# The managed case deliberately covers normal exhaustion plus
# continue/break/return/throw cleanup. Buffer also proves that standard-library
# extern properties remain exact borrowed call targets after its managed-class
# migration.
Write-Output "XLIR protocol iterator ABI gate ok: scalar and managed product projection, stable-borrow and owned-temporary iterable acquisition, native-backed-class-managed-iterator-release, no language-level native cursor split, managed iterator/item ownership, standard-library extern property classification, direct CALL targets, deterministic mechanical C, -Werror, and legacy differential runtime."
exit 0
