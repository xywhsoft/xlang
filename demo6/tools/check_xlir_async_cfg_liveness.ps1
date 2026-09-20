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
$actualActionShape = @($frameActions | ForEach-Object { "{0}:{1}" -f $_.kindName,$_.frameSlot })
$expectedActionShape = @(
    "init-parameter:0", "init-parameter:1", "init-parameter:2",
    "bind-storage:3", "destroy-if-initialized:0"
)
if ( [int]$machines[0].actionCount -ne 5 -or
     [int]$machines[0].prologueActionCount -ne 4 -or
     [int]$machines[0].destroyActionCount -ne 1 -or
     @($suspensions | Where-Object { $_.spillActionCount -ne 0 -or $_.resumeActionCount -ne 0 }).Count -ne 0 -or
     (Compare-Object $expectedActionShape $actualActionShape -SyncWindow 0) ) {
    throw "XLIR async machine did not derive ordered storage binding and guarded destroy actions"
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
foreach ( $required in @("async-machine", "machine-entry", "kind=launch", "kind=resume-success", "kind=resume-failure", "kind=resume-cancelled", "kind=resume-closed", "machine-block", "kind=suspend", "kind=execute", "instructions=", "init-bits=1", "machine-frame", "residence=launch-parameter", "initBit=0", "suspension", "scope=1 region=5", "cleanupPlan=", "cleanupTarget=0/0", "live", "frame-action", "kind=init-parameter", "kind=bind-storage", "kind=destroy-if-initialized", "source=storage", "transfer=retain-future", "destroy=destroy-future") ) {
    if ( ! $text.Contains($required) ) { throw "XLIR async CFG text dump is missing: $required" }
}
$c = [IO.File]::ReadAllText($cPath, [Text.Encoding]::UTF8)
if ( -not $c.Contains("xlir_async_machine_frame_2") -or
     -not $c.Contains("xlir_async_machine_resume_2_0") -or
     -not $c.Contains("xlir_async_machine_resume_2_1") -or
     -not [regex]::IsMatch($c, 'if \(v\d+\) goto ap\d+; else goto ap\d+;') -or
     -not $c.Contains("next->outer = frame->outer; frame->outer = NULL") -or
     -not $c.Contains("next->cancelObserved = frame->cancelObserved") -or
     -not $c.Contains("xrtFutureWatchAdd(source, &frame->watch)") -or
     $c.Contains("xlir_async_chain_frame_") -or
     $c.Contains("xlir_async_capture_chain_frame_") ) {
    throw "XLIR async CFG branch target was not emitted directly from its verified machine"
}

$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_async_cfg_liveness.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_cfg_liveness.exe && build\tmp\xlir_async_cfg_liveness.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async CFG liveness strict-C/runtime gate failed" }

Write-Output "XLIR async CFG liveness gate ok: explicit launch/success/failure/cancelled/closed entries and block program, two AWAIT splits, direct call-target machine link, unique machine frame layout, per-state membership, frame residence/ranges/init bits, ordered frame actions, lexical cleanup transitions, exact transfer/destroy ABI, direct branch machine Cgen, deterministic C, task-pool compatibility and runtime semantics."
