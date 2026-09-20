param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\stdlib_future_continuation.xl"
$hostSource = Join-Path $Root "test\cases\xlir_future_continuation_abi_host.c"
$outputDir = Join-Path $Root "build\tmp"
$testDir = Join-Path $Root "build\test"
$typedJson = Join-Path $outputDir "xlir_future_continuation_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_future_continuation_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_future_continuation_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_future_continuation_abi.second.c"
$legacyC = Join-Path $outputDir "xlir_future_continuation_abi.legacy.c"
$xlirExe = Join-Path $testDir "xlir_future_continuation_abi.exe"
$legacyExe = Join-Path $testDir "xlir_future_continuation_abi.legacy.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR Future continuation ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir, $testDir | Out-Null

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Future continuation Typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$continuations = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -in @("future-then", "future-catch", "future-finally")
})
if ( $continuations.Count -ne 10 -or
     @($continuations | Where-Object {
         $_.admissionName -ne "verified" -or
         $_.resultAbiName -ne "owned" -or
         $_.errorAbiName -ne "may-raise" -or
         $_.sourcePayloadType -eq 0 -or $_.payloadType -eq 0 -or
         $_.sourcePayloadAbiName -eq "invalid" -or
         $_.payloadAbiName -eq "invalid"
     }).Count -ne 0 ) {
    throw "XLIR Future continuation Typed source/output payload ABI is incomplete"
}
$kindPartition = @($continuations | Group-Object kindName |
    Sort-Object Name | ForEach-Object { "$($_.Name):$($_.Count)" }) -join ','
if ( $kindPartition -ne 'future-catch:1,future-finally:5,future-then:4' ) {
    throw "XLIR Future continuation kind partition changed: $kindPartition"
}
$typedChain = @($continuations | Where-Object {
    $_.kindName -eq "future-then" -and
    $_.sourcePayloadAbiName -eq "copy-i64" -and
    $_.payloadAbiName -eq "owned-string" -and
    $_.sourcePayloadType -ne $_.payloadType
})
if ( $typedChain.Count -ne 1 ) {
    throw "XLIR Future<T> to Future<U> continuation lost its distinct payload ABI"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Future continuation lowering failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "future-continue-closure"
})
if ( $operations.Count -ne 10 -or
     @($operations | Where-Object {
         $_.callbackBoundary -eq 0 -or
         $_.kindName -ne "runtime-call" -or $_.operandCount -ne 2 -or
         $_.calleeType -eq 0 -or $_.sourcePayloadType -eq 0 -or
         $_.payloadType -eq 0 -or $_.sourceAsyncResultAbiName -eq "invalid" -or
         $_.asyncResultAbiName -eq "invalid" -or
         $_.futureContinuationKindName -notin @("then", "catch", "finally") -or
         $_.targetSymbol -ne 0 -or $_.hasImmediate -ne 0
     }).Count -ne 0 ) {
    throw "XLIR lowered Future continuation RuntimeOp/frame ABI is incomplete"
}
if (@($lowered.callbackBoundaries | Where-Object { $_.transportAbi -eq 2 -and $_.cleanupAbi -eq 1 -and $_.errorAbi -eq 1 -and $_.forwardAbi -eq 2 }).Count -ne 10 -or
    @($lowered.callbackBoundaries | Where-Object { $_.transportAbi -eq 1 -and $_.cleanupAbi -eq 1 -and $_.errorAbi -eq 2 -and $_.forwardAbi -eq 1 }).Count -ne 9) {
    throw 'Future continuations and tasks need distinct verified transports and shared cleanup transactions'
}
$loweredKinds = @($operations | Group-Object futureContinuationKindName |
    Sort-Object Name | ForEach-Object { "$($_.Name):$($_.Count)" }) -join ','
if ( $loweredKinds -ne 'catch:1,finally:5,then:4' ) {
    throw "XLIR lowered continuation kind partition changed: $loweredKinds"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Future continuation C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR repeat Future continuation C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
$second = [System.IO.File]::ReadAllText(
    $generatedCSecond, [System.Text.Encoding]::UTF8)
if ( $c -cne $second ) {
    throw "XLIR Future continuation generated C is not byte-for-byte deterministic"
}
foreach ( $required in @(
    "typedef struct xlir_future_continue_frame_",
    "static void xlir_future_continue_proc_",
    "static xfuture* xlir_future_continue_start_",
    "xrtFutureWatchAdd(source, &frame->watch)",
    "xlir_future_continue_frame_cleanup_",
    "xrtPromiseForward(output, source)",
    "frame->callable = callable != NULL ? xlir_closure_retain(callable) : NULL;",
    "xlir_task_spawn_wrap_current_error",
    'desc.Data = "AsyncError"; desc.Cause = cause;',
    "extern bool xrtCancelRequested(const xcancel*);",
    "cancelled = ok && !xrtGetError() && xrtCancelRequested(cancel);",
    "xlir_async_machine_"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR Future continuation mechanical C ABI is missing: $required"
    }
}
if ( ([regex]::Matches($c,
        'static xfuture\* xlir_future_continue_start_[0-9]+\(')).Count -ne 10 -or
     ([regex]::Matches($c,
        'static void xlir_future_continue_proc_[0-9]+\(')).Count -ne 10 -or
     ([regex]::Matches($c,
        'static xtaskoutcome xlir_task_spawn_task_[0-9]+\(')).Count -ne 9 -or
     ([regex]::Matches($c,
        'cancelled = ok && !xrtGetError\(\) && xrtCancelRequested\(cancel\);')).Count -ne 9 ) {
    throw "XLIR Future continuation did not emit one helper pair per RuntimeOp"
}
foreach ( $forbidden in @(
    "XL6_AST_",
    "xl6IrNodeGet",
    "xl6IrFutureMemberCalleeKind",
    "__xl6_future_continue",
    "future = xrtFutureContinueOwned("
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR Future continuation Cgen leaked source/legacy dispatch: $forbidden"
    }
}

$xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_future_continuation_abi.c test\cases\xlir_future_continuation_abi_host.c !XL6_CGEN_LINKS! -o build\test\xlir_future_continuation_abi.exe && build\test\xlir_future_continuation_abi.exe'
$xlirOutput = @(& $env:ComSpec /d /v:on /c $xlirCommand 2>&1)
if ( $LASTEXITCODE -ne 0 ) {
    $xlirOutput | ForEach-Object { Write-Output $_ }
    throw "XLIR Future continuation strict-C/runtime gate failed"
}
if ( (@($xlirOutput | Where-Object { $_ -eq 'future continuation ok' })).Count -ne 1 ) {
    throw "XLIR Future continuation runtime output changed"
}

& $xl cgen $source --backend legacy --emit $legacyC
if ( $LASTEXITCODE -ne 0 ) { throw "Legacy Future continuation C generation failed" }
$legacyCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror build\tmp\xlir_future_continuation_abi.legacy.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_future_continuation_abi.legacy.exe && build\test\xlir_future_continuation_abi.legacy.exe'
$legacyOutput = @(& $env:ComSpec /d /v:on /c $legacyCommand 2>&1)
if ( $LASTEXITCODE -ne 0 ) {
    $legacyOutput | ForEach-Object { Write-Output $_ }
    throw "Legacy Future continuation differential runtime gate failed"
}
if ( (@($legacyOutput | Where-Object { $_ -eq 'future continuation ok' })).Count -ne 1 ) {
    throw "Legacy Future continuation runtime output changed"
}

Write-Output "XLIR Future then/catch/finally ABI gate passed: explicit source/output payloads, typed continuation kind, deterministic mechanical C, cooperative task cancellation before result publication, task-boundary AsyncError cause, async captured closure, strict runtime, and legacy differential semantics."
exit 0
