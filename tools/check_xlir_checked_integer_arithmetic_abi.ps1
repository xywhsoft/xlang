param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_checked_integer_arithmetic_abi.xl"
$compoundSource = Join-Path $Root "test\cases\xlir_checked_integer_compound_error_abi.xl"
$continuationSource = Join-Path $Root "test\cases\xlir_checked_integer_continuation_abi.xl"
$programHost = Join-Path $Root "test\cases\xlir_program_entry_host.c"
$moduleHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\test"
$loweredPath = Join-Path $outputDir "xlir_checked_integer_arithmetic_abi.lowered.json"
$compoundLoweredPath = Join-Path $outputDir "xlir_checked_integer_compound_error_abi.lowered.json"
$continuationLoweredPath = Join-Path $outputDir "xlir_checked_integer_continuation_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_checked_integer_arithmetic_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_checked_integer_arithmetic_abi_generated_again.c"
$compoundGeneratedPath = Join-Path $outputDir "xlir_checked_integer_compound_error_abi_generated.c"
$compoundGeneratedAgainPath = Join-Path $outputDir "xlir_checked_integer_compound_error_abi_generated_again.c"
$continuationGeneratedPath = Join-Path $outputDir "xlir_checked_integer_continuation_abi_generated.c"
$continuationGeneratedAgainPath = Join-Path $outputDir "xlir_checked_integer_continuation_abi_generated_again.c"
foreach ( $path in @(
    $xl, $source, $compoundSource, $continuationSource, $programHost, $moduleHost
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR checked integer arithmetic gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

function Test-LoweredCheckedIntegerArithmetic(
    [string]$Path,
    [hashtable]$ExpectedCounts,
    [int]$ExpectedTotal,
    [hashtable]$ExpectedTargets
) {
    $lowered = [System.IO.File]::ReadAllText(
        $Path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $operationNames = @(
        "integer-div-checked", "integer-mod-checked",
        "integer-shl-checked", "integer-shr-checked")
    $operations = @($lowered.instructions | Where-Object {
        $_.runtimeOpName -in $operationNames
    })
    if ( $operations.Count -ne $ExpectedTotal ) {
        throw "Lowered XLIR checked integer operation count is not canonical: $Path"
    }
    foreach ( $name in $operationNames ) {
        $expected = if ( $ExpectedCounts.ContainsKey($name) ) {
            [int]$ExpectedCounts[$name]
        } else { 0 }
        if ( @($operations | Where-Object { $_.runtimeOpName -eq $name }).Count -ne
             $expected ) {
            throw "Checked integer operation partition is incomplete: $name"
        }
    }
    foreach ( $target in $ExpectedTargets.Keys ) {
        if ( @($operations | Where-Object {
                $_.integerTargetName -eq $target
             }).Count -ne [int]$ExpectedTargets[$target] ) {
            throw "Checked integer target width/signedness partition is incomplete: $target"
        }
    }
    foreach ( $operation in $operations ) {
        $origin = @($lowered.nodes | Where-Object { $_.id -eq $operation.origin })
        $next = @($lowered.instructions | Where-Object {
            $_.index -eq $operation.index + 1
        })
        if ( $operation.operandCount -ne 2 -or $operation.result -eq 0 -or
             $operation.integerTargetName -eq "none" -or
             $origin.Count -ne 1 -or
             $origin[0].operatorErrorAbiName -ne "may-raise" -or
             $next.Count -ne 1 -or $next[0].kindName -ne "error-test" -or
             $next[0].operandCount -ne 1 -or
             $next[0].operands[0] -ne $operation.result ) {
            throw "Checked integer operation has no exact target, Typed effect and adjacent ERROR_TEST ABI"
        }
    }
    if ( @($lowered.instructions | Where-Object {
            $_.kindName -eq "binary" -and $_.origin -in @($operations.origin)
         }).Count -ne 0 ) {
        throw "Checked integer semantics leaked into a physical BINARY instruction"
    }
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR checked integer arithmetic lowering failed"
Invoke-Xl @("ir", $compoundSource, "--lowered", "--format", "json", "--emit", $compoundLoweredPath) `
    "XLIR checked integer compound lowering failed"
Invoke-Xl @("ir", $continuationSource, "--lowered", "--format", "json", "--emit", $continuationLoweredPath) `
    "XLIR checked integer continuation lowering failed"

Test-LoweredCheckedIntegerArithmetic $loweredPath @{
    "integer-div-checked" = 5
    "integer-mod-checked" = 4
    "integer-shl-checked" = 2
    "integer-shr-checked" = 4
} 15 @{
    "i64" = 11
    "i8" = 1
    "u64" = 2
    "u8" = 1
}
Test-LoweredCheckedIntegerArithmetic $compoundLoweredPath @{
    "integer-div-checked" = 1
    "integer-mod-checked" = 1
} 2 @{
    "i64" = 2
}

$continuation = [System.IO.File]::ReadAllText(
    $continuationLoweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$asyncMachines = @($continuation.asyncMachines | Where-Object {
    $_.statusName -eq "analyzed"
})
$generatorMachines = @($continuation.generatorMachines | Where-Object {
    $_.statusName -eq "analyzed"
})
if ( $asyncMachines.Count -ne 1 -or $generatorMachines.Count -ne 1 -or
     $asyncMachines[0].suspensionCount -ne 1 -or
     $generatorMachines[0].suspensionCount -ne 2 ) {
    throw "Checked integer continuation fixture has no exact async/generator machine partition"
}
function Get-MachineInstructions(
    [object[]]$Blocks,
    [object]$Machine,
    [object[]]$Instructions
) {
    $result = @()
    foreach ( $block in @($Blocks | Where-Object { $_.machine -eq $Machine.id }) ) {
        $first = [int]$block.firstInstruction
        $count = [int]$block.instructionCount
        if ( $count -ne 0 ) {
            $result += $Instructions[$first..($first + $count - 1)]
        }
    }
    return @($result)
}
$asyncInstructions = @(Get-MachineInstructions `
    @($continuation.asyncMachineBlocks) $asyncMachines[0] @($continuation.instructions))
$generatorInstructions = @(Get-MachineInstructions `
    @($continuation.generatorMachineBlocks) $generatorMachines[0] @($continuation.instructions))
$asyncChecked = @($asyncInstructions | Where-Object {
    $_.runtimeOpName -like "integer-*-checked"
})
$generatorChecked = @($generatorInstructions | Where-Object {
    $_.runtimeOpName -like "integer-*-checked"
})
$nestedLaunch = @($asyncInstructions | Where-Object { $_.kindName -eq "async-start" })
if ( $asyncChecked.Count -ne 1 -or
     $asyncChecked[0].runtimeOpName -ne "integer-div-checked" -or
     $asyncChecked[0].integerTargetName -ne "i64" -or
     $generatorChecked.Count -ne 2 -or
     @($generatorChecked | Where-Object {
        $_.runtimeOpName -eq "integer-div-checked"
     }).Count -ne 1 -or
     @($generatorChecked | Where-Object {
        $_.runtimeOpName -eq "integer-shl-checked"
     }).Count -ne 1 -or
     $nestedLaunch.Count -ne 1 ) {
    throw "Checked integer RuntimeOps are not owned by the exact continuation machines"
}
$launchErrorTest = @($asyncInstructions | Where-Object {
    $_.kindName -eq "error-test" -and $_.index -eq $nestedLaunch[0].index + 1 -and
    $_.operandCount -eq 1 -and $_.operands[0] -eq $nestedLaunch[0].result
})
if ( $launchErrorTest.Count -ne 1 ) {
    throw "Nested async launch has no adjacent Future-result ERROR_TEST contract"
}
foreach ( $operation in @($asyncChecked + $generatorChecked) ) {
    $next = @($continuation.instructions | Where-Object {
        $_.index -eq $operation.index + 1
    })
    if ( $next.Count -ne 1 -or $next[0].kindName -ne "error-test" -or
         $next[0].operandCount -ne 1 -or
         $next[0].operands[0] -ne $operation.result ) {
        throw "Continuation checked integer operation lost its adjacent ERROR_TEST CFG"
    }
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR checked integer arithmetic Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR checked integer arithmetic deterministic Cgen failed"
Invoke-Xl @("cgen", $compoundSource, "--backend", "xlir", "--emit", $compoundGeneratedPath) `
    "XLIR checked integer compound Cgen failed"
Invoke-Xl @("cgen", $compoundSource, "--backend", "xlir", "--emit", $compoundGeneratedAgainPath) `
    "XLIR checked integer compound deterministic Cgen failed"
Invoke-Xl @("cgen", $continuationSource, "--backend", "xlir", "--emit", $continuationGeneratedPath) `
    "XLIR checked integer continuation Cgen failed"
Invoke-Xl @("cgen", $continuationSource, "--backend", "xlir", "--emit", $continuationGeneratedAgainPath) `
    "XLIR checked integer continuation deterministic Cgen failed"
foreach ( $pair in @(
    @($generatedPath, $generatedAgainPath),
    @($compoundGeneratedPath, $compoundGeneratedAgainPath),
    @($continuationGeneratedPath, $continuationGeneratedAgainPath)
) ) {
    if ( ! [System.Linq.Enumerable]::SequenceEqual(
            [byte[]][System.IO.File]::ReadAllBytes($pair[0]),
            [byte[]][System.IO.File]::ReadAllBytes($pair[1])
        ) ) {
        throw "XLIR checked integer arithmetic C output is not deterministic"
    }
}
$continuationGenerated = [System.IO.File]::ReadAllText(
    $continuationGeneratedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef struct xlir_result_future { bool ok; struct xfuture* value; } xlir_result_future;",
    "static xlir_result_future xlir_async_start_1(int64_t a0);",
    "static xfuture* xlir_async_start_raw_1(int64_t a0)",
    "xlir_async_machine_resume_2_0",
    "xlir_generator_resume_3",
    "xlir_result_future r",
    " = xlir_async_start_1(v",
    "xlir_i64_binary_checked("
) ) {
    if ( ! $continuationGenerated.Contains($required) ) {
        throw "XLIR checked integer continuation C emission is incomplete: $required"
    }
}
# The async checked operation appears in both the start path and its resume
# callback; the generator contributes two more physical sites.  Typed/lowered
# checks above prove that these are three semantic RuntimeOps.
if ( $continuationGenerated -notmatch
     'xlir_result_future (?<launch>r[0-9]+) = xlir_async_start_1\(v[0-9]+\);\s*if \(\k<launch>\.ok\) \{ v[0-9]+ = \k<launch>\.value; goto ap[0-9]+; \} else goto ap[0-9]+;' -or
     ([regex]::Matches($continuationGenerated,
        'xlir_i64_binary_checked\(').Count - 1) -ne 4 ) {
    throw "XLIR continuation C does not mechanically preserve nested-launch and checked-operation ABIs"
}
$generated = [System.IO.File]::ReadAllText(
    $generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_integer_target_width(",
    "xlir_integer_normalize_i64(",
    "xlir_i64_binary_checked(",
    "xlir_u64_binary_checked(",
    "INT64_MIN && right == -1",
    "ZeroDivisionError",
    "shift count is outside the left operand width"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR checked integer arithmetic C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $generatedPath $programHost !XL6_CGEN_LINKS! -o build\test\xlir_checked_integer_arithmetic_abi_xlir.exe && build\test\xlir_checked_integer_arithmetic_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR checked integer arithmetic strict-C runtime failed"
    }

    $compoundCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $compoundGeneratedPath $programHost !XL6_CGEN_LINKS! -o build\test\xlir_checked_integer_compound_error_abi_xlir.exe && build\test\xlir_checked_integer_compound_error_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $compoundCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR checked integer compound strict-C runtime failed"
    }

    $continuationCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $continuationGeneratedPath $moduleHost !XL6_CGEN_LINKS! -o build\test\xlir_checked_integer_continuation_abi_xlir.exe && build\test\xlir_checked_integer_continuation_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $continuationCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR checked integer continuation strict-C runtime failed"
    }

} finally {
    Pop-Location
}

Write-Output "XLIR checked integer arithmetic ABI gate ok: twenty exact div/mod/shift RuntimeOps across i8/u8/i64/u64, Typed may-raise effects, adjacent ERROR_TEST CFG, compound assignment errors, nested async launch plus async/generator machine ownership, and deterministic strict-C runtime semantics."
