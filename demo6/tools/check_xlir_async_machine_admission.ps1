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

# This is the complete representative continuation-machine inventory.  Cases
# with no suspension machine (for example a ready Future) remain in the list so
# an accidental lowering-role change is visible, but every analyzed machine is
# required to have one direct callback per canonical suspension.
$fixtures = @(
    "xlir_async_cfg_liveness",
    "xlir_async_class_method",
    "xlir_checked_integer_continuation_abi",
    "xlir_async_closure_result",
    "xlir_async_future_capture_chain",
    "xlir_async_future_capture_chain_await_right",
    "xlir_async_future_capture_chain_mixed",
    "xlir_async_future_direct_call",
    "xlir_async_future_direct_call_capture",
    "xlir_async_future_direct_call_catch",
    "xlir_async_future_direct_string_capture",
    "xlir_async_future_direct_xvalue_capture",
    "xlir_async_future_direct_xvalue_differential",
    "xlir_async_raw_pointer_abi",
    "xlir_async_future_forward",
    "xlir_async_future_preawait_local",
    "xlir_async_future_preawait_parameter_local",
    "xlir_async_future_scalar_mixed_capture_chain",
    "xlir_async_future_string_chain",
    "xlir_async_future_then_float",
    "xlir_async_future_then_scalar",
    "xlir_async_future_then_scalar_reversed",
    "xlir_async_future_then_string",
    "xlir_async_future_then_string_capture_chain",
    "xlir_async_future_then_string_capture_chain_reversed",
    "xlir_async_future_then_string_immediate",
    "xlir_async_future_then_string_local",
    "xlir_async_future_then_string_mixed_capture_chain",
    "xlir_async_future_then_string_parameter_chain",
    "xlir_async_future_then_string_parameter_chain_reversed",
    "xlir_async_future_then_string_reversed",
    "xlir_async_future_then_xvalue",
    "xlir_async_future_then_xvalue_local_parameter",
    "xlir_async_future_three_awaits",
    "xlir_async_future_three_awaits_mixed",
    "xlir_async_future_three_strings",
    "xlir_async_future_two_awaits",
    "xlir_async_future_xvalue_capture_chain",
    "xlir_async_future_xvalue_differential",
    "xlir_async_future_xvalue_local_parameter_differential",
    "xlir_closure_future_capture",
    "xlir_nullable_async_continuation",
    "hir_flow_value_ownership_matrix",
    "stdlib_net_fast_path",
    "stdlib_net_send_file",
    "stdlib_file_async",
    "stdlib_process_extended",
    "stdlib_tls_server_loopback"
)

# These production-sized fixtures protect instruction families that small
# continuation probes do not exercise together.  Requirements are evaluated
# only over canonical analyzed-machine blocks, never over unrelated sync code.
$productionRequirements = @{
    "hir_flow_value_ownership_matrix" = @{
        Kinds = @("null", "nullable-wrap", "protocol-call", "await")
        RuntimeOps = @(
            "xvalue-int-map-new",
            "xvalue-int-map-set-i64-new",
            "xvalue-object-new",
            "xvalue-object-get-literal",
            "xvalue-weak-create",
            "xvalue-weak-lock"
        )
        CMarkers = @(
            "static char* xlir_pdispatch_1(xvalue* receiver);",
            "xrtValueIntMap()",
            "xrtValueObject()",
            "xlir_xvalue_weak_create"
        )
    }
    "stdlib_net_fast_path" = @{
        Kinds = @("await", "call", "error-test")
        RuntimeOps = @(
            "xvalue-native-handle-take",
            "xvalue-object-get-literal-native-handle-borrow"
        )
        CMarkers = @("xlir_native_handle_take_s", "xlir_async_machine_resume_")
    }
    "stdlib_net_send_file" = @{
        Kinds = @("move", "async-start", "null-test")
        RuntimeOps = @("string-concat", "xvalue-array-append-new")
        CMarkers = @("xlir_async_start_", "xlir_async_machine_resume_")
    }
    "stdlib_file_async" = @{
        Kinds = @("await", "call", "error-test")
        RuntimeOps = @()
        CMarkers = @(
            "xlir_future_await_raw_pointer",
            "*(void* const*)input->Value"
        )
    }
    "stdlib_process_extended" = @{
        # The raw-pointer await is in module initialization rather than an
        # async worker; keep it here as a production compile-only ABI probe.
        Kinds = @()
        RuntimeOps = @()
        CMarkers = @("xlir_future_await_raw_pointer")
    }
    "stdlib_tls_server_loopback" = @{
        Kinds = @("await", "null-test", "error-test")
        RuntimeOps = @("xvalue-class-identity-bind", "i64-to-string")
        CMarkers = @("xlir_class_identity_bind_t", "xlir_i64_to_string")
    }
}

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    throw "XLIR async machine admission compiler is missing: $xl"
}
New-Item -ItemType Directory -Force -Path $outputDir,$generatedDir | Out-Null

$machineCount = 0
$suspensionCount = 0
foreach ( $fixture in $fixtures ) {
    $source = Join-Path $Root ("test\cases\{0}.xl" -f $fixture)
    $jsonPath = Join-Path $outputDir ("{0}.machine-admission.json" -f $fixture)
    $cPath = Join-Path $generatedDir ("{0}_generated.c" -f $fixture)
    $repeatPath = Join-Path $outputDir ("{0}.machine-admission.repeat.c" -f $fixture)
    if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
        throw "XLIR async machine admission source is missing: $source"
    }

    & $xl ir $source --lowered --format json --emit $jsonPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async machine lowered JSON failed: $fixture" }
    & $xl cgen $source --backend xlir --emit $cPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async machine C generation failed: $fixture" }
    & $xl cgen $source --backend xlir --emit $repeatPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async machine repeated C generation failed: $fixture" }

    $left = [System.IO.File]::ReadAllBytes($cPath)
    $right = [System.IO.File]::ReadAllBytes($repeatPath)
    if ( $left.Length -ne $right.Length -or
         -not [System.Linq.Enumerable]::SequenceEqual([byte[]]$left, [byte[]]$right) ) {
        throw "XLIR async machine generated C is not byte-for-byte deterministic: $fixture"
    }

    $ir = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) |
        ConvertFrom-Json
    $machines = @($ir.asyncMachines)
    $analyzed = @($machines | Where-Object { $_.statusName -eq "analyzed" })
    $expectedCallbacks = 0
    $machineInstructionIndices =
        [System.Collections.Generic.HashSet[int]]::new()
    foreach ( $machine in $analyzed ) {
        $target = @($ir.callTargets | Where-Object {
            [int64]$_.asyncMachine -eq [int64]$machine.id
        })
        if ( $target.Count -ne 1 -or [int]$machine.suspensionCount -lt 1 ) {
            throw "XLIR analyzed async machine has no unique target/suspension program: $fixture"
        }
        $expectedCallbacks += [int]$machine.suspensionCount
        for ( $blockIndex = 0;
              $blockIndex -lt [int]$machine.blockCount;
              ++$blockIndex ) {
            $machineBlock = $ir.asyncMachineBlocks[
                [int]$machine.firstBlock + $blockIndex]
            for ( $instructionIndex = [int]$machineBlock.firstInstruction;
                  $instructionIndex -lt
                    [int]$machineBlock.firstInstruction +
                    [int]$machineBlock.instructionCount;
                  ++$instructionIndex ) {
                [void]$machineInstructionIndices.Add($instructionIndex)
            }
        }
    }

    if ( $productionRequirements.ContainsKey($fixture) ) {
        $requirement = $productionRequirements[$fixture]
        $machineInstructions = @($ir.instructions | Where-Object {
            $machineInstructionIndices.Contains([int]$_.index)
        })
        foreach ( $kind in $requirement.Kinds ) {
            if ( -not @($machineInstructions.kindName).Contains($kind) ) {
                throw "XLIR production async machine lost required instruction kind: $fixture ($kind)"
            }
        }
        foreach ( $runtimeOp in $requirement.RuntimeOps ) {
            if ( -not @($machineInstructions.runtimeOpName).Contains($runtimeOp) ) {
                throw "XLIR production async machine lost required runtime ABI: $fixture ($runtimeOp)"
            }
        }
    }
    if ( $fixture -eq "xlir_async_raw_pointer_abi" ) {
        $rawAwaits = @($ir.instructions | Where-Object {
            $_.kindName -eq "await" -and
            $_.asyncResultAbiName -eq "copy-raw-pointer"
        })
        $rawSuspensions = @($ir.asyncSuspensions | Where-Object {
            $_.resultAbiName -eq "copy-raw-pointer"
        })
        if ( $rawAwaits.Count -lt 2 -or $rawSuspensions.Count -ne 1 ) {
            throw "XLIR raw-pointer Future lost explicit await/suspension ABI"
        }
        foreach ( $await in $rawAwaits ) {
            $type = @($ir.types | Where-Object {
                [int]$_.id -eq [int]$await.type
            })
            if ( $type.Count -ne 1 -or [int]$type[0].cAbi -ne 28 -or
                 [int]$type[0].ownershipAbi -ne 1 ) {
                throw "XLIR raw-pointer await type is not trivial raw-pointer ABI"
            }
        }
    }

    $c = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
    $callbacks = [regex]::Matches(
        $c, 'static void xlir_async_machine_resume_\d+_\d+\(const xfutureresult\* input').Count
    $legacyMarkers = @(
        "xlir_async_chain_frame_",
        "xlir_async_capture_chain_frame_",
        "xlir_async_string_capture_chain_",
        "xlir_async_xvalue_capture_chain_",
        "xlir_async_catch_default_"
    )
    foreach ( $marker in $legacyMarkers ) {
        if ( $c.Contains($marker) ) {
            throw "XLIR analyzed async machine fell back to legacy continuation rendering: $fixture ($marker)"
        }
    }
    if ( $callbacks -ne $expectedCallbacks ) {
        throw "XLIR direct callback count does not match canonical suspensions: $fixture ($callbacks/$expectedCallbacks)"
    }
    if ( $productionRequirements.ContainsKey($fixture) ) {
        foreach ( $marker in $productionRequirements[$fixture].CMarkers ) {
            if ( -not $c.Contains($marker) ) {
                throw "XLIR production async machine C lost required mechanical emission: $fixture ($marker)"
            }
        }
    }
    if ( $fixture -eq "xlir_async_raw_pointer_abi" ) {
        foreach ( $marker in @(
            "xlir_future_await_raw_pointer",
            "*(void* const*)input->Value"
        ) ) {
            if ( -not $c.Contains($marker) ) {
                throw "XLIR raw-pointer Future C lost explicit payload ABI: $marker"
            }
        }
        if ( $c.Contains("*(const void**)input->Value") -or
             $c.Contains("xrtPromiseReject(xpromise*, const void*)") ) {
            throw "XLIR raw-pointer Future C contains an ambiguous pointer/error ABI"
        }
    }
    $machineCount += $analyzed.Count
    $suspensionCount += $expectedCallbacks
}

# The curated inventory above carries deep per-family assertions and runtime
# hosts.  In addition, discover every XLIR-prefixed async/await/yield fixture so
# a newly added supported program cannot silently remain outside admission.
$supportedAsyncFixtures = @(Get-ChildItem -LiteralPath (Join-Path $Root "test\cases") `
    -File -Filter "xlir_*.xl" | Sort-Object Name | Where-Object {
        Select-String -LiteralPath $_.FullName `
            -Pattern '\b(async\s+func|await|yield)\b' -Quiet
    })
$fullMachineCount = 0
$fullSuspensionCount = 0
$fullLegacyMarkers = @(
    "xlir_async_chain_frame_",
    "xlir_async_capture_chain_frame_",
    "xlir_async_string_capture_chain_",
    "xlir_async_xvalue_capture_chain_",
    "xlir_async_catch_default_"
)
foreach ( $source in $supportedAsyncFixtures ) {
    $fixture = $source.BaseName
    $jsonPath = Join-Path $outputDir ("{0}.full-async-inventory.json" -f $fixture)
    $cPath = Join-Path $outputDir ("{0}.full-async-inventory.c" -f $fixture)
    & $xl ir $source.FullName --lowered --format json --emit $jsonPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR full async inventory lowering failed: $fixture"
    }
    & $xl cgen $source.FullName --backend xlir --emit $cPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR full async inventory C generation failed: $fixture"
    }
    $ir = [System.IO.File]::ReadAllText(
        $jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $machines = @($ir.asyncMachines)
    $analyzed = @($machines | Where-Object { $_.statusName -eq "analyzed" })
    if ( $analyzed.Count -ne $machines.Count ) {
        $statuses = @($machines | Where-Object { $_.statusName -ne "analyzed" } |
            ForEach-Object { "machine $($_.id):$($_.statusName)" }) -join ", "
        throw "XLIR full async inventory contains a non-analyzed machine: $fixture ($statuses)"
    }
    $analyzedIds = [System.Collections.Generic.HashSet[int]]::new()
    $expectedCallbacks = 0
    foreach ( $machine in $analyzed ) {
        [void]$analyzedIds.Add([int]$machine.id)
        $expectedCallbacks += [int]$machine.suspensionCount
    }
    $suspensions = @($ir.asyncSuspensions)
    if ( $expectedCallbacks -ne $suspensions.Count -or
         @($suspensions | Where-Object {
             -not $analyzedIds.Contains([int]$_.machine) -or
             $_.resultAbiName -eq "invalid" -or [int]$_.resultType -eq 0
         }).Count -ne 0 ) {
        throw "XLIR full async inventory has an orphaned or ambiguous suspension: $fixture"
    }
    $c = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
    $callbacks = [regex]::Matches(
        $c, 'static void xlir_async_machine_resume_\d+_\d+\(const xfutureresult\* input').Count
    if ( $callbacks -ne $expectedCallbacks ) {
        throw "XLIR full async inventory callback count does not match canonical suspensions: $fixture ($callbacks/$expectedCallbacks)"
    }
    foreach ( $marker in $fullLegacyMarkers ) {
        if ( $c.Contains($marker) ) {
            throw "XLIR full async inventory fell back to legacy continuation rendering: $fixture ($marker)"
        }
    }
    $fullMachineCount += $analyzed.Count
    $fullSuspensionCount += $suspensions.Count
}

$commands = @(
    "call test\suites\common.bat",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_async_future_direct_call_catch_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_future_direct_call_catch.machine-admission.exe",
    "build\tmp\xlir_async_future_direct_call_catch.machine-admission.exe",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_async_future_direct_string_capture_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_future_direct_string_capture.machine-admission.exe",
    "build\tmp\xlir_async_future_direct_string_capture.machine-admission.exe",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_closure_future_capture_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_closure_future_capture.machine-admission.exe",
    "build\tmp\xlir_closure_future_capture.machine-admission.exe",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_nullable_async_continuation_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_nullable_async_continuation.machine-admission.exe",
    "build\tmp\xlir_nullable_async_continuation.machine-admission.exe",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\test\hir_flow_value_ownership_matrix_generated.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\tmp\hir_flow_value_ownership_matrix.machine-admission.exe",
    "build\tmp\hir_flow_value_ownership_matrix.machine-admission.exe",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\test\xlir_checked_integer_continuation_abi_generated.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_checked_integer_continuation_abi.machine-admission.exe",
    "build\tmp\xlir_checked_integer_continuation_abi.machine-admission.exe",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_async_raw_pointer_abi_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_raw_pointer_abi.machine-admission.exe",
    "build\tmp\xlir_async_raw_pointer_abi.machine-admission.exe",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c build\test\stdlib_net_fast_path_generated.c -o build\tmp\stdlib_net_fast_path.machine-admission.o",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c build\test\stdlib_net_send_file_generated.c -o build\tmp\stdlib_net_send_file.machine-admission.o",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c build\test\stdlib_tls_server_loopback_generated.c -o build\tmp\stdlib_tls_server_loopback.machine-admission.o",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c build\test\stdlib_file_async_generated.c -o build\tmp\stdlib_file_async.machine-admission.o",
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c build\test\stdlib_process_extended_generated.c -o build\tmp\stdlib_process_extended.machine-admission.o"
)

Push-Location $Root
try {
    & $env:ComSpec /d /v:on /c ($commands -join " && ")
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR async machine admission strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output ("XLIR async machine admission gate ok: {0} analyzed machines / {1} canonical suspensions use direct verified machine Cgen across {2} representative fixtures; full supported XLIR async inventory covers {3} fixtures / {4} analyzed machines / {5} canonical suspensions with no non-analyzed machine, orphaned suspension or legacy renderer; byte-for-byte deterministic C, nested async launch, checked integer effects, catch recovery, closure/future ownership, tagged nullable/null construction, protocol dispatch, production ownership matrix and net/TLS runtime families, -Werror and runtime semantics." -f $machineCount, $suspensionCount, $fixtures.Count, $supportedAsyncFixtures.Count, $fullMachineCount, $fullSuspensionCount)
