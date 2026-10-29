param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_async_error_abi.xl"
$raceSource = Join-Path $Root "test\cases\xlir_async_error_race_abi.xl"
$allSource = Join-Path $Root "test\cases\xlir_task_all_error_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$diagnosticHostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$loweredJson = Join-Path $outputDir "xlir_async_error_abi.gate.lowered.json"
$generatedC = Join-Path $outputDir "xlir_async_error_abi.gate.c"
$generatedRepeat = Join-Path $outputDir "xlir_async_error_abi.gate.repeat.c"
$executable = Join-Path $outputDir "xlir_async_error_abi.gate.exe"
$raceLoweredJson = Join-Path $outputDir "xlir_async_error_race_abi.gate.lowered.json"
$raceGeneratedC = Join-Path $outputDir "xlir_async_error_race_abi.gate.c"
$raceExecutable = Join-Path $Root "build\test\xlir_async_error_race_abi.gate.exe"
$allTypedJson = Join-Path $outputDir "xlir_task_all_error_abi.gate.typed.json"
$allLoweredJson = Join-Path $outputDir "xlir_task_all_error_abi.gate.lowered.json"
$allGeneratedC = Join-Path $outputDir "xlir_task_all_error_abi.gate.c"
$allGeneratedRepeat = Join-Path $outputDir "xlir_task_all_error_abi.gate.repeat.c"
$allExecutable = Join-Path $Root "build\test\xlir_task_all_error_abi.gate.exe"

foreach ( $path in @(
    $xl, $source, $raceSource, $allSource,
    $hostSource, $diagnosticHostSource
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR async-error gate input is missing: $path"
    }
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-error lowered IR failed" }
$ir = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$targets = @($ir.callTargets | Where-Object {
    $_.kind -eq 4 -and $_.asyncResultAbiName -eq "retained-error"
})
if ( $targets.Count -ne 1 -or
     $targets[0].asyncDispatch -ne 1 -or
     @($targets[0].parameterAbis).Count -ne 1 -or
     $targets[0].parameterAbis[0] -ne 2 -or
     @($targets[0].asyncCaptureAbis).Count -ne 1 -or
     $targets[0].asyncCaptureAbis[0] -ne "retain-error" ) {
    throw "XLIR async-error CallTarget does not carry exact result/frame ownership ABI"
}

$awaits = @($ir.instructions | Where-Object {
    $_.kindName -eq "await" -and $_.asyncResultAbiName -eq "retained-error"
})
if ( $awaits.Count -ne 3 -or
     @($awaits | Where-Object { $_.type -eq 0 -or $_.result -eq 0 }).Count -ne 0 ) {
    throw "XLIR async-error repeated AWAIT does not carry retained-error ABI"
}

# The catch and callable signature normally share the canonical error type.  A
# frontend-registry peer remains legal only through an explicit VALUE rebind
# whose representation and TypeValueId are identical; ASYNC_START must never
# rely on a C pointer coincidence.
$parameterType = [int]$targets[0].parameterTypes[0]
$targetType = $ir.types | Where-Object { $_.id -eq $parameterType } |
    Select-Object -First 1
$starts = @($ir.instructions | Where-Object {
    $_.kindName -eq "async-start" -and $_.targetSymbol -eq $targets[0].symbol
})
$canonicalBindings = 0
foreach ( $start in $starts ) {
    if ( @($start.operands).Count -ne 1 ) { continue }
    $argumentValue = [int]$start.operands[0]
    $definition = $ir.instructions | Where-Object { $_.result -eq $argumentValue } |
        Select-Object -First 1
    if ( $null -eq $definition ) { continue }
    if ( $definition.type -eq $parameterType ) {
        ++$canonicalBindings
        continue
    }
    if ( $definition.kindName -ne "value" -or
         @($definition.operands).Count -ne 1 ) { continue }
    $sourceValue = [int]$definition.operands[0]
    $sourceDefinition = $ir.instructions |
        Where-Object { $_.result -eq $sourceValue } | Select-Object -First 1
    $sourceType = if ( $null -ne $sourceDefinition ) {
        $ir.types | Where-Object { $_.id -eq $sourceDefinition.type } |
            Select-Object -First 1
    } else { $null }
    if ( $null -ne $sourceType -and $null -ne $targetType -and
         $sourceType.cAbi -eq $targetType.cAbi -and
         $sourceType.ownershipAbi -eq $targetType.ownershipAbi -and
         $sourceType.typeValueId -eq $targetType.typeValueId ) {
        ++$canonicalBindings
    }
}
if ( $null -eq $targetType -or $targetType.cAbi -ne 29 -or
     $targetType.ownershipAbi -ne 2 -or $targetType.nullAbi -ne 1 -or
     $targetType.typeValueId -eq "0x0000000000000000" -or
     $starts.Count -ne 2 -or $canonicalBindings -ne $starts.Count ) {
    throw "XLIR async-error catch/signature boundary has no canonical TypeValueId binding"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-error C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedRepeat
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-error repeat C generation failed" }
$left = [System.IO.File]::ReadAllBytes($generatedC)
$right = [System.IO.File]::ReadAllBytes($generatedRepeat)
if ( ! [System.Linq.Enumerable]::SequenceEqual([byte[]]$left, [byte[]]$right) ) {
    throw "XLIR async-error C generation is not byte-for-byte deterministic"
}

$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "#include <__xl_runtime/xl6_future_payload.h>",
    "{XL6_CAPTURE_ERROR, offsetof(xlir_async_frame_1, p0)",
    "(xl6TaskRuntimeV2Get()->Cleanup)(frame);",
    "xerror* xlir_acquired = a0 != NULL ? xrtErrorRef(a0) : NULL;",
    "{ &(frame->p0), &(xlir_committed), sizeof(frame->p0) }",
    "result->Destroy = xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_ERROR);",
    "xerror* const* payload = (xerror* const*)(xrtFutureValue(",
    "*payload != NULL ? xrtErrorRef(*payload) : NULL;",
    "xrtErrorFree(xlir_released);"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR async-error generated C is missing: $required"
    }
}

$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_async_error_abi.gate.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_error_abi.gate.exe && build\tmp\xlir_async_error_abi.gate.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR async-error strict-C/repeated-await runtime gate failed"
}

& $xl ir $raceSource --lowered --format json --emit $raceLoweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-error race lowered IR failed" }
$raceIr = [System.IO.File]::ReadAllText(
    $raceLoweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$races = @($raceIr.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "future-race" -and
    $_.asyncResultAbiName -eq "retained-error"
})
if ( $races.Count -ne 1 -or
     @($races[0].operands).Count -ne 2 -or
     $races[0].payloadType -eq 0 ) {
    throw "XLIR async-error race has no exact retained-error aggregate ABI"
}
$raceAwaits = @($raceIr.instructions | Where-Object {
    $_.kindName -eq "await" -and $_.asyncResultAbiName -eq "retained-error"
})
if ( $raceAwaits.Count -ne 2 ) {
    throw "XLIR async-error race/repeated source awaits do not preserve retained-error ABI"
}

& $xl cgen $raceSource --backend xlir --emit $raceGeneratedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-error race C generation failed" }
$raceC = [System.IO.File]::ReadAllText(
    $raceGeneratedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xl6FutureAggregateRaceV2(",
    "xerror* const* payload = (xerror* const*)(xrtFutureValue(",
    "*payload != NULL ? xrtErrorRef(*payload) : NULL;",
    "result->Destroy = xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_ERROR);"
) ) {
    if ( ! $raceC.Contains($required) ) {
        throw "XLIR async-error race generated C is missing: $required"
    }
}
if ( $raceC.Contains("xlir_future_payload_box_t") ) {
    throw "XLIR task.race retained-error path unexpectedly materialized task.all xvalue boxing"
}

$raceCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_async_error_race_abi.gate.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_async_error_race_abi.gate.exe && build\test\xlir_async_error_race_abi.gate.exe'
& $env:ComSpec /d /v:on /c $raceCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR async-error task.race strict-C/runtime gate failed"
}

& $xl ir $allSource --typed --format json --emit $allTypedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.all retained-error Typed IR failed" }
$allTyped = [System.IO.File]::ReadAllText(
    $allTypedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$allCalls = @($allTyped.intrinsicCalls | Where-Object {
    $_.kindName -eq "task-all" -and $_.admissionName -eq "verified" -and
    $_.payloadAbiName -eq "retained-error" -and $_.payloadType -ne 0
})
if ( $allCalls.Count -ne 1 -or $allCalls[0].operandCount -ne 3 ) {
    throw "Typed XLIR task.all has no unique verified retained-error contract"
}
$allCall = $allCalls[0]
$allPayload = $allTyped.types | Where-Object { $_.id -eq $allCall.payloadType } |
    Select-Object -First 1
$allResultFuture = $allTyped.types | Where-Object { $_.id -eq $allCall.resultType } |
    Select-Object -First 1
$allResultArray = if ( $null -ne $allResultFuture -and
                       @($allResultFuture.params).Count -eq 1 ) {
    $allTyped.types | Where-Object { $_.id -eq $allResultFuture.params[0] } |
        Select-Object -First 1
} else { $null }
$allOperands = @($allTyped.intrinsicOperands | Select-Object `
    -Skip $allCall.firstOperand -First $allCall.operandCount)
$allOperandTypes = @($allOperands | ForEach-Object {
    $operand = $_
    $allTyped.types | Where-Object { $_.id -eq $operand.type } |
        Select-Object -First 1
})
if ( $null -eq $allPayload -or $allPayload.name -ne "error" -or
     $allPayload.cAbi -ne 29 -or $allPayload.ownershipAbi -ne 2 -or
     $allPayload.nullAbi -ne 1 -or
     $allPayload.typeValueId -eq "0x0000000000000000" -or
     $null -eq $allResultFuture -or $allResultFuture.cAbi -ne 30 -or
     $null -eq $allResultArray -or
     $allResultArray.collectionAbiName -ne "array" -or
     @($allResultArray.params).Count -ne 1 -or
     $allResultArray.params[0] -ne $allCall.payloadType -or
     $allOperands.Count -ne 3 -or
     @($allOperands | Where-Object { $_.abiName -ne "borrow" }).Count -ne 0 -or
     $allOperandTypes.Count -ne 3 -or
     @($allOperandTypes | Where-Object {
         $null -eq $_ -or $_.cAbi -ne 30 -or @($_.params).Count -ne 1 -or
         $_.params[0] -ne $allCall.payloadType
     }).Count -ne 0 ) {
    throw "Typed XLIR task.all retained-error payload/result/operand ABI is incomplete"
}

& $xl ir $allSource --lowered --format json --emit $allLoweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.all retained-error lowering failed" }
$allIr = [System.IO.File]::ReadAllText(
    $allLoweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$allAggregates = @($allIr.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "future-all-xvalues" -and
    $_.asyncResultAbiName -eq "retained-error" -and
    $_.payloadType -ne 0 -and @($_.operands).Count -eq 3
})
$allErrorProjects = @($allIr.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-project-error-ref" -and
    $_.type -eq $allCall.payloadType
})
$allErrorAwaits = @($allIr.instructions | Where-Object {
    $_.kindName -eq "await" -and $_.asyncResultAbiName -eq "retained-error" -and
    $_.type -eq $allCall.payloadType
})
if ( $allAggregates.Count -ne 1 -or $allErrorProjects.Count -ne 3 -or
     $allErrorAwaits.Count -ne 2 ) {
    throw "Lowered XLIR task.all did not preserve aggregate boxing, owned error projection and source re-await ABI"
}

& $xl cgen $allSource --backend xlir --emit $allGeneratedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.all retained-error C generation failed" }
& $xl cgen $allSource --backend xlir --emit $allGeneratedRepeat
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.all retained-error repeat C generation failed" }
$allLeft = [System.IO.File]::ReadAllBytes($allGeneratedC)
$allRight = [System.IO.File]::ReadAllBytes($allGeneratedRepeat)
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]]$allLeft, [byte[]]$allRight) ) {
    throw "XLIR task.all retained-error C generation is not byte-for-byte deterministic"
}
$allC = [System.IO.File]::ReadAllText(
    $allGeneratedC, [System.Text.Encoding]::UTF8)
$allBox = "return xl6ErrorValueRefV1(*(xerror* const*)payload, $($allPayload.typeValueId)ULL);"
foreach ( $required in @(
    "static xvalue* xlir_future_payload_box_t",
    $allBox,
    "extern xvalue* xl6ErrorValueRefV1(const xerror*, uint64_t)",
    "static xerror* xlir_xvalue_project_error_ref(const xvalue* value, uint64_t typeId)",
    "if (!xl6ErrorValueReadRefV1(value, typeId, &result)) abort();",
    "xl6FutureAggregateAllV2(",
    "xerror* const* payload = (xerror* const*)(xrtFutureValue("
) ) {
    if ( ! $allC.Contains($required) ) {
        throw "XLIR task.all retained-error generated C is missing: $required"
    }
}
foreach ( $forbidden in @("XL6_Function", "xl6Cgen", "xl6Lower") ) {
    if ( $allC.Contains($forbidden) ) {
        throw "XLIR task.all retained-error Cgen leaked legacy/compiler machinery: $forbidden"
    }
}

$allCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_task_all_error_abi.gate.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_task_all_error_abi.gate.exe && build\test\xlir_task_all_error_abi.gate.exe'
& $env:ComSpec /d /v:on /c $allCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR task.all retained-error strict-C/runtime ownership gate failed"
}

Write-Output "XLIR retained-error result/frame/repeated-await ABI, direct task.race propagation, and task.all exact-TypeId xvalue boxing/projection ownership gate passed."
