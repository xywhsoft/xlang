param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_async_generic_expression_planner.xl"
$hostSource = Join-Path $Root "test\cases\xlir_async_generic_expression_planner_host.c"
$irHeader = Join-Path $Root "src\ir\xl6_ir.h"
$irSource = Join-Path $Root "src\ir\xl6_ir.c"
$lowerSource = Join-Path $Root "src\ir\xl6_ir_lower.c"
$cgenSource = Join-Path $Root "src\cgen\xl6_ir_cgen.c"
$outputDir = Join-Path $Root "build\tmp"
$generatedDir = Join-Path $Root "build\test"
$jsonPath = Join-Path $outputDir "xlir_async_generic_expression_planner.lowered.json"
$textPath = Join-Path $outputDir "xlir_async_generic_expression_planner.lowered.txt"
$cPath = Join-Path $generatedDir "xlir_async_generic_expression_planner_generated.c"
$cRepeatPath = Join-Path $outputDir "xlir_async_generic_expression_planner.repeat.c"
$executable = Join-Path $outputDir "xlir_async_generic_expression_planner.exe"

foreach ( $path in @($xl, $source, $hostSource, $irHeader, $irSource, $lowerSource, $cgenSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR generic async planner gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir,$generatedDir | Out-Null

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic async planner lowered JSON failed" }
& $xl ir $source --lowered --format text --emit $textPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic async planner lowered text failed" }
& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic async planner C generation failed" }
& $xl cgen $source --backend xlir --emit $cRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic async planner repeated C generation failed" }

$ir = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$asyncTargets = @($ir.callTargets | Where-Object { $_.kind -eq 4 })
$asyncWorkers = @($ir.functions | Where-Object { $_.kindName -eq "async-worker" })
$machines = @($ir.asyncMachines)
$suspensions = @($ir.asyncSuspensions)
$machineFrameSlots = @($ir.asyncMachineFrameSlots)
$liveSlots = @($ir.asyncLiveSlots)
$frameActions = @($ir.asyncFrameActions)
$machineEntries = @($ir.asyncMachineEntries)
$machineBlocks = @($ir.asyncMachineBlocks)
foreach ( $removedProperty in @(
    "asyncContinuationCount", "asyncFrameSlotCount", "asyncResumeOpCount",
    "asyncContinuations", "asyncFrameSlots", "asyncResumeOps"
) ) {
    if ( $ir.PSObject.Properties.Name -contains $removedProperty ) {
        throw "XLIR lowered JSON still exposes removed async compatibility data: $removedProperty"
    }
}
if ( $asyncTargets.Count -ne 3 -or
     @($asyncTargets | Where-Object { $_.asyncDispatch -ne 1 }).Count -ne 0 -or
     $asyncWorkers.Count -ne 3 -or
     @($ir.functions | Where-Object { $_.kindName -eq "async-cfg" }).Count -ne 0 -or
     @($asyncWorkers | Where-Object { $_.reachability -ne "transitive" }).Count -ne 0 -or
     $machines.Count -ne 3 -or $suspensions.Count -ne 3 -or
     @($machines | Where-Object { $_.statusName -ne "analyzed" -or $_.suspensionCount -ne 1 }).Count -ne 0 -or
     @($machineFrameSlots | Where-Object { $_.transferAbiName -eq "invalid" -or $_.destroyAbiName -eq "invalid" }).Count -ne 0 -or
     @($machineFrameSlots | Where-Object { $_.residenceName -eq "invalid" -or
         ($_.destroyAbiName -eq "none" -and $_.hasInitBit -ne 0) -or
         ($_.destroyAbiName -ne "none" -and $_.hasInitBit -ne 1) }).Count -ne 0 -or
     @($liveSlots | Where-Object { [int64]$_.frameSlot -lt 0 -or [int64]$_.frameSlot -ge $machineFrameSlots.Count }).Count -ne 0 -or
     @($frameActions | Where-Object { $_.kindName -eq "invalid" }).Count -ne 0 -or
     @($machineEntries | Where-Object { $_.kindName -eq "invalid" }).Count -ne 0 ) {
    throw "XLIR canonical async machine analysis did not cover all three planner-free workers"
}
foreach ( $machine in $machines ) {
    $machineTarget = @($asyncTargets | Where-Object { $_.symbol -eq $machine.targetSymbol })
    $machineFrames = @($machineFrameSlots | Select-Object -Skip ([int]$machine.firstFrameSlot) |
        Select-Object -First ([int]$machine.frameSlotCount))
    $machineActions = @($frameActions | Select-Object -Skip ([int]$machine.firstAction) |
        Select-Object -First ([int]$machine.actionCount))
    $transfers = @($machineActions | Where-Object { $_.kindName -eq 'transfer-before-instruction' })
    if ($transfers.Count -ne [int]$machine.transferActionCount -or
        ($transfers.Count -gt 0 -and [uint64]$transfers[0].id -ne [uint64]$machine.firstTransferAction)) {
        throw 'Async transfer actions must have an exact machine-owned slice'
    }
    foreach ($transfer in $transfers) {
        if ([uint64]$transfer.instruction -ge [uint64]$ir.instructions.Count -or
            $machineFrameSlots[[int]$transfer.frameSlot].transferAbiName -ne 'move-owned' -or
            $transfer.value -eq 0) {
            throw 'Async transfer actions must name an owned resident and exact consumer operand'
        }
    }
    $entries = @($machineEntries | Select-Object -Skip ([int]$machine.firstEntry) |
        Select-Object -First ([int]$machine.entryCount))
    $blocks = @($machineBlocks | Select-Object -Skip ([int]$machine.firstBlock) |
        Select-Object -First ([int]$machine.blockCount))
    if ( $ir.functions[[int]$machine.function].kindName -ne "async-worker" -or
         $machineTarget.Count -ne 1 -or
         [int64]$machineTarget[0].asyncMachine -ne [int64]$machine.id -or
         [int]$machine.initBitCount -ne @($machineFrames | Where-Object { $_.hasInitBit -eq 1 }).Count -or
         [int]$machine.destroyActionCount -ne [int]$machine.initBitCount -or
         $machineActions.Count -ne [int]$machine.actionCount -or
         $entries.Count -ne (1 + 4 * [int]$machine.suspensionCount) -or
         $entries[0].kindName -ne "launch" -or
         [uint64]$entries[0].suspension -lt [uint64]$suspensions.Count -or
         $entries[0].block -ne $machine.entryBlock -or
         $entries[0].programBlock -lt $machine.firstBlock -or
         $entries[0].programBlock -ge ([int64]$machine.firstBlock + [int64]$machine.blockCount) -or
         $blocks.Count -ne ([int]$ir.functions[[int]$machine.function].blockCount + [int]$machine.suspensionCount) -or
         @($blocks | Where-Object { $_.kindName -eq "suspend" }).Count -ne [int]$machine.suspensionCount -or
         @($blocks | Where-Object { $_.kindName -eq "invalid" }).Count -ne 0 -or
         @($entries | Where-Object { $_.machine -ne $machine.id }).Count -ne 0 -or
         @($blocks | Where-Object { $_.machine -ne $machine.id }).Count -ne 0 ) {
        throw "XLIR async machine analysis is not sourced from its canonical lowered worker CFG"
    }
    $machineSuspensions = @($suspensions | Select-Object -Skip ([int]$machine.firstSuspension) |
        Select-Object -First ([int]$machine.suspensionCount))
    for ( $stateIndex = 0; $stateIndex -lt $machineSuspensions.Count; ++$stateIndex ) {
        $state = $machineSuspensions[$stateIndex]
        $success = $entries[1 + 4 * $stateIndex]
        $failure = $entries[2 + 4 * $stateIndex]
        $outcomes = @('resume-success', 'resume-failure', 'resume-cancelled', 'resume-closed')
        for ($outcome = 0; $outcome -lt 4; ++$outcome) {
            $entry = $entries[1 + 4 * $stateIndex + $outcome]
            if ($entry.kindName -ne $outcomes[$outcome] -or
                $entry.machine -ne $success.machine -or
                $entry.suspension -ne $state.id -or $entry.block -ne $state.block -or
                $entry.firstInstruction -ne (1 + [int64]$state.awaitInstruction) -or
                $entry.programBlock -ne $success.programBlock -or
                $ir.instructions[[int]$state.awaitInstruction].futureOutcomeAbi -ne 1) {
                throw 'All four Future outcomes must enter the verified post-AWAIT fragment'
            }
        }
        if ( $success.kindName -ne "resume-success" -or $success.suspension -ne $state.id -or
             $success.block -ne $state.block -or
             $success.firstInstruction -ne (1 + [int64]$state.awaitInstruction) -or
             $success.programBlock -ne $failure.programBlock -or
             $failure.kindName -ne "resume-failure" -or $failure.suspension -ne $state.id -or
             $failure.block -ne $state.block -or
             $failure.firstInstruction -ne (1 + [int64]$state.awaitInstruction) ) {
            throw "XLIR continuation machine callback entries do not match suspension CFG edges"
        }
        $suspendBlock = @($blocks | Where-Object {
            $_.kindName -eq "suspend" -and $_.suspension -eq $state.id
        })
        if ( $suspendBlock.Count -ne 1 -or
             ([int64]$suspendBlock[0].firstInstruction + [int64]$suspendBlock[0].instructionCount - 1) -ne [int64]$state.awaitInstruction ) {
            throw "XLIR continuation machine did not split its program at the exact AWAIT"
        }
    }
}
$stringMachine = @($machines | Where-Object { $_.targetSymbol -eq 5 })
if ( $stringMachine.Count -ne 1 ) {
    throw "XLIR managed async target has no unique machine"
}
$stringMachineFrames = @($machineFrameSlots |
    Select-Object -Skip ([int]$stringMachine[0].firstFrameSlot) |
    Select-Object -First ([int]$stringMachine[0].frameSlotCount))
$borrowedStringFrames = @($stringMachineFrames | Where-Object {
    $_.sourceName -eq "value" -and $_.residenceName -eq "ssa-spill" -and
    $_.transferAbiName -eq "borrow-storage" -and $_.destroyAbiName -eq "none" -and
    $_.hasInitBit -eq 0
})
$anchoredStringFrames = @($stringMachineFrames | Where-Object {
    $_.sourceName -eq "storage" -and $_.residenceName -eq "frame-storage" -and
    $_.transferAbiName -eq "none" -and $_.destroyAbiName -eq "free-string"
})
$copiedStringParameters = @($stringMachineFrames | Where-Object {
    $_.sourceName -eq "storage" -and $_.residenceName -eq "launch-parameter" -and
    $_.transferAbiName -eq "copy-string" -and $_.destroyAbiName -eq "free-string"
})
if ( $borrowedStringFrames.Count -ne 1 -or $copiedStringParameters.Count -ne 1 -or
     $anchoredStringFrames.Count -ne 1 -or
     $borrowedStringFrames[0].borrowFrameSlot -ne $anchoredStringFrames[0].id -or
     @($ir.asyncFrameActions | Where-Object {
         $_.frameSlot -eq $borrowedStringFrames[0].id -and $_.kindName -like 'transfer-*'
     }).Count -ne 0 ) {
    throw "XLIR managed async frame lost its parameter, operand owner or borrowed observation"
}
$anchorFrame = $anchoredStringFrames[0]
$anchorStorage = @($ir.storages | Where-Object { $_.symbol -eq $anchorFrame.storageSymbol })
$observation = @($ir.instructions | Where-Object { $_.result -eq $borrowedStringFrames[0].value })
$stringState = @($suspensions | Where-Object { $_.machine -eq $stringMachine[0].id })[0]
if ( $anchorStorage.Count -ne 1 -or $anchorStorage[0].kindName -ne "temporary" -or
     $anchorStorage[0].type -ne $anchorFrame.type -or $anchorFrame.hasInitBit -ne 1 -or
     $borrowedStringFrames[0].type -ne $anchorFrame.type -or
     $observation.Count -ne 1 -or $observation[0].kindName -ne "load" -or
     $observation[0].targetSymbol -ne $anchorFrame.storageSymbol -or
     $observation[0].index -ge $stringState.awaitInstruction -or
     $observation[0].type -ne $borrowedStringFrames[0].type ) {
    throw "The cross-await borrowed string must observe the exact live operand owner"
}
$failureBlock = @($ir.blocks | Where-Object { $_.id -eq $stringState.failureBlock })[0]
$failureCleanup = @($ir.instructions | Where-Object {
    $_.index -ge $failureBlock.firstInstruction -and
    $_.index -lt $failureBlock.firstInstruction + $failureBlock.instructionCount -and
    $_.kindName -eq "release" -and $_.targetSymbol -eq $anchorFrame.storageSymbol -and
    $_.type -eq $anchorFrame.type
})
if ( $failureCleanup.Count -ne 1 ) {
    throw "Await failure lost the binary operand owner's exact cleanup"
}
$successBlock = @($ir.blocks | Where-Object { $_.id -eq $stringState.successBlock })[0]
$borrowConsumers = @($ir.instructions | Where-Object {
    $_.index -ge $successBlock.firstInstruction -and
    $_.index -lt $successBlock.firstInstruction + $successBlock.instructionCount -and
    $_.runtimeOpName -eq "string-concat" -and
    @($_.operands) -contains $observation[0].result
})
if ( $borrowConsumers.Count -ne 1 ) {
    throw "Await success did not consume the exact anchored string observation"
}
$borrowConsumerTest = @($ir.instructions | Where-Object {
    $_.index -eq (1 + [int64]$borrowConsumers[0].index) -and
    $_.kindName -eq "error-test" -and @($_.operands) -contains $borrowConsumers[0].result
})
if ( $borrowConsumerTest.Count -ne 1 ) {
    throw "Anchored string observation has no explicit success/failure CFG boundary"
}
foreach ( $blockId in @($borrowConsumerTest[0].trueTarget, $borrowConsumerTest[0].falseTarget) ) {
    $block = @($ir.blocks | Where-Object { $_.id -eq $blockId })[0]
    $cleanup = @($ir.instructions | Where-Object {
        $_.index -ge $block.firstInstruction -and
        $_.index -lt $block.firstInstruction + $block.instructionCount -and
        $_.kindName -eq "release" -and $_.targetSymbol -eq $anchorFrame.storageSymbol -and
        $_.type -eq $anchorFrame.type
    })
    if ( $cleanup.Count -ne 1 ) {
        throw "Borrowed string consumer success/failure lost its anchored owner cleanup"
    }
}
foreach ( $state in $suspensions ) {
    $awaitInstruction = $ir.instructions[[int]$state.awaitInstruction]
    $cleanupPlan = $ir.exitPlans[[int]$state.failureExitPlan]
    if ( $cleanupPlan.origin -ne $awaitInstruction.origin -or $cleanupPlan.kind -ne 4 -or
         $state.scopeDepth -le $state.cleanupTargetScopeDepth -or
         $state.cleanupTargetScopeDepth -ne $cleanupPlan.targetScopeDepth -or
         $state.cleanupTargetRegion -ne $cleanupPlan.targetRegion -or
         $state.firstCleanupAction -ne $cleanupPlan.firstCleanup -or
         $state.cleanupActionCount -ne $cleanupPlan.cleanupCount ) {
        throw "XLIR canonical async CFG did not preserve its lexical failure cleanup plan"
    }
}
if ( @($frameActions | Where-Object { $_.kindName -eq "spill-value" }).Count -lt 3 -or
     @($frameActions | Where-Object { $_.kindName -eq "project-value" }).Count -lt 3 -or
     @($frameActions | Where-Object { $_.kindName -eq "destroy-if-initialized" }).Count -ne
         @($machineFrameSlots | Where-Object { $_.destroyAbiName -ne "none" }).Count ) {
    throw "XLIR canonical async CFGs did not derive spill/project/guarded-destroy actions"
}
foreach ( $slot in @($machineFrameSlots | Where-Object { $_.destroyAbiName -ne "none" }) ) {
    if ( @($frameActions | Where-Object {
        $_.kindName -eq "destroy-if-initialized" -and $_.frameSlot -eq $slot.id
    }).Count -ne 1 ) { throw "Every managed frame slot needs one guarded destructor action" }
}
foreach ( $cfg in $asyncWorkers ) {
    $cfgInstructions = @($ir.instructions | Where-Object {
        $_.id -gt [int64]$cfg.firstInstruction -and
        $_.id -le ([int64]$cfg.firstInstruction + [int64]$cfg.instructionCount)
    })
    if ( @($cfgInstructions | Where-Object { $_.kindName -eq "await" }).Count -ne 1 ) {
        throw "XLIR planner-free async target did not retain one canonical lowered await CFG"
    }
}

$lower = [System.IO.File]::ReadAllText($lowerSource, [System.Text.Encoding]::UTF8)
$cgen = [System.IO.File]::ReadAllText($cgenSource, [System.Text.Encoding]::UTF8)
foreach ( $include in [regex]::Matches(
    $cgen, '#include "(?<path>src/cgen/xl6_ir_cgen[^\"]+\.c)"') ) {
    $path = Join-Path $Root ($include.Groups["path"].Value -replace '/', '\')
    $cgen += "`n" + [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}
$asyncModelSources = [System.IO.File]::ReadAllText($irHeader, [System.Text.Encoding]::UTF8) +
    [System.IO.File]::ReadAllText($irSource, [System.Text.Encoding]::UTF8) + $lower + $cgen
foreach ( $removedModel in @(
    "XL6_IR_FUNCTION_ASYNC_CFG", "XL6_IR_ASYNC_DISPATCH_XRT_FUTURE_CONTINUATION",
    "XL6_IrAsyncContinuation", "XL6_IrAsyncFrameSlot", "XL6_IrAsyncResumeOp",
    "asyncContinuations", "asyncFrameSlots", "asyncResumeOps"
) ) {
    if ( $asyncModelSources.Contains($removedModel) ) {
        throw "XLIR source still exposes removed async compatibility model: $removedModel"
    }
}
if ( $lower.Contains("!xl6IrLowerPlanAsyncPureReturn") -or
     $lower.Contains("!xl6IrLowerMarkAsyncFutureExternCatchDefault") ) {
    throw "XLIR async registration still invokes a source-shape compatibility planner"
}
foreach ( $legacySelector in @(
    "xl6IrLowerMarkAsyncFutureExternForward",
    "xl6IrLowerMarkAsyncFutureExternThenLocalParameter",
    "xl6IrLowerMarkAsyncFutureForward",
    "xl6IrLowerMarkAsyncFutureThenScalar",
    "xl6IrLowerMarkAsyncFutureThenImmediate",
    "xl6IrLowerMarkAsyncFutureThenLocalParameter",
    "xl6IrLowerMarkAsyncFutureThenStringParameters",
    "xl6IrLowerMarkAsyncFutureThenCaptureChain",
    "xl6IrLowerMarkAsyncFutureAwaitChain"
) ) {
    if ( ([regex]::Matches($lower, [regex]::Escape($legacySelector) + '\s*\(')).Count -ne 0 ) {
        throw "Legacy async source-shape selector was not deleted: $legacySelector"
    }
}
if ( $cgen.Contains("xl6IrLowerPlanAsyncPureReturn") -or
     $cgen.Contains("XL6_IrAsyncAliasPlan") -or
     (-not $cgen.Contains("xl6IrCgenAsyncMachine")) -or
     (-not $cgen.Contains("xl6IrCgenFunctionIsCanonicalMachineProgram")) -or
     (-not $cgen.Contains("target->asyncMachine")) -or
     (-not $cgen.Contains("machine->suspensionCount")) -or
     $cgen.Contains("for ( i = 0; i < module->asyncMachineCount") ) {
    throw "XLIR Cgen depends on lowering-time async expression planning"
}

$c = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $cfg in $asyncWorkers ) {
    if ( $c.Contains(("xlir_fn_{0}(" -f $cfg.symbol)) ) {
        throw "XLIR Cgen emitted an analysis-only async CFG as an ordinary C function"
    }
}
foreach ( $required in @(
    "typedef struct xlir_async_machine_frame_3",
    "xlir_async_machine_resume_3_0",
    "typedef struct xlir_async_machine_frame_4",
    "typedef struct xlir_async_machine_frame_5",
    "xlir_async_machine_resume_5_0",
    "XL6_ContinuationFrameLayoutV2",
    "xl6ContinuationExecutorReserveOwned(",
    "xl6ContinuationReserveSuccessor(",
    "xl6ContinuationWatchRegister(",
    "xl6ContinuationComplete(",
    "xl6ContinuationFrameRuntimeV2Get()->Cleanup(",
    "xl6PayloadResolveOwned(",
    "{ const XL6_SlotWrite xlir_writes[] = {"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR generic async machine Cgen is missing verified CFG emission: $required"
    }
}
$operators = @{ add = "+"; sub = "-" }
foreach ( $worker in $asyncWorkers ) {
    $binaryInstructions = @($ir.instructions | Where-Object {
        $_.index -ge $worker.firstInstruction -and
        $_.index -lt ($worker.firstInstruction + $worker.instructionCount) -and
        $_.kindName -eq "binary"
    })
    foreach ( $binary in $binaryInstructions ) {
        $operator = $operators[$binary.opcodeName]
        if ( $null -eq $operator ) { throw "Unexpected generic async binary opcode: $($binary.opcodeName)" }
        $rendering = "v$($binary.result) = v$($binary.operands[0]) $operator v$($binary.operands[1]);"
        if ( !$c.Contains($rendering) ) {
            throw "Async Cgen did not render the verified binary operands: $rendering"
        }
    }
}
foreach ( $machine in $machines ) {
    $machineActions = @($frameActions | Select-Object -Skip ([int]$machine.firstAction) |
        Select-Object -First ([int]$machine.actionCount))
    foreach ( $project in @($machineActions | Where-Object { $_.kindName -eq "project-value" }) ) {
        $frameLocal = [int64]$project.frameSlot - [int64]$machine.firstFrameSlot
        $rendering = "v$($project.value) = frame->f$frameLocal;"
        if ( !$c.Contains($rendering) ) {
            throw "Async Cgen did not project the verified frame resident: $rendering"
        }
    }
    foreach ( $spill in @($machineActions | Where-Object { $_.kindName -eq "spill-value" }) ) {
        $frameLocal = [int64]$spill.frameSlot - [int64]$machine.firstFrameSlot
        $rendering = "{ &(frame->f$frameLocal), &(xlir_committed), sizeof(frame->f$frameLocal) },"
        if ( !$c.Contains($rendering) ) {
            throw "Async Cgen did not atomically commit the verified frame spill: $rendering"
        }
    }
}
$resultCopy = "v$($stringState.resultValue) = *payload != NULL ? xrtStrDup(*payload) : NULL;"
$observedValue = $borrowedStringFrames[0].value
$spillIndex = $borrowedStringFrames[0].id - $stringMachine[0].firstFrameSlot
$spillCopy = "frame->f$spillIndex = v$observedValue != NULL ? xrtStrDup(v$observedValue) : NULL;"
$spillCommit = "{ &(frame->f$spillIndex), &(xlir_committed), sizeof(frame->f$spillIndex) },"
if ( !$c.Contains($resultCopy) -or !$c.Contains($spillCommit) -or $c.Contains($spillCopy) ) {
    throw "Async generated C must acquire the await result and preserve the verified borrow-storage spill without copying it"
}
$worker = $ir.functions[$stringMachine[0].function]
$concats = @($ir.instructions | Where-Object {
    $_.index -ge $worker.firstInstruction -and $_.index -lt $worker.firstInstruction + $worker.instructionCount -and
    $_.runtimeOpName -eq "string-concat"
})
if ( $concats.Count -ne 3 ) { throw "Nested async string expression lost a concat" }
foreach ( $concat in $concats ) {
    $rendering = "v$($concat.result) = xrtStrConcat(xrtStrView(v$($concat.operands[0])), xrtStrView(v$($concat.operands[1])));"
    if ( !$c.Contains($rendering) ) { throw "Async Cgen did not render the verified concat operands" }
}
if ( $c.Contains("xlir_async_capture_chain_frame_3") -or
     $c.Contains("xlir_async_capture_chain_frame_4") -or
     $c.Contains("xlir_async_string_capture_chain_frame_5") ) {
    throw "XLIR generic async targets 3/4/5 still use the legacy capture/resume renderer"
}
$firstBytes = [System.IO.File]::ReadAllBytes($cPath)
$repeatBytes = [System.IO.File]::ReadAllBytes($cRepeatPath)
if ( $firstBytes.Length -ne $repeatBytes.Length -or
     ! [System.Linq.Enumerable]::SequenceEqual(
         [byte[]]$firstBytes, [byte[]]$repeatBytes) ) {
    throw "XLIR generic async planner C emission is not byte deterministic"
}

$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_async_generic_expression_planner_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_generic_expression_planner.exe && build\tmp\xlir_async_generic_expression_planner.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic async planner strict-C/runtime gate failed" }

Write-Output "XLIR generic async machine gate ok: source-shape planners disconnected and compatibility model removed; lowered worker CFGs, suspension-split launch/resume programs, direct target links, explicit ownership/take points, ordered frame actions and lexical cleanup transitions drive deterministic direct machine C, -Werror and runtime semantics."
