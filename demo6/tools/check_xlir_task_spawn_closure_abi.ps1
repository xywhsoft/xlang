param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_task_spawn_closure_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$testDir = Join-Path $Root "build\test"
$typedJson = Join-Path $outputDir "xlir_task_spawn_closure_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_task_spawn_closure_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_task_spawn_closure_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_task_spawn_closure_abi.second.c"
$xlirExe = Join-Path $testDir "xlir_task_spawn_closure_abi.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR task.spawn closure ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir, $testDir | Out-Null

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.spawn typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$spawns = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "task-spawn"
})
if ( $spawns.Count -ne 4 -or
     @($spawns | Where-Object {
         $_.admissionName -ne "verified" -or
         $_.resultAbiName -ne "owned" -or
         $_.errorAbiName -ne "may-raise" -or
         $_.payloadType -eq 0
     }).Count -ne 0 ) {
    throw "XLIR task.spawn Typed intrinsic identity/admission ABI is incomplete"
}
$payloadAbis = @($spawns | ForEach-Object { $_.payloadAbiName })
if ( ($payloadAbis -join ',') -ne
     'owned-string,copy-bool,copy-f64,copy-i64' ) {
    throw "XLIR task.spawn Typed payload ABI partition changed: $($payloadAbis -join ',')"
}
$expectedOperandAbis = @(
    'borrow,borrow,borrow',
    'borrow,value',
    'borrow,value',
    'borrow,borrow'
)
for ( $i = 0; $i -lt $spawns.Count; ++$i ) {
    $spawn = $spawns[$i]
    $slice = @($typed.intrinsicOperands |
        Select-Object -Skip $spawn.firstOperand -First $spawn.operandCount)
    $actual = ($slice | ForEach-Object { $_.abiName }) -join ','
    if ( $actual -ne $expectedOperandAbis[$i] ) {
        throw "XLIR task.spawn canonical operand ABI mismatch at call $i`: $actual"
    }
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.spawn lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "task-spawn-closure"
})
if ( $operations.Count -ne 4 -or
     @($operations | Where-Object {
         $_.kindName -ne "runtime-call" -or
         $_.calleeType -eq 0 -or $_.payloadType -eq 0 -or
         $_.targetSymbol -ne 0 -or $_.hasImmediate -ne 0 -or
         $_.operandCount -lt 2
     }).Count -ne 0 ) {
    throw "XLIR task.spawn Lowered RuntimeOp has no exact closure/frame ABI"
}
$captureAbis = @('6/5,3/2,3/2', '6/5,2/1', '6/5,2/1', '6/5,4/3')
if (@($lowered.callbackBoundaries).Count -ne 4 -or @($lowered.callbackCaptures).Count -ne 9) {
    throw 'Task RuntimeOps lost explicit callback activation/capture records'
}
for ($i = 0; $i -lt $operations.Count; ++$i) {
    $op = $operations[$i]
    if ($op.callbackBoundary -ne $i + 1) { throw 'Task callback identity is not canonical' }
    $boundary = $lowered.callbackBoundaries[$op.callbackBoundary - 1]
    if ($boundary.instruction -ne $op.index -or $boundary.captureCount -ne $op.operandCount -or
        $boundary.transportAbi -ne 1 -or $boundary.cleanupAbi -ne 1 -or
        $boundary.errorAbi -ne 2 -or $boundary.forwardAbi -ne 1) {
        throw 'Task capture boundary lost pool/transaction/AsyncError/no-forward contracts'
    }
    $captures = @($lowered.callbackCaptures | Select-Object -Skip $boundary.firstCapture -First $boundary.captureCount)
    for ($j = 0; $j -lt $captures.Count; ++$j) {
        $expectedRole = if ($j -eq 0) { 2 } else { 3 }
        if ($captures[$j].operand -ne $j -or $captures[$j].value -ne $op.operands[$j] -or
            $captures[$j].type -eq 0 -or $captures[$j].role -ne $expectedRole) {
            throw 'Task capture does not refer to the canonical callable/argument operand'
        }
    }
    if (($captures | ForEach-Object { "$($_.transferAbi)/$($_.destroyAbi)" }) -join ',' -cne $captureAbis[$i]) {
        throw 'Task capture acquisition and reverse destruction ABIs changed'
    }
}

$loweredAbis = @($operations | ForEach-Object { $_.asyncResultAbiName })
if ( ($loweredAbis -join ',') -ne
     'owned-string,copy-bool,copy-f64,copy-i64' ) {
    throw "XLIR task.spawn Lowered async result ABI partition changed"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.spawn C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR task.spawn repeat C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
$second = [System.IO.File]::ReadAllText(
    $generatedCSecond, [System.Text.Encoding]::UTF8)
if ( $c -cne $second ) {
    throw "XLIR task.spawn generated C is not byte-for-byte deterministic"
}
foreach ( $required in @(
    "typedef struct xlir_task_spawn_frame_",
    "static xtaskoutcome xlir_task_spawn_task_",
    "static xfuture* xlir_task_spawn_start_",
    "frame->callable = callable != NULL ? xlir_closure_retain(callable) : NULL;",
    "args.Destroy = xlir_task_spawn_frame_free_",
    "xlir_async_pool_submit(xlir_task_spawn_task_",
    "frame->p0 = a0 != NULL ? xrtStrDup(a0) : NULL;",
    "frame->p0 = a0 != NULL ? xrtValueRetain(a0) : NULL;",
    "{ xvalue* value = frame->p0; frame->p0 = NULL; xrtValueRelease(value); }",
    "result->Destroy = xlir_async_string_free;",
    "extern bool xrtCancelRequested(const xcancel*);",
    "cancelled = ok && !xrtGetError() && xrtCancelRequested(cancel);",
    "return XTASK_CANCELLED;"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR task.spawn mechanical C ABI emission is missing: $required"
    }
}
if ( ([regex]::Matches($c,
        'static xfuture\* xlir_task_spawn_start_[0-9]+\(')).Count -ne 4 -or
     ([regex]::Matches($c,
        'static xtaskoutcome xlir_task_spawn_task_[0-9]+\(')).Count -ne 4 -or
     ([regex]::Matches($c,
        'if \(cancelled\) \{ xrtClearError\(\); return XTASK_CANCELLED; \}')).Count -ne 4 ) {
    throw "XLIR task.spawn did not emit exactly one typed helper pair per RuntimeOp"
}
$workers = [regex]::Matches($c, '(?ms)^static xtaskoutcome xlir_task_spawn_task_[0-9]+\([^;]*?^\}')
foreach ($worker in $workers) {
    $cleanup = $worker.Value.IndexOf('if (frame) xlir_task_spawn_frame_cleanup_')
    $publish = $worker.Value.IndexOf('result->Value = value;')
    if ($cleanup -lt 0 -or $publish -le $cleanup -or
        !$worker.Value.Contains('xrtClearError(); xrtSetErrorTake(primary);')) {
        throw 'Task result publication must follow capture cleanup and preserve rollback errors'
    }
}

foreach ( $forbidden in @(
    "XL6_AST_",
    "xl6IrNodeGet",
    "xl6IrIsTaskSpawnCallee",
    "__xl6_spawn_dispatch"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR task.spawn Cgen leaked source/legacy dispatch machinery: $forbidden"
    }
}

$xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_task_spawn_closure_abi.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\xlir_task_spawn_closure_abi.exe && build\test\xlir_task_spawn_closure_abi.exe'
& $env:ComSpec /d /v:on /c $xlirCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR task.spawn strict-C/runtime gate failed"
}

Write-Output "XLIR task.spawn closure/frame ABI gate passed: verified canonical arguments, typed RuntimeOp metadata, managed captures/results, deterministic mechanical C, and strict runtime."
exit 0
