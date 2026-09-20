param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_task_nested_future_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_task_nested_future_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_task_nested_future_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_task_nested_future_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_task_nested_future_abi.second.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR task nested Future ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task nested Future typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$retainedSpawns = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "task-spawn" -and $_.payloadAbiName -eq "retained-future"
})
$retainedAggregates = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -in @("task-all", "task-all-array", "task-race", "task-race-array") -and
    $_.payloadAbiName -eq "retained-future"
})
if ( $retainedSpawns.Count -ne 6 -or $retainedAggregates.Count -ne 4 ) {
    throw "XLIR task nested Future typed IntrinsicCalls have no exact retained-future payload ABI: spawn=$($retainedSpawns.Count), aggregate=$($retainedAggregates.Count)"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task nested Future lowering failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$spawnOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "task-spawn-closure" -and
    $_.asyncResultAbiName -eq "retained-future"
})
$aggregateOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -in @(
        "future-all-xvalues", "future-all-xvalues-array",
        "future-race", "future-race-array") -and
    $_.asyncResultAbiName -eq "retained-future"
})
$nestedAwaits = @($lowered.instructions | Where-Object {
    $_.kindName -eq "await" -and
    $_.asyncResultAbiName -eq "retained-future"
})
if ( $spawnOperations.Count -ne 6 -or
     $aggregateOperations.Count -ne 4 -or
     $nestedAwaits.Count -ne 2 ) {
    throw "XLIR task nested Future lowering lost task.spawn/all/race/await ownership facts: spawn=$($spawnOperations.Count), aggregate=$($aggregateOperations.Count), await=$($nestedAwaits.Count)"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task nested Future C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task nested Future repeat C generation failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR task nested Future generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static void xlir_async_future_free(void* value, void* data)",
    "xrtFutureDestroy(*(xfuture**)value)",
    "static xfuture* xlir_future_await_future(xfuture* future)",
    "xrtFutureRef(*value)",
    "xrtValueFuture(*(xfuture* const*)payload)",
    "frame->p0 = a0 != NULL ? xrtFutureRef(a0) : NULL;",
    "xrtFutureDestroy(frame->p0);",
    "xlir_future_all_xvalues_array(",
    "xlir_future_race_array("
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR task nested Future generated C is missing mechanical ABI emission: $required"
    }
}
foreach ( $forbidden in @(
    "xl6_task_spawn", "xl6_task_all", "xl6_task_race",
    "xl6_future_to_xvalue"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR task nested Future Cgen reused legacy/source-shape generation: $forbidden"
    }
}

Push-Location $Root
try {
    $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_task_nested_future_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_task_nested_future_abi.exe && build\test\xlir_task_nested_future_abi.exe'
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR task nested Future strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR task nested Future ABI gate passed: task.spawn Future argument frame retain, retained-future task.spawn/all/race payloads, independent await reference, JSON IR, deterministic mechanical C, strict XLIR-only runtime contract, and no legacy source-shape fallback."
exit 0
