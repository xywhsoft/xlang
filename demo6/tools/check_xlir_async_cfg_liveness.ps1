param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$compiler = Join-Path $Root "release\xl.exe"
$fixture = Join-Path $Root "test\cases\xlir_async_cfg_liveness.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\tmp"
$jsonPath = Join-Path $outputDir "xlir_async_cfg_liveness.lowered.json"
$textPath = Join-Path $outputDir "xlir_async_cfg_liveness.lowered.txt"
$cPath = Join-Path $outputDir "xlir_async_cfg_liveness.c"
$cRepeatPath = Join-Path $outputDir "xlir_async_cfg_liveness.repeat.c"
$executable = Join-Path $outputDir "xlir_async_cfg_liveness.exe"

foreach ( $path in @($compiler,$fixture,$hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR async CFG liveness gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $compiler ir $fixture --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async CFG liveness JSON dump failed" }
& $compiler ir $fixture --lowered --format text --emit $textPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async CFG liveness text dump failed" }
& $compiler cgen $fixture --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async CFG liveness C generation failed" }
& $compiler cgen $fixture --backend xlir --emit $cRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async CFG liveness repeated C generation failed" }
if ( -not [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][IO.File]::ReadAllBytes($cPath),
        [byte[]][IO.File]::ReadAllBytes($cRepeatPath)) ) {
    throw "XLIR async CFG liveness generated C is not deterministic"
}

$ir = [IO.File]::ReadAllText($jsonPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
$machines = @($ir.asyncMachines)
$suspensions = @($ir.asyncSuspensions)
$frameSlots = @($ir.asyncMachineFrameSlots)
$liveSlots = @($ir.asyncLiveSlots)
$frameActions = @($ir.asyncFrameActions)
$machineEntries = @($ir.asyncMachineEntries)
$machineBlocks = @($ir.asyncMachineBlocks)
if ( $machines.Count -ne 1 -or $suspensions.Count -ne 2 -or
     $machines[0].statusName -ne "analyzed" -or
     $machines[0].suspensionCount -ne 2 ) {
    throw "XLIR did not derive one two-state analyzed machine from the branch-after-await CFG"
}
$machineFunction = $ir.functions[[int]$machines[0].function]
$entries = @($machineEntries | Select-Object -Skip ([int]$machines[0].firstEntry) |
    Select-Object -First ([int]$machines[0].entryCount))
$blocks = @($machineBlocks | Select-Object -Skip ([int]$machines[0].firstBlock) |
    Select-Object -First ([int]$machines[0].blockCount))
if ( $entries.Count -ne 9 -or $entries[0].kindName -ne "launch" -or
     $entries[0].block -ne $machines[0].entryBlock -or
     $entries[0].programBlock -lt $machines[0].firstBlock -or
     $entries[0].programBlock -ge ([int64]$machines[0].firstBlock + [int64]$machines[0].blockCount) -or
     @($entries | Where-Object { $_.machine -ne $machines[0].id }).Count -ne 0 -or
     $blocks.Count -ne ([int]$machineFunction.blockCount + $suspensions.Count) -or
     @($blocks | Where-Object { $_.kindName -eq "suspend" }).Count -ne $suspensions.Count -or
     @($blocks | Where-Object { $_.kindName -eq "invalid" }).Count -ne 0 -or
     @($blocks | Where-Object { $_.machine -ne $machines[0].id }).Count -ne 0 ) {
    throw "XLIR async machine does not own an explicit canonical entry/block program"
}
for ( $stateIndex = 0; $stateIndex -lt $suspensions.Count; ++$stateIndex ) {
    $state = $suspensions[$stateIndex]
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
        throw "XLIR async machine entry table does not match all four suspension outcomes"
    }
    $suspendBlock = @($blocks | Where-Object {
        $_.kindName -eq "suspend" -and $_.suspension -eq $state.id
    })
    if ( $suspendBlock.Count -ne 1 -or
         ([int64]$suspendBlock[0].firstInstruction + [int64]$suspendBlock[0].instructionCount - 1) -ne [int64]$state.awaitInstruction ) {
        throw "XLIR async machine did not split a program fragment at its exact AWAIT"
    }
}
$machineTarget = @($ir.callTargets | Where-Object { $_.symbol -eq $machines[0].targetSymbol })
if ( $machineFunction.kindName -ne "async-worker" -or
     $machineTarget.Count -ne 1 -or $machineTarget[0].asyncDispatch -ne 1 -or
     [int64]$machineTarget[0].asyncMachine -ne [int64]$machines[0].id ) {
    throw "XLIR async CFG analysis did not remain independent of the task-pool dispatch decision"
}
$suspension = $suspensions[0]
$sliceRefs = @($liveSlots | Select-Object -Skip ([int]$suspension.firstLiveSlot) |
    Select-Object -First ([int]$suspension.liveSlotCount))
$slice = @($sliceRefs | ForEach-Object { $frameSlots[[int]$_.frameSlot] })
if ( $slice.Count -lt 4 -or
     @($slice | Where-Object { $_.sourceName -eq "storage" }).Count -lt 4 -or
     @($slice | Where-Object { $_.transferAbiName -eq "invalid" -or $_.destroyAbiName -eq "invalid" }).Count -ne 0 -or
     @($slice | Where-Object { $_.residenceName -eq "launch-parameter" -and $_.transferAbiName -eq "retain-future" -and $_.destroyAbiName -eq "destroy-future" }).Count -ne 1 -or
     @($slice | Where-Object { $_.transferAbiName -eq "copy-value" -and $_.destroyAbiName -eq "none" }).Count -lt 2 -or
     @($slice | Where-Object { $_.residenceName -eq "frame-storage" -and $_.transferAbiName -eq "none" -and $_.destroyAbiName -eq "none" }).Count -ne 1 ) {
    throw "XLIR async CFG liveness did not preserve the future/scalar storage frame ABI"
}
$allReferencedFrameSlots = @($liveSlots | ForEach-Object { [int]$_.frameSlot } | Sort-Object -Unique)
if ( $frameSlots.Count -ne [int]$machines[0].frameSlotCount -or
     $allReferencedFrameSlots.Count -ne $frameSlots.Count -or
     $liveSlots.Count -le $frameSlots.Count ) {
    throw "XLIR async CFG liveness did not coalesce per-state liveness into one unique machine frame layout"
}
$managedFrameSlots = @($frameSlots | Where-Object { $_.destroyAbiName -ne "none" })
$localFrameSlots = @($frameSlots | Where-Object { $_.residenceName -eq "frame-storage" })
if ( [int]$machines[0].initBitCount -ne 1 -or
     $managedFrameSlots.Count -ne 1 -or
     $managedFrameSlots[0].residenceName -ne "launch-parameter" -or
     $managedFrameSlots[0].hasInitBit -ne 1 -or
     @($frameSlots | Where-Object { $_.destroyAbiName -eq "none" -and $_.hasInitBit -ne 0 }).Count -ne 0 -or
     $localFrameSlots.Count -ne 1 -or
     @($frameSlots | Where-Object { $_.firstLiveState -gt $_.lastLiveState }).Count -ne 0 ) {
    throw "XLIR async frame residence/range/initialization-bit ABI is not explicit"
}
$machineFirstAction = [int]$machines[0].firstAction
$machineActionCount = [int]$machines[0].actionCount
$machineActions = @($frameActions | Select-Object -Skip $machineFirstAction |
    Select-Object -First $machineActionCount)
$prologueActions = @($frameActions | Select-Object -Skip ([int]$machines[0].firstPrologueAction) |
    Select-Object -First ([int]$machines[0].prologueActionCount))
$destroyActions = @($frameActions | Select-Object -Skip ([int]$machines[0].firstDestroyAction) |
    Select-Object -First ([int]$machines[0].destroyActionCount))
$actualPrologueShape = @($prologueActions | ForEach-Object { "{0}:{1}" -f $_.kindName,$_.frameSlot })
$expectedActionShape = @(
    "init-parameter:0", "init-parameter:1", "init-parameter:2",
    "bind-storage:3"
)
$expectedDestroySlots = @()
for ( $offset = [int]$machines[0].frameSlotCount - 1; $offset -ge 0; --$offset ) {
    $slotIndex = [int]$machines[0].firstFrameSlot + $offset
    if ( [int]$frameSlots[$slotIndex].hasInitBit -eq 1 ) {
        $expectedDestroySlots += $slotIndex
    }
}
$actualDestroyShape = @($destroyActions | ForEach-Object { "{0}:{1}" -f $_.kindName,$_.frameSlot })
$expectedDestroyShape = @($expectedDestroySlots | ForEach-Object { "destroy-if-initialized:$_" })
if ( $machineActions.Count -ne $machineActionCount -or
     @($machineActions | Where-Object { $_.machine -ne $machines[0].id }).Count -ne 0 -or
     [int]$machines[0].prologueActionCount -ne $expectedActionShape.Count -or
     [int]$machines[0].destroyActionCount -ne $expectedDestroyShape.Count -or
     [int]$machines[0].transferActionCount -ne 0 -or
     (Compare-Object $expectedActionShape $actualPrologueShape -SyncWindow 0) -or
     (Compare-Object $expectedDestroyShape $actualDestroyShape -SyncWindow 0) ) {
    throw "XLIR async machine did not derive ordered storage binding and guarded destroy actions"
}
$accountedActionCount = [int]$machines[0].prologueActionCount +
    [int]$machines[0].transferActionCount + [int]$machines[0].destroyActionCount
for ( $actionOffset = 0; $actionOffset -lt $machineActions.Count; ++$actionOffset ) {
    if ( [int]$machineActions[$actionOffset].id -ne $machineFirstAction + $actionOffset ) {
        throw "XLIR async machine action arena is not one contiguous machine-owned slice"
    }
}
foreach ( $state in $suspensions ) {
    if ( [int]$state.spillActionCount -ne 0 -or [int]$state.resumeActionCount -ne 0 ) {
        throw "Storage-backed async liveness unexpectedly emitted SSA spill/resume actions"
    }
    $stateLiveSlots = @($liveSlots | Select-Object -Skip ([int]$state.firstLiveSlot) |
        Select-Object -First ([int]$state.liveSlotCount) |
        ForEach-Object { [int]$_.frameSlot } | Sort-Object -Unique)
    $expectedHandoffSlots = @()
    for ( $offset = 0; $offset -lt [int]$machines[0].frameSlotCount; ++$offset ) {
        $slotIndex = [int]$machines[0].firstFrameSlot + $offset
        if ( [int]$frameSlots[$slotIndex].hasInitBit -eq 1 -or
             $stateLiveSlots -contains $slotIndex ) {
            $expectedHandoffSlots += $slotIndex
        }
    }
    $handoff = @($frameActions | Select-Object -Skip ([int]$state.firstHandoffAction) |
        Select-Object -First ([int]$state.handoffActionCount))
    $rollback = @($frameActions | Select-Object -Skip ([int]$state.firstRollbackAction) |
        Select-Object -First ([int]$state.rollbackActionCount))
    if ( $handoff.Count -ne $expectedHandoffSlots.Count -or
         $rollback.Count -ne $expectedHandoffSlots.Count ) {
        throw "XLIR async suspension handoff/rollback action count does not match resident ownership"
    }
    for ( $actionOffset = 0; $actionOffset -lt $expectedHandoffSlots.Count; ++$actionOffset ) {
        $forward = $handoff[$actionOffset]
        $reverse = $rollback[$expectedHandoffSlots.Count - 1 - $actionOffset]
        $expectedSlot = $expectedHandoffSlots[$actionOffset]
        if ( $forward.kindName -ne "handoff-resident" -or
             $forward.machine -ne $machines[0].id -or $forward.suspension -ne $state.id -or
             [int]$forward.frameSlot -ne $expectedSlot -or
             $reverse.kindName -ne "rollback-handoff-resident" -or
             $reverse.machine -ne $machines[0].id -or $reverse.suspension -ne $state.id -or
             [int]$reverse.frameSlot -ne $expectedSlot ) {
            throw "XLIR async suspension did not derive a forward handoff and exact reverse rollback"
        }
    }
    $accountedActionCount += $handoff.Count + $rollback.Count
}
if ( $accountedActionCount -ne $machineActionCount ) {
    throw "XLIR async machine action slice contains an unclassified ownership action"
}
foreach ( $state in $suspensions ) {
    $awaitInstruction = $ir.instructions[[int]$state.awaitInstruction]
    $cleanupPlan = $ir.exitPlans[[int]$state.failureExitPlan]
    if ( $state.scopeDepth -ne 1 -or $state.region -ne 5 -or
         $cleanupPlan.origin -ne $awaitInstruction.origin -or $cleanupPlan.kind -ne 4 -or
         $state.cleanupTargetScopeDepth -ne $cleanupPlan.targetScopeDepth -or
         $state.cleanupTargetRegion -ne $cleanupPlan.targetRegion -or
         $state.firstCleanupAction -ne $cleanupPlan.firstCleanup -or
         $state.cleanupActionCount -ne $cleanupPlan.cleanupCount ) {
        throw "XLIR async suspension did not retain its exact lexical cleanup transition"
    }
}
$text = [IO.File]::ReadAllText($textPath, [Text.Encoding]::UTF8)
foreach ( $required in @("async-machine", "machine-entry", "kind=launch", "kind=resume-success", "kind=resume-failure", "kind=resume-cancelled", "kind=resume-closed", "machine-block", "kind=suspend", "kind=execute", "instructions=", "init-bits=1", "machine-frame", "residence=launch-parameter", "initBit=0", "suspension", "scope=1 region=5", "cleanupPlan=", "live", "frame-action", "kind=init-parameter", "kind=bind-storage", "kind=handoff-resident", "kind=rollback-handoff-resident", "kind=destroy-if-initialized", "source=storage", "transfer=retain-future", "destroy=destroy-future") ) {
    if ( ! $text.Contains($required) ) { throw "XLIR async CFG text dump is missing: $required" }
}
foreach ( $state in $suspensions ) {
    $cleanupTarget = "cleanupTarget=$($state.cleanupTargetScopeDepth)/$($state.cleanupTargetRegion)"
    if ( ! $text.Contains($cleanupTarget) ) {
        throw "XLIR async CFG text dump does not preserve the verified cleanup target: $cleanupTarget"
    }
}
$c = [IO.File]::ReadAllText($cPath, [Text.Encoding]::UTF8)
if ( -not $c.Contains("xlir_async_machine_frame_2") -or
     -not $c.Contains("xlir_async_machine_resume_2_0") -or
     -not $c.Contains("xlir_async_machine_resume_2_1") -or
     -not [regex]::IsMatch($c, 'if \(v\d+\) goto ap\d+; else goto ap\d+;') -or
     -not $c.Contains("XL6_ContinuationTicket ticket") -or
     -not $c.Contains("xl6ContinuationReserveSuccessor(predecessor, &frame->ticket") -or
     -not $c.Contains("xl6ContinuationFrameRuntimeV3Get()->Bind(frame)") -or
     -not $c.Contains("xl6ContinuationWatchRegister(&frame->ticket, &frame->watch, source)") -or
     -not $c.Contains("xl6ContinuationFrameRuntimeV4Get()->Observe(frame, &input)") -or
     -not $c.Contains("{ &(next->outer), &(frame->outer), sizeof(next->outer) },") -or
     -not $c.Contains("{ &(frame->outer), &(next->outer), sizeof(frame->outer) },") -or
     $c.Contains("xlir_async_chain_frame_") -or
     $c.Contains("xlir_async_capture_chain_frame_") ) {
    throw "XLIR async CFG branch target was not emitted directly from its verified machine"
}
$transitionStart = $c.IndexOf("xlir_async_frame_allocate(sizeof(*next)", [StringComparison]::Ordinal)
$transitionEnd = if ( $transitionStart -ge 0 ) {
    $c.IndexOf("xlir_async_frame_dispose(next)", $transitionStart, [StringComparison]::Ordinal)
} else { -1 }
if ( $transitionStart -lt 0 -or $transitionEnd -le $transitionStart ) {
    throw "XLIR async CFG did not emit one inspectable continuation handoff transaction"
}
$handoffState = $suspensions[1]
$handoffActions = @($frameActions | Select-Object -Skip ([int]$handoffState.firstHandoffAction) |
    Select-Object -First ([int]$handoffState.handoffActionCount))
$rollbackActions = @($frameActions | Select-Object -Skip ([int]$handoffState.firstRollbackAction) |
    Select-Object -First ([int]$handoffState.rollbackActionCount))
$cursor = $transitionStart
foreach ( $action in $handoffActions ) {
    $field = "f$($action.frameSlot)"
    $pattern = "{ &(next->$field), &(frame->$field), sizeof(next->$field) },"
    $position = $c.IndexOf($pattern, $cursor, [StringComparison]::Ordinal)
    if ( $position -lt 0 -or $position -ge $transitionEnd ) {
        throw "XLIR async Cgen omitted or reordered a verified resident handoff: $field"
    }
    $slot = $frameSlots[[int]$action.frameSlot]
    if ( [int]$slot.hasInitBit -eq 1 ) {
        $initField = "i$($slot.initBit)"
        $initPattern = "{ &(next->$initField), &(frame->$initField), sizeof(next->$initField) },"
        $initPosition = $c.IndexOf($initPattern, $position, [StringComparison]::Ordinal)
        if ( $initPosition -lt 0 -or $initPosition -ge $transitionEnd ) {
            throw "XLIR async Cgen omitted the resident handoff initialization bit: $initField"
        }
    }
    $cursor = $position + $pattern.Length
}
foreach ( $action in $rollbackActions ) {
    $field = "f$($action.frameSlot)"
    $pattern = "{ &(frame->$field), &(next->$field), sizeof(frame->$field) },"
    $position = $c.IndexOf($pattern, $cursor, [StringComparison]::Ordinal)
    if ( $position -lt 0 -or $position -ge $transitionEnd ) {
        throw "XLIR async Cgen omitted or reordered a verified resident rollback: $field"
    }
    $slot = $frameSlots[[int]$action.frameSlot]
    if ( [int]$slot.hasInitBit -eq 1 ) {
        $initField = "i$($slot.initBit)"
        $initPattern = "{ &(frame->$initField), &(next->$initField), sizeof(frame->$initField) },"
        $initPosition = $c.IndexOf($initPattern, $position, [StringComparison]::Ordinal)
        if ( $initPosition -lt 0 -or $initPosition -ge $transitionEnd ) {
            throw "XLIR async Cgen omitted the resident rollback initialization bit: $initField"
        }
    }
    $cursor = $position + $pattern.Length
}

$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_async_cfg_liveness.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_cfg_liveness.exe && build\tmp\xlir_async_cfg_liveness.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async CFG liveness strict-C/runtime gate failed" }

Write-Output "XLIR async CFG liveness gate ok: explicit launch/success/failure/cancelled/closed entries and block program, two AWAIT splits, direct call-target machine link, unique machine frame layout, per-state membership, frame residence/ranges/init bits, ordered frame actions, lexical cleanup transitions, exact transfer/destroy ABI, direct branch machine Cgen, deterministic C, task-pool compatibility and runtime semantics."
