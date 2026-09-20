param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\tmp"
$generatedDir = Join-Path $Root "build\test"
$cases = @(
    [pscustomobject]@{
        Name = "xlir_async_future_two_awaits"
        Suspensions = 2
        FutureFrames = 1
        ScalarSpills = 1
        ManagedSpills = 0
        Modes = @("", "-DXLIR_CHAIN_SECOND_REJECT")
    },
    [pscustomobject]@{
        Name = "xlir_async_future_three_awaits"
        Suspensions = 3
        FutureFrames = 2
        ScalarSpills = 2
        ManagedSpills = 0
        Modes = @("", "-DXLIR_CHAIN_THIRD_REJECT", "-DXLIR_CHAIN_THIRD_CANCEL", "-DXLIR_CHAIN_THIRD_CLOSE")
    },
    [pscustomobject]@{
        Name = "xlir_async_future_three_awaits_mixed"
        Suspensions = 3
        FutureFrames = 2
        ScalarSpills = 2
        ManagedSpills = 0
        Modes = @("")
    },
    [pscustomobject]@{
        Name = "xlir_async_future_three_strings"
        Suspensions = 3
        FutureFrames = 2
        ScalarSpills = 0
        ManagedSpills = 2
        Modes = @("", "-DXLIR_STRING_THIRD_REJECT", "-DXLIR_STRING_THIRD_CANCEL", "-DXLIR_STRING_THIRD_CLOSE")
    }
)

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    throw "XLIR multi-await machine gate compiler is missing: $xl"
}
New-Item -ItemType Directory -Force -Path $outputDir,$generatedDir | Out-Null

foreach ( $case in $cases ) {
    $source = Join-Path $Root ("test\cases\{0}.xl" -f $case.Name)
    $hostSource = Join-Path $Root ("test\cases\{0}_host.c" -f $case.Name)
    $jsonPath = Join-Path $outputDir ("{0}.multi-await.lowered.json" -f $case.Name)
    $textPath = Join-Path $outputDir ("{0}.multi-await.lowered.txt" -f $case.Name)
    $cPath = Join-Path $generatedDir ("{0}_generated.c" -f $case.Name)
    $repeatPath = Join-Path $outputDir ("{0}.multi-await.repeat.c" -f $case.Name)
    foreach ( $path in @($source, $hostSource) ) {
        if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
            throw "XLIR multi-await machine gate input is missing: $path"
        }
    }

    & $xl ir $source --lowered --format json --emit $jsonPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR multi-await lowered JSON failed: $($case.Name)" }
    & $xl ir $source --lowered --format text --emit $textPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR multi-await lowered text failed: $($case.Name)" }
    & $xl cgen $source --backend xlir --emit $cPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR multi-await C generation failed: $($case.Name)" }
    & $xl cgen $source --backend xlir --emit $repeatPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR multi-await repeated C generation failed: $($case.Name)" }

    $leftBytes = [System.IO.File]::ReadAllBytes($cPath)
    $rightBytes = [System.IO.File]::ReadAllBytes($repeatPath)
    if ( $leftBytes.Length -ne $rightBytes.Length -or
         -not [System.Linq.Enumerable]::SequenceEqual([byte[]]$leftBytes, [byte[]]$rightBytes) ) {
        throw "XLIR multi-await generated C is not deterministic: $($case.Name)"
    }

    $ir = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $machines = @($ir.asyncMachines)
    if ( $machines.Count -ne 1 ) {
        throw "XLIR multi-await source does not have one canonical machine: $($case.Name)"
    }
    $machine = $machines[0]
    $suspensions = @($ir.asyncSuspensions | Select-Object -Skip ([int]$machine.firstSuspension) |
        Select-Object -First ([int]$machine.suspensionCount))
    $frames = @($ir.asyncMachineFrameSlots | Select-Object -Skip ([int]$machine.firstFrameSlot) |
        Select-Object -First ([int]$machine.frameSlotCount))
    $entries = @($ir.asyncMachineEntries | Select-Object -Skip ([int]$machine.firstEntry) |
        Select-Object -First ([int]$machine.entryCount))
    $blocks = @($ir.asyncMachineBlocks | Select-Object -Skip ([int]$machine.firstBlock) |
        Select-Object -First ([int]$machine.blockCount))
    $target = @($ir.callTargets | Where-Object { [int64]$_.asyncMachine -eq [int64]$machine.id })
    if ( $machine.statusName -ne "analyzed" -or
         [int]$machine.suspensionCount -ne [int]$case.Suspensions -or
         $suspensions.Count -ne [int]$case.Suspensions -or
         $target.Count -ne 1 -or
         $entries.Count -ne (1 + 4 * [int]$case.Suspensions) -or
         $entries[0].kindName -ne "launch" -or
         @($blocks | Where-Object { $_.kindName -eq "suspend" }).Count -ne [int]$case.Suspensions -or
         @($blocks | Where-Object { $_.kindName -eq "invalid" }).Count -ne 0 ) {
        throw "XLIR multi-await call-target/machine topology is unresolved: $($case.Name)"
    }

    for ( $state = 0; $state -lt $suspensions.Count; ++$state ) {
        $suspension = $suspensions[$state]
        $success = $entries[1 + 4 * $state]
        $failure = $entries[2 + 4 * $state]
        $outcomes = @('resume-success', 'resume-failure', 'resume-cancelled', 'resume-closed')
        for ($outcome = 0; $outcome -lt 4; ++$outcome) {
            $entry = $entries[1 + 4 * $state + $outcome]
            if ($entry.kindName -ne $outcomes[$outcome] -or
                $entry.machine -ne $success.machine -or
                $entry.suspension -ne $suspension.id -or $entry.block -ne $suspension.block -or
                $entry.firstInstruction -ne (1 + [int64]$suspension.awaitInstruction) -or
                $entry.programBlock -ne $success.programBlock -or
                $ir.instructions[[int]$suspension.awaitInstruction].futureOutcomeAbi -ne 1) {
                throw 'All four Future outcomes must enter the verified post-AWAIT fragment'
            }
        }
        $suspendBlock = @($blocks | Where-Object {
            $_.kindName -eq "suspend" -and [int64]$_.suspension -eq [int64]$suspension.id
        })
        if ( [int]$suspension.stateId -ne $state -or
             $success.kindName -ne "resume-success" -or
             $failure.kindName -ne "resume-failure" -or
             [int64]$success.suspension -ne [int64]$suspension.id -or
             [int64]$failure.suspension -ne [int64]$suspension.id -or
             [int64]$success.programBlock -ne [int64]$failure.programBlock -or
             $suspendBlock.Count -ne 1 -or
             ([int64]$suspendBlock[0].firstInstruction + [int64]$suspendBlock[0].instructionCount - 1) -ne
                [int64]$suspension.awaitInstruction ) {
            throw "XLIR multi-await suspension entry/program split is not canonical: $($case.Name) state $state"
        }
        $live = @($ir.asyncLiveSlots | Select-Object -Skip ([int]$suspension.firstLiveSlot) |
            Select-Object -First ([int]$suspension.liveSlotCount))
        foreach ( $slot in $live ) {
            if ( [int64]$slot.frameSlot -lt [int64]$machine.firstFrameSlot -or
                 [int64]$slot.frameSlot -ge ([int64]$machine.firstFrameSlot + [int64]$machine.frameSlotCount) ) {
                throw "XLIR multi-await suspension live-set escapes its canonical frame: $($case.Name) state $state"
            }
        }
    }

    $futureFrames = @($frames | Where-Object {
        $_.residenceName -eq "launch-parameter" -and
        $_.transferAbiName -eq "retain-future" -and
        $_.destroyAbiName -eq "destroy-future" -and $_.hasInitBit -eq 1
    })
    $scalarSpills = @($frames | Where-Object {
        $_.residenceName -eq "ssa-spill" -and
        $_.transferAbiName -eq "copy-value" -and
        $_.destroyAbiName -eq "none" -and $_.hasInitBit -eq 0
    })
    $managedSpills = @($frames | Where-Object {
        $_.residenceName -eq "ssa-spill" -and
        $_.transferAbiName -eq "copy-string" -and
        $_.destroyAbiName -eq "free-string" -and $_.hasInitBit -eq 1
    })
    if ( $futureFrames.Count -ne [int]$case.FutureFrames -or
         $scalarSpills.Count -ne [int]$case.ScalarSpills -or
         $managedSpills.Count -ne [int]$case.ManagedSpills -or
         @($frames | Where-Object {
             $_.transferAbiName -eq "invalid" -or $_.destroyAbiName -eq "invalid"
         }).Count -ne 0 ) {
        throw "XLIR multi-await frame transfer/destroy ABI is unresolved: $($case.Name)"
    }
    # Managed binary operands already have explicit owners before the later
    # await. Their SSA loads are borrowed observations, not additional owners
    # that can be moved away from the storage cleanup path.
    $operandFrames = @($frames | Where-Object {
        $_.residenceName -eq "frame-storage" -and $_.transferAbiName -eq "none" -and
        $_.destroyAbiName -eq "free-string" -and $_.hasInitBit -eq 1
    })
    if ( $operandFrames.Count -ne [int]$case.ManagedSpills ) {
        throw "Multi-await managed observations lost their explicit operand owner frames"
    }
    foreach ( $spill in $managedSpills ) {
        $load = @($ir.instructions | Where-Object { $_.result -eq $spill.value })[0]
        $owner = @($operandFrames | Where-Object {
            $_.storageSymbol -eq $load.targetSymbol -and $_.type -eq $load.type
        })
        if ( $load.kindName -ne "load" -or $owner.Count -ne 1 -or $spill.type -ne $load.type -or
             @($ir.asyncFrameActions | Where-Object {
                 $_.frameSlot -eq $spill.id -and $_.kindName -like 'transfer-*'
             }).Count -ne 0 ) {
            throw "Multi-await borrowed spill does not name its exact operand owner"
        }
        foreach ( $state in @($suspensions | Where-Object {
            $_.stateId -ge $spill.firstLiveState -and $_.stateId -le $spill.lastLiveState
        }) ) {
            foreach ( $blockId in @($state.successBlock, $state.failureBlock) ) {
                $block = @($ir.blocks | Where-Object { $_.id -eq $blockId })[0]
                $releases = @($ir.instructions | Where-Object {
                    $_.index -ge $block.firstInstruction -and
                    $_.index -lt $block.firstInstruction + $block.instructionCount -and
                    $_.kindName -eq "release" -and $_.targetSymbol -eq $owner[0].storageSymbol -and
                    $_.type -eq $owner[0].type
                })
                if ( $releases.Count -ne 1 ) { throw "Multi-await success/failure lost its managed operand cleanup" }
            }
        }
    }

    $text = [System.IO.File]::ReadAllText($textPath, [System.Text.Encoding]::UTF8)
    $c = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
    $callbackDefinitions = [regex]::Matches(
        $c, 'static void xlir_async_machine_resume_\d+_\d+\(const xfutureresult\* input').Count
    if ( -not $text.Contains("async-machine") -or
         $callbackDefinitions -ne [int]$case.Suspensions -or
         -not $c.Contains("xlir_async_machine_frame_") -or
         -not $c.Contains("xrtFutureWatchAdd(source, &frame->watch)") -or
         -not $c.Contains("next->outer = frame->outer; frame->outer = NULL") -or
         -not $c.Contains("next->cancelObserved = frame->cancelObserved") -or
         $c.Contains("xlir_async_chain_frame_") -or
         $c.Contains("xlir_async_capture_chain_frame_") -or
         $c.Contains("xrtTaskSubmit(") ) {
        throw "XLIR multi-await Cgen did not use the canonical single-layout activation handoff: $($case.Name)"
    }
}

$commands = @("call test\suites\common.bat")
foreach ( $case in $cases ) {
    $modeIndex = 0
    foreach ( $mode in $case.Modes ) {
        $suffix = if ( $mode -eq "" ) { "success" } else { "mode$modeIndex" }
        $modeArgument = if ( $mode -eq "" ) { "" } else { "$mode " }
        $commands += ("!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror {0}test\cases\{1}_host.c !XL6_CGEN_LINKS! -o build\tmp\{1}.multi-await.{2}.exe" -f
            $modeArgument, $case.Name, $suffix)
        $commands += ("build\tmp\{0}.multi-await.{1}.exe" -f $case.Name, $suffix)
        ++$modeIndex
    }
}

Push-Location $Root
try {
    & $env:ComSpec /d /v:on /c ($commands -join " && ")
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR multi-await strict-C/runtime terminal-state propagation gate failed"
    }
} finally {
    Pop-Location
}

Write-Host "XLIR async multi-await machine gate ok: direct scalar/managed two/three-await machine CFG, canonical single-layout activation handoff, per-suspension live-set transfer, exact managed operand owners and borrowed spill copies with success/failure cleanup, legacy async-chain renderer forbidden, deterministic C, -Werror and runtime terminal-state propagation."
