param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$programHostSource = Join-Path $Root "test\cases\xlir_program_null_args_host.c"
$outputDir = Join-Path $Root "build\test"
$cases = @(
    [pscustomobject]@{
        Name = "cgen_generator"
        Source = "cgen_generator.xl"
        MachineCount = 9; EntryCount = 18; SuspensionCount = 9
        ItemAbis = @("copy-i64", "copy-bool", "copy-f64", "copy-time")
        Managed = $false
    },
    [pscustomobject]@{
        Name = "cgen_generator_two_targets"
        Source = "cgen_generator_two_targets.xl"
        MachineCount = 1; EntryCount = 5; SuspensionCount = 4
        ItemAbis = @("copy-i64")
        Managed = $false
    },
    [pscustomobject]@{
        Name = "cgen_generator_fstring_needs"
        Source = "cgen_generator_fstring_needs.xl"
        MachineCount = 1; EntryCount = 3; SuspensionCount = 2
        ItemAbis = @("copy-i64")
        Managed = $false
    },
    [pscustomobject]@{
        Name = "cgen_async_generator"
        Source = "cgen_async_generator.xl"
        MachineCount = 9; EntryCount = 23; SuspensionCount = 14
        ItemAbis = @("copy-i64")
        Managed = $false
        BlockingAwaitCount = 12
    },
    [pscustomobject]@{
        Name = "cgen_generator_string_resource"
        Source = "cgen_generator_string_resource.xl"
        MachineCount = 1; EntryCount = 2; SuspensionCount = 1
        ItemAbis = @("move-string")
        Managed = $true
    },
    [pscustomobject]@{
        Name = "xlir_generator_string_machine"
        Source = "xlir_generator_string_machine.xl"
        MachineCount = 1; EntryCount = 3; SuspensionCount = 2
        ItemAbis = @("move-string")
        Managed = $true
        Program = $false
    },
    [pscustomobject]@{
        Name = "sem_generator_function_param"
        Source = "sem_generator_function_param.xl"
        MachineCount = 1; EntryCount = 2; SuspensionCount = 1
        ItemAbis = @("copy-i64")
        Managed = $false
        Program = $true
    }
)

foreach ( $path in @($xl, $hostSource, $programHostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR generator machine gate input is missing: $path"
    }
}
foreach ( $case in $cases ) {
    $source = Join-Path $Root "test\cases\$($case.Source)"
    if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
        throw "XLIR generator machine fixture is missing: $source"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$countFields = [ordered]@{
    generatorMachines = "generatorMachineCount"
    generatorMachineEntries = "generatorMachineEntryCount"
    generatorMachineBlocks = "generatorMachineBlockCount"
    generatorFrameSlots = "generatorFrameSlotCount"
    generatorSuspensions = "generatorSuspensionCount"
    generatorLiveSlots = "generatorLiveSlotCount"
    generatorFrameActions = "generatorFrameActionCount"
}

foreach ( $case in $cases ) {
    $source = Join-Path $Root "test\cases\$($case.Source)"
    $jsonPath = Join-Path $outputDir "$($case.Name).generator-machine.json"
    $cPath = Join-Path $outputDir "$($case.Name).generator-machine.c"
    $repeatCPath = Join-Path $outputDir "$($case.Name).generator-machine.repeat.c"
    $exePath = Join-Path $outputDir "$($case.Name).generator-machine.exe"

    & $xl ir $source --lowered --format json --emit $jsonPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR generator lowered JSON failed: $($case.Name)"
    }
    $ir = [System.IO.File]::ReadAllText(
        $jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

    foreach ( $pair in $countFields.GetEnumerator() ) {
        $actual = @($ir.($pair.Key)).Count
        $declared = [int]$ir.($pair.Value)
        if ( $actual -ne $declared ) {
            throw "XLIR generator dump count mismatch for $($case.Name): $($pair.Key)=$actual, $($pair.Value)=$declared"
        }
    }

    $machines = @($ir.generatorMachines)
    $entries = @($ir.generatorMachineEntries)
    $blocks = @($ir.generatorMachineBlocks)
    $suspensions = @($ir.generatorSuspensions)
    if ( $machines.Count -ne $case.MachineCount -or
         $entries.Count -ne $case.EntryCount -or
         $suspensions.Count -ne $case.SuspensionCount ) {
        throw "XLIR generator machine topology changed without an explicit gate update: $($case.Name)"
    }

    $generatorTargets = @($ir.callTargets | Where-Object {
        [uint64]$_.generatorMachine -lt [uint64]$machines.Count
    })
    if ( $generatorTargets.Count -ne $machines.Count ) {
        throw "XLIR generator targets do not have a one-to-one machine link: $($case.Name)"
    }
    $linkedMachineIds = @($generatorTargets | ForEach-Object {
        [int]$_.generatorMachine
    } | Sort-Object -Unique)
    if ( $linkedMachineIds.Count -ne $machines.Count ) {
        throw "XLIR generator call targets alias or omit a machine: $($case.Name)"
    }

    foreach ( $machine in $machines ) {
        $machineEntries = @($entries | Where-Object { $_.machine -eq $machine.id })
        $machineBlocks = @($blocks | Where-Object { $_.machine -eq $machine.id })
        $machineSuspensions = @($suspensions | Where-Object { $_.machine -eq $machine.id })
        $target = @($generatorTargets | Where-Object {
            $_.generatorMachine -eq $machine.id
        })
        if ( $machine.statusName -ne "analyzed" -or $target.Count -ne 1 -or
             $target[0].symbol -ne $machine.targetSymbol -or
             $target[0].errorAbiName -ne "no-raise" -or
             $target[0].generatorItemType -ne $machine.itemType -or
             $target[0].generatorYieldAbiName -ne $machine.yieldAbiName ) {
            throw "XLIR generator target/machine ABI link is ambiguous: $($case.Name) g$($machine.id)"
        }
        if ( $machineEntries.Count -ne (1 + $machineSuspensions.Count) -or
             @($machineEntries | Where-Object { $_.kindName -eq "start" }).Count -ne 1 -or
             @($machineEntries | Where-Object { $_.kindName -eq "resume" }).Count -ne $machineSuspensions.Count -or
             @($machineBlocks | Where-Object { $_.kindName -eq "yield" }).Count -ne $machineSuspensions.Count ) {
            throw "XLIR generator continuation entries do not exactly cover suspension states: $($case.Name) g$($machine.id)"
        }
        if ( $machine.yieldAbiName -notin $case.ItemAbis ) {
            throw "XLIR generator item ABI is not admitted by the fixture contract: $($case.Name) $($machine.yieldAbiName)"
        }
        if ( @($target[0].generatorCaptureAbis).Count -ne [int]$target[0].parameterCount ) {
            throw "XLIR generator capture ABI vector does not cover every launch parameter: $($case.Name)"
        }
    }

    $instructionByIndex = @{}
    foreach ( $instruction in @($ir.instructions) ) {
        $instructionByIndex[[string]$instruction.index] = $instruction
    }
    foreach ( $suspension in $suspensions ) {
        $yield = $instructionByIndex[[string]$suspension.yieldInstructionIndex]
        $machine = $machines[[int]$suspension.machine]
        if ( $null -eq $yield -or $yield.kindName -ne "yield" -or
             $suspension.yieldInstruction -ne $suspension.yieldInstructionIndex -or
             $suspension.yieldInstructionId -ne $yield.id -or
             $suspension.itemType -ne $machine.itemType -or
             $suspension.yieldAbiName -ne $machine.yieldAbiName ) {
            throw "XLIR generator suspension is not tied to its verified YIELD instruction and item ABI: $($case.Name)"
        }
    }

    $starts = @($ir.instructions | Where-Object { $_.kindName -eq "generator-start" })
    $nexts = @($ir.instructions | Where-Object { $_.kindName -eq "generator-next" })
    $gets = @($ir.instructions | Where-Object { $_.kindName -eq "generator-item-get" })
    $takes = @($ir.instructions | Where-Object { $_.kindName -eq "generator-item-take" })
    if ( $starts.Count -eq 0 -or $nexts.Count -eq 0 -or
         (!$case.Managed -and $gets.Count -eq 0) -or
         ($case.Managed -and ($takes.Count -eq 0 -or $gets.Count -ne 0)) ) {
        throw "XLIR generator consumer GET/TAKE ownership instructions are incomplete: $($case.Name)"
    }

    if ( $case.Name -eq "cgen_generator_string_resource" ) {
        $target = $generatorTargets[0]
        $frameSlots = @($ir.generatorFrameSlots)
        $actions = @($ir.generatorFrameActions)
        if ( (@($target.generatorCaptureAbis) -join ",") -ne "copy-string,copy-value" -or
             @($frameSlots | Where-Object {
                 $_.residenceName -eq "launch-parameter" -and
                 $_.transferAbiName -eq "copy-string" -and
                 $_.destroyAbiName -eq "free-string" -and
                 [uint64]$_.initBit -lt [uint64]$ir.generatorFrameSlotCount
             }).Count -ne 1 -or
             @($frameSlots | Where-Object {
                 $_.residenceName -eq "frame-storage" -and
                 $_.destroyAbiName -eq "free-string" -and
                 [uint64]$_.initBit -lt [uint64]$ir.generatorFrameSlotCount
             }).Count -ne 1 -or
             @($actions | Where-Object {
                 $_.kindName -eq "destroy-if-initialized"
             }).Count -ne 2 ) {
            throw "XLIR string generator capture/frame ownership ABI is not exact"
        }

        $textPath = Join-Path $outputDir "$($case.Name).generator-machine.txt"
        & $xl ir $source --lowered --emit $textPath
        if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generator text dump failed" }
        $textDump = [System.IO.File]::ReadAllText($textPath, [System.Text.Encoding]::UTF8)
        foreach ( $required in @(
            "generator-machine", "generator-entry", "generator-block",
            "generator-frame", "generator-suspension", "generator-live",
            "generator-frame-action", "move-string", "copy-string",
            "destroy-if-initialized"
        ) ) {
            if ( ! $textDump.Contains($required) ) {
                throw "XLIR generator text dump omits verified machine data: $required"
            }
        }
    }
    if ( $case.Name -eq "sem_generator_function_param" ) {
        $target = $generatorTargets[0]
        $frameSlots = @($ir.generatorFrameSlots)
        $closureCalls = @($ir.instructions | Where-Object {
            $_.kindName -eq "closure-call"
        })
        $closureErrorTests = @($ir.instructions | Where-Object {
            $_.kindName -eq "error-test" -and $closureCalls.Count -eq 1 -and
            @($_.operands).Count -eq 1 -and
            [int]$_.operands[0] -eq [int]$closureCalls[0].result
        })
        if ( (@($target.generatorCaptureAbis) -join ",") -ne
                 "retain-closure" -or
             @($frameSlots | Where-Object {
                 $_.residenceName -eq "launch-parameter" -and
                 $_.transferAbiName -eq "retain-closure" -and
                 $_.destroyAbiName -eq "release-closure"
             }).Count -ne 1 -or
             $closureCalls.Count -ne 1 -or $closureErrorTests.Count -ne 1 ) {
            throw "XLIR closure-parameter generator lost its exact capture/call/error carrier ABI"
        }
    }
    if ( $case.Name -eq "cgen_async_generator" ) {
        $awaits = @($ir.instructions | Where-Object {
            $_.kindName -eq "await"
        })
        $asyncStarts = @($ir.instructions | Where-Object {
            $_.kindName -eq "async-start"
        })
        $asyncTargets = @($ir.callTargets | Where-Object {
            [int]$_.kind -eq 4
        })
        if ( $awaits.Count -ne $case.BlockingAwaitCount -or
             $asyncStarts.Count -ne $case.BlockingAwaitCount -or
             @($awaits | Where-Object {
                 $_.asyncResultAbiName -ne "copy-i64" -or
                 @($_.operands).Count -ne 1
             }).Count -ne 0 -or
             $asyncTargets.Count -ne 1 -or
             @($asyncStarts | Where-Object {
                 [int]$_.targetSymbol -ne [int]$asyncTargets[0].symbol -or
                 [int]$_.operandCount -ne 2
             }).Count -ne 0 -or
             @($generatorTargets | Where-Object {
                 [int]$_.kind -ne 5 -or
                 $_.asyncResultAbiName -ne "invalid"
             }).Count -ne 0 -or
             @($generatorTargets | Where-Object {
                 $_.workerErrorAbiName -eq "may-raise"
             }).Count -ne 8 ) {
            throw "XLIR async generator lost its explicit generator/async-start/blocking-await ABI"
        }
    }

    & $xl cgen $source --backend xlir --emit $cPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generator Cgen failed: $($case.Name)" }
    & $xl cgen $source --backend xlir --emit $repeatCPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generator repeat Cgen failed: $($case.Name)" }
    $left = [System.IO.File]::ReadAllBytes($cPath)
    $right = [System.IO.File]::ReadAllBytes($repeatCPath)
    if ( -not [System.Linq.Enumerable]::SequenceEqual([byte[]]$left, [byte[]]$right) ) {
        throw "XLIR generator Cgen is not byte-for-byte deterministic: $($case.Name)"
    }

    $generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
    $resumeCount = ([regex]::Matches($generated,
        "static bool xlir_generator_resume_[0-9]+\(")).Count
    $startCount = ([regex]::Matches($generated,
        "static xlir_generator\* xlir_generator_start_[0-9]+\(")).Count
    if ( $resumeCount -ne $machines.Count -or $startCount -ne $machines.Count -or
         ! $generated.Contains("static xlir_generator_step xlir_generator_next(") -or
         ! $generated.Contains("step.ok = outcome.ok;") -or
         ! $generated.Contains("static bool xlir_generator_item_get(") -or
         ! $generated.Contains("xlir_generator_put(") -or
         ! $generated.Contains("static void xlir_generator_destroy(") -or
         ($case.Managed -and ! $generated.Contains(", true);")) -or
         (!$case.Managed -and ! $generated.Contains(", false);")) ) {
        throw "XLIR generator C is not a mechanical rendering of machine and item ownership ABI: $($case.Name)"
    }
    $nextCount = @($ir.instructions | Where-Object { $_.kindName -eq 'generator-next' }).Count
    if (([regex]::Matches($generated, 'xlir_generator_step g[0-9]+ = xlir_generator_next\(')).Count -ne $nextCount -or
        ([regex]::Matches($generated, 'if \(g[0-9]+\.ok\) \{ v[0-9]+ = g[0-9]+\.value; goto b[0-9]+; \} else goto b[0-9]+;')).Count -ne $nextCount) {
        throw "XLIR generator next must observe its outcome separately from yielded/end: $($case.Name)"
    }
    if ( $case.Name -eq "sem_generator_function_param" -and
         (([regex]::Matches($generated, "bool c[0-9]+ =")).Count -ne 1 -or
          ([regex]::Matches($generated,
              "if \(c[0-9]+\) goto gp[0-9]+; else goto gp[0-9]+;")).Count -ne 1) ) {
        throw "XLIR closure-parameter generator C lost its exact boolean call carrier"
    }
    if ( $case.Name -eq "cgen_async_generator" -and
         (([regex]::Matches($generated,
             "v[0-9]+ = xlir_async_start_[0-9]+\(")).Count -ne
                $case.BlockingAwaitCount -or
          ([regex]::Matches($generated,
             "v[0-9]+ = xlir_future_await_i64\(v[0-9]+\);")).Count -ne
                $case.BlockingAwaitCount) ) {
        throw "XLIR async generator C is not a mechanical rendering of async-start/await ABI"
    }
    foreach ( $forbidden in @("XL6_Generator", "xrtCoCreate", "xcoro")) {
        if ( $generated.Contains($forbidden) ) {
            throw "XLIR generator C leaked legacy coroutine machinery: $forbidden"
        }
    }

    $selectedHost = if ( $case.Program ) { $programHostSource } else { $hostSource }
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" -DXLIR_GENERATED_MODULE_PREINCLUDED=1 `"$selectedHost`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR generator strict generated-C runtime failed: $($case.Name)"
    }
}

Write-Output "XLIR generator machine gate ok: analyzed machine/entry/block/frame/suspension/action JSON and text dumps, exact value/string/closure capture and item ownership ABI, explicit async-start/blocking-await execution, scalar and managed GET/TAKE consumers, byte-for-byte deterministic mechanical C, legacy coroutine exclusion, -Werror, and runtime semantics."
exit 0
