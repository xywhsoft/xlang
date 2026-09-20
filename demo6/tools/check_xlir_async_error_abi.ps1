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
$allRejectedSource = Join-Path $Root "test\cases\sem_task_all_error_payload_xlir_rejected.xl"
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
$allRejectedJson = Join-Path $outputDir "xlir_async_error_all_rejected.gate.lowered.json"
$allRejectedLog = Join-Path $outputDir "xlir_async_error_all_rejected.gate.log"

foreach ( $path in @(
    $xl, $source, $raceSource, $allRejectedSource,
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

# Catch owns the synthetic canonical error TypeId while the callable signature
# may use the frontend registry peer.  Equal TypeValueId plus representation is
# made explicit by VALUE; ASYNC_START must never rely on a C pointer coincidence.
$parameterType = [int]$targets[0].parameterTypes[0]
$rebinds = @($ir.instructions | Where-Object {
    $_.kindName -eq "value" -and $_.type -eq $parameterType
})
$explicitRebinds = 0
foreach ( $rebind in $rebinds ) {
    if ( @($rebind.operands).Count -ne 1 ) { continue }
    $sourceValue = [int]$rebind.operands[0]
    $definition = $ir.instructions | Where-Object { $_.result -eq $sourceValue } |
        Select-Object -First 1
    if ( $null -eq $definition -or $definition.type -eq $parameterType ) { continue }
    $sourceType = $ir.types | Where-Object { $_.id -eq $definition.type } |
        Select-Object -First 1
    $targetType = $ir.types | Where-Object { $_.id -eq $parameterType } |
        Select-Object -First 1
    if ( $null -ne $sourceType -and $null -ne $targetType -and
         $sourceType.cAbi -eq $targetType.cAbi -and
         $sourceType.ownershipAbi -eq $targetType.ownershipAbi -and
         $sourceType.typeValueId -eq $targetType.typeValueId ) {
        ++$explicitRebinds
    }
}
if ( $explicitRebinds -lt 2 ) {
    throw "XLIR async-error catch/signature boundary has no explicit TypeValueId rebind"
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
    "static void xlir_async_error_free(void* value, void* data)",
    "xrtErrorFree(*(xerror**)value);",
    "static xerror* xlir_future_await_error(xfuture* future)",
    "return value != NULL && *value != NULL ? xrtErrorRef(*value) : NULL;",
    "frame->p0 = a0 != NULL ? xrtErrorRef(a0) : NULL;",
    "xrtErrorFree(frame->p0);",
    "result->Destroy = xlir_async_error_free;",
    "xlir_future_await_error("
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
    "(xl6FutureAggregateRuntimeV1Get()->Race)(",
    "static xerror* xlir_future_await_error(xfuture* future)",
    "result->Destroy = xlir_async_error_free;"
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

Remove-Item -LiteralPath $allRejectedJson,$allRejectedLog `
    -Force -ErrorAction SilentlyContinue
# Windows PowerShell promotes redirected native stderr to ErrorRecord under
# Stop.  Execute the expected-negative probe through cmd so its diagnostic is
# ordinary captured data and the gate can assert the compiler exit contract.
$allRejectedCommand = ('"{0}" ir "{1}" --lowered --format json --emit "{2}" > "{3}" 2>&1' -f
    $xl, $allRejectedSource, $allRejectedJson, $allRejectedLog)
& $env:ComSpec /d /c $allRejectedCommand
$allRejectedExit = $LASTEXITCODE
$allRejectedOutput = if ( Test-Path -LiteralPath $allRejectedLog -PathType Leaf ) {
    [System.IO.File]::ReadAllText($allRejectedLog, [System.Text.Encoding]::UTF8)
} else {
    ""
}
if ( $allRejectedExit -eq 0 -or
     (Test-Path -LiteralPath $allRejectedJson -PathType Leaf) -or
     !($allRejectedOutput.Contains(
          "XLIR task.all payload has no canonical xvalue boxing ABI")) ) {
    throw "XLIR task.all unexpectedly admitted retained-error without a canonical xvalue boxing ABI"
}
$global:LASTEXITCODE = 0

Write-Output "XLIR retained-error result/frame/repeated-await ABI, direct task.race winner propagation and loser cancellation, and task.all xvalue-box admission boundary gate passed."
