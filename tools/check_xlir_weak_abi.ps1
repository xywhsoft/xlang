param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$compiler = Join-Path $Root "release\xl.exe"
$fixture = Join-Path $Root "test\cases\protocol_weak_lifecycle.xl"
$assignmentFixture = Join-Path $Root "test\cases\hir_nullable_types_check.xl"
$classFixture = Join-Path $Root "test\cases\weak_class_lifecycle.xl"
$entryHost = Join-Path $Root "test\cases\xlir_program_entry_diagnostic_host.c"
$nullArgsHost = Join-Path $Root "test\cases\xlir_program_null_args_host.c"
$xrtDeclaration = Join-Path $Root "release\include\xrt_decl.h"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_weak_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_weak_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_weak_abi.generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_weak_abi.generated_again.c"
$exePath = Join-Path $outputDir "xlir_weak_abi.exe"
$assignmentTypedPath = Join-Path $outputDir "xlir_weak_assignment.typed.json"
$assignmentLoweredPath = Join-Path $outputDir "xlir_weak_assignment.lowered.json"
$assignmentGeneratedPath = Join-Path $outputDir "xlir_weak_assignment.generated.c"
$assignmentExePath = Join-Path $outputDir "xlir_weak_assignment.exe"
$classLoweredPath = Join-Path $outputDir "xlir_weak_class.lowered.json"
$classGeneratedPath = Join-Path $outputDir "xlir_weak_class.generated.c"
$classExePath = Join-Path $outputDir "xlir_weak_class.exe"

foreach ( $path in @(
    $compiler, $fixture, $assignmentFixture, $classFixture,
    $entryHost, $nullArgsHost,
    $xrtDeclaration
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR weak ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure)
{
    & $compiler @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

function Read-Json([string]$Path)
{
    return [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8) |
        ConvertFrom-Json
}

Invoke-Xl @("ir", $fixture, "--format", "json", "--emit", $typedPath) `
    "XLIR weak Typed IR failed"
Invoke-Xl @("ir", $fixture, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR weak lowering failed"
$typed = Read-Json $typedPath
$lowered = Read-Json $loweredPath

Invoke-Xl @("ir", $assignmentFixture, "--format", "json", "--emit", $assignmentTypedPath) `
    "XLIR strong-to-weak assignment Typed IR failed"
Invoke-Xl @("ir", $assignmentFixture, "--lowered", "--format", "json", "--emit", $assignmentLoweredPath) `
    "XLIR strong-to-weak assignment lowering failed"
$assignmentTyped = Read-Json $assignmentTypedPath
$assignmentLowered = Read-Json $assignmentLoweredPath
Invoke-Xl @("ir", $classFixture, "--lowered", "--format", "json", "--emit", $classLoweredPath) `
    "XLIR weak class/generator lowering failed"
$classLowered = Read-Json $classLoweredPath

$namedType = @($typed.types | Where-Object {
    $_.name -eq "Named" -and $_.kind -eq 10 -and $_.cAbi -eq 26 -and
    $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 2
})
$weakType = @($typed.types | Where-Object {
    $_.name -eq "weak" -and $_.kind -eq 9 -and $_.cAbi -eq 26 -and
    $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 2 -and
    @($_.params).Count -eq 1
})
$nullableType = @($typed.types | Where-Object {
    $_.name -eq "nullable" -and $_.kind -eq 8 -and $_.cAbi -eq 26 -and
    $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 2 -and
    @($_.params).Count -eq 1
})
if ( $namedType.Count -ne 1 -or $weakType.Count -ne 1 -or
     $nullableType.Count -ne 1 -or
     [int]$weakType[0].params[0] -ne [int]$namedType[0].id -or
     [int]$nullableType[0].params[0] -ne [int]$namedType[0].id ) {
    throw "Typed XLIR weak/nullable nominal types lost their exact managed xvalue ABI"
}

$weakInitializers = @($typed.valueInitializers | Where-Object {
    $_.type -eq $weakType[0].id -and
    $_.runtimeOpName -eq "xvalue-weak-create" -and
    $_.errorAbiName -eq "may-raise"
})
if ( $weakInitializers.Count -ne 2 ) {
    throw "Typed XLIR did not expose both strong-to-weak initializers as may-raise runtime operations"
}
$weakLocks = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "weak-lock" -and $_.admissionName -eq "verified" -and
    $_.receiverType -eq $weakType[0].id -and
    $_.resultType -eq $nullableType[0].id -and
    $_.resultAbiName -eq "owned" -and $_.errorAbiName -eq "no-raise" -and
    $_.operandCount -eq 1
})
if ( $weakLocks.Count -ne 2 ) {
    throw "Typed XLIR weak.lock() calls lost their exact receiver/result/ownership ABI"
}

$weakCreates = @($lowered.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-weak-create" -and
    $_.type -eq $weakType[0].id -and $_.operandCount -eq 1
})
$loweredLocks = @($lowered.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-weak-lock" -and
    $_.type -eq $nullableType[0].id -and $_.operandCount -eq 1
})
if ( $weakCreates.Count -ne 2 -or $loweredLocks.Count -ne 2 ) {
    throw "Lowered XLIR did not preserve the exact weak create/lock runtime operations"
}
foreach ( $create in $weakCreates ) {
    $test = @($lowered.instructions | Where-Object {
        $_.index -eq ([int]$create.index + 1) -and
        $_.kindName -eq "error-test" -and
        $_.origin -eq $create.origin -and
        @($_.operands).Count -eq 1 -and
        [int]$_.operands[0] -eq [int]$create.result
    })
    if ( $test.Count -ne 1 ) {
        throw "Lowered XLIR weak creation is not followed by its exact error-result test"
    }
}

$weakAssignments = @($assignmentTyped.nodes | Where-Object {
    $_.kindName -eq "assign" -and
    $_.implicitConversionRuntimeOpName -eq "xvalue-weak-create" -and
    $_.implicitConversionErrorAbiName -eq "may-raise"
})
if ( $weakAssignments.Count -ne 1 ) {
    throw "Typed XLIR did not expose strong-to-weak assignment as one exact may-raise RuntimeOp"
}
$assignmentExitPlans = @($assignmentTyped.exitPlans | Where-Object {
    $_.origin -eq $weakAssignments[0].id -and $_.kind -eq 4
})
if ( $assignmentExitPlans.Count -ne 1 ) {
    throw "Typed XLIR strong-to-weak assignment does not own one exact THROW ExitPlan"
}
$assignmentCreates = @($assignmentLowered.instructions | Where-Object {
    $_.origin -eq $weakAssignments[0].id -and
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-weak-create" -and $_.operandCount -eq 1
})
if ( $assignmentCreates.Count -ne 1 ) {
    throw "Lowered XLIR lost the strong-to-weak assignment RuntimeOp"
}
$assignmentTest = @($assignmentLowered.instructions | Where-Object {
    $_.index -eq ([int]$assignmentCreates[0].index + 1) -and
    $_.kindName -eq "error-test" -and
    $_.origin -eq $weakAssignments[0].id -and
    @($_.operands).Count -eq 1 -and
    [int]$_.operands[0] -eq [int]$assignmentCreates[0].result
})
if ( $assignmentTest.Count -ne 1 ) {
    throw "Lowered XLIR strong-to-weak assignment is not followed by its exact error test"
}

$classWeakType = @($classLowered.types | Where-Object {
    $_.name -eq "weak" -and $_.kind -eq 9 -and $_.cAbi -eq 26 -and
    $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 2
})
$weakGeneratorTarget = @($classLowered.callTargets | Where-Object {
    $_.kind -eq 5 -and $_.generatorYieldAbiName -eq "copy-i64" -and
    @($_.parameterTypes).Count -eq 1 -and
    @($_.generatorCaptureAbis).Count -eq 1 -and
    $_.generatorCaptureAbis[0] -eq "retain-xvalue"
})
$weakGeneratorMachine = @($classLowered.generatorMachines | Where-Object {
    $_.statusName -eq "analyzed" -and $_.yieldAbiName -eq "copy-i64"
})
$weakGeneratorFrame = @($classLowered.generatorFrameSlots | Where-Object {
    $_.residenceName -eq "launch-parameter" -and
    $_.transferAbiName -eq "retain-xvalue" -and
    $_.destroyAbiName -eq "release-xvalue"
})
if ( $classWeakType.Count -ne 1 -or $weakGeneratorTarget.Count -ne 1 -or
     $weakGeneratorMachine.Count -ne 1 -or $weakGeneratorFrame.Count -ne 1 -or
     [int]$weakGeneratorTarget[0].parameterTypes[0] -ne [int]$classWeakType[0].id -or
     [int]$weakGeneratorFrame[0].type -ne [int]$classWeakType[0].id -or
     [int]$weakGeneratorMachine[0].id -ne [int]$weakGeneratorTarget[0].generatorMachine ) {
    throw "Lowered XLIR weak generator lost its exact capture/frame/item machine ABI"
}

Invoke-Xl @("cgen", $fixture, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR weak Cgen failed"
Invoke-Xl @("cgen", $fixture, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR weak deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR weak generated C is not deterministic"
}
$generated = [IO.File]::ReadAllText($generatedPath, [Text.Encoding]::UTF8)
foreach ( $required in @(
    "extern xvalue* xrtValueWeakRef(const xvalue*);",
    "extern xvalue* xrtValueWeakRefLock(const xvalue*);",
    "xlir_xvalue_weak_create",
    "xlir_xvalue_weak_lock"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR weak generated C lost the public XRT Value weak ABI: $required"
    }
}
foreach ( $forbidden in @(
    "xrtValueGetRuntimeObject", "xrtValueRuntimeObjectTake", "xrtWeakInit"
) ) {
    if ( $generated.Contains($forbidden) ) {
        throw "XLIR weak generated C leaked the legacy runtime-object weak bridge: $forbidden"
    }
}
$xrtText = [IO.File]::ReadAllText($xrtDeclaration, [Text.Encoding]::UTF8)
foreach ( $required in @("xrtValueWeakRef", "xrtValueWeakRefExpired", "xrtValueWeakRefLock") ) {
    if ( ! $xrtText.Contains($required) ) {
        throw "XRT declaration snapshot does not expose the Value weak ABI: $required"
    }
}

$compileCommand = "call test\suites\common.bat && " +
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror " +
    "-include `"$generatedPath`" -DXLIR_GENERATED_MODULE_PREINCLUDED=1 " +
    "`"$entryHost`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
$runtimeOutput = & $env:ComSpec /d /v:on /c $compileCommand 2>&1
if ( $LASTEXITCODE -ne 0 ) {
    $runtimeOutput | ForEach-Object { Write-Host $_ }
    throw "XLIR weak strict-C/runtime gate failed"
}
if ( (@($runtimeOutput | Where-Object {
          $_.ToString().Contains("protocol weak lifecycle ok")
      })).Count -ne 1 ) {
    $runtimeOutput | ForEach-Object { Write-Host $_ }
    throw "XLIR weak runtime output did not prove live-lock and expiration semantics"
}

Invoke-Xl @("cgen", $assignmentFixture, "--backend", "xlir", "--emit", $assignmentGeneratedPath) `
    "XLIR strong-to-weak assignment Cgen failed"
$assignmentCompileCommand = "call test\suites\common.bat && " +
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror " +
    "-include `"$assignmentGeneratedPath`" -DXLIR_GENERATED_MODULE_PREINCLUDED=1 " +
    "`"$nullArgsHost`" !XL6_CGEN_LINKS! -o `"$assignmentExePath`" && `"$assignmentExePath`""
$assignmentRuntimeOutput = & $env:ComSpec /d /v:on /c $assignmentCompileCommand 2>&1
if ( $LASTEXITCODE -ne 0 ) {
    $assignmentRuntimeOutput | ForEach-Object { Write-Host $_ }
    throw "XLIR strong-to-weak assignment strict-C/runtime gate failed"
}

Invoke-Xl @("cgen", $classFixture, "--backend", "xlir", "--emit", $classGeneratedPath) `
    "XLIR weak class/generator Cgen failed"
$classCompileCommand = "call test\suites\common.bat && " +
    "!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror " +
    "-include `"$classGeneratedPath`" -DXLIR_GENERATED_MODULE_PREINCLUDED=1 " +
    "`"$nullArgsHost`" !XL6_CGEN_LINKS! -o `"$classExePath`" && `"$classExePath`""
$classRuntimeOutput = & $env:ComSpec /d /v:on /c $classCompileCommand 2>&1
if ( $LASTEXITCODE -ne 0 ) {
    $classRuntimeOutput | ForEach-Object { Write-Host $_ }
    throw "XLIR weak class/generator strict-C/runtime gate failed"
}

Write-Output "XLIR weak ABI gate ok: exact managed types, verified initializer/assignment/lock IR, public XRT Value weak refs, generator capture/frame ABI, deterministic strict C, and lifecycle runtime."
exit 0
