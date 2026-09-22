param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_future_member_status_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_future_member_status_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_future_member_status_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_future_member_status_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_future_member_status_abi.second.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR Future member ABI gate input is missing: $path"
    }
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$expected = [ordered]@{
    "future-wait" = 3
    "future-wait-timeout" = 1
    "future-wait-until" = 1
    "future-is-done" = 4
    "future-is-pending" = 2
    "future-is-cancelled" = 2
    "future-is-failed" = 3
    "future-state" = 4
    "future-status" = 4
    "future-error" = 1
    "future-set-debug-name" = 1
    "future-debug-name" = 1
    "future-create-time-ms" = 2
    "future-complete-time-ms" = 2
    "future-pending-continuations" = 1
}

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Future member typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$futureIntrinsics = @($typed.intrinsicCalls | Where-Object {
    $expected.Contains($_.kindName)
})
if ( $futureIntrinsics.Count -ne 32 ) {
    throw "XLIR Future member IntrinsicCall count is incomplete: $($futureIntrinsics.Count)"
}
foreach ( $entry in $expected.GetEnumerator() ) {
    $matches = @($futureIntrinsics | Where-Object {
        $_.kindName -eq $entry.Key
    })
    if ( $matches.Count -ne $entry.Value ) {
        throw "XLIR Future member intrinsic count mismatch for $($entry.Key): $($matches.Count)"
    }
    foreach ( $intrinsic in $matches ) {
        $takesTime = $intrinsic.kindName -in @(
            "future-wait-timeout", "future-wait-until")
        $takesName = $intrinsic.kindName -eq "future-set-debug-name"
        $operands = @($typed.intrinsicOperands |
            Select-Object -Skip $intrinsic.firstOperand -First $intrinsic.operandCount)
        $receiverType = if ( $operands.Count -gt 0 ) {
            @($typed.types | Where-Object { $_.id -eq $operands[0].type })[0]
        } else { $null }
        $resultType = @($typed.types | Where-Object {
            $_.id -eq $intrinsic.resultType
        })[0]
        $returnsError = $intrinsic.kindName -eq "future-error"
        $returnsName = $intrinsic.kindName -eq "future-debug-name"
        $returnsI64 = $intrinsic.kindName -in @(
            "future-state", "future-status", "future-create-time-ms",
            "future-complete-time-ms", "future-pending-continuations")
        if ( $intrinsic.admissionName -ne "verified" -or
             $intrinsic.resultAbiName -ne $(if ( $returnsError -or $returnsName ) { "owned" } else { "value" }) -or
             $intrinsic.errorAbiName -ne "no-raise" -or
             $intrinsic.operandCount -ne $(if ( $takesTime -or $takesName ) { 2 } else { 1 }) -or
             $operands.Count -ne $intrinsic.operandCount -or
             $operands[0].abiName -ne "borrow" -or
             $null -eq $receiverType -or $receiverType.name -ne "future" -or
             @($receiverType.params).Count -ne 1 -or
             $null -eq $resultType -or
             ($returnsError -and $resultType.name -ne "error") -or
             ($returnsName -and $resultType.name -ne "string") -or
             ($returnsI64 -and ($resultType.name -ne "int" -or
                                $resultType.integerTargetName -ne "i64")) -or
             (!$returnsError -and !$returnsName -and !$returnsI64 -and
              $resultType.name -ne "bool") ) {
            throw "XLIR Future member intrinsic identity/receiver/result ABI is incomplete for $($entry.Key)"
        }
        if ( $takesTime ) {
            $timeType = @($typed.types | Where-Object {
                $_.id -eq $operands[1].type
            })[0]
            if ( $operands[1].abiName -ne "value" -or
                 $null -eq $timeType -or $timeType.name -ne "int" -or
                 $timeType.integerTargetName -ne "i64" ) {
                throw "XLIR Future member millisecond operand ABI is incomplete for $($entry.Key)"
            }
        }
        if ( $takesName ) {
            $nameType = @($typed.types | Where-Object {
                $_.id -eq $operands[1].type
            })[0]
            if ( $operands[1].abiName -ne "borrow" -or
                 $null -eq $nameType -or $nameType.name -ne "string" ) {
                throw "XLIR Future metadata name operand ABI is incomplete"
            }
        }
    }
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Future member lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$runtimeExpected = [ordered]@{}
foreach ( $entry in $expected.GetEnumerator() ) {
    $runtimeName = if ( $entry.Key -eq "future-error" ) {
        "future-error-ref"
    } elseif ( $entry.Key -eq "future-debug-name" ) {
        "future-debug-name-copy"
    } else { $entry.Key }
    $runtimeExpected[$runtimeName] = $entry.Value
}
$futureOperations = @($lowered.instructions | Where-Object {
    $runtimeExpected.Contains($_.runtimeOpName)
})
if ( $futureOperations.Count -ne 32 ) {
    throw "XLIR Future member RuntimeOp count is incomplete: $($futureOperations.Count)"
}
foreach ( $entry in $runtimeExpected.GetEnumerator() ) {
    $matches = @($futureOperations | Where-Object {
        $_.runtimeOpName -eq $entry.Key
    })
    if ( $matches.Count -ne $entry.Value ) {
        throw "XLIR Future member RuntimeOp count mismatch for $($entry.Key): $($matches.Count)"
    }
    foreach ( $operation in $matches ) {
        $takesTime = $operation.runtimeOpName -in @(
            "future-wait-timeout", "future-wait-until")
        $takesName = $operation.runtimeOpName -eq "future-set-debug-name"
        $returnsError = $operation.runtimeOpName -eq "future-error-ref"
        $returnsName = $operation.runtimeOpName -eq "future-debug-name-copy"
        $returnsI64 = $operation.runtimeOpName -in @(
            "future-state", "future-status", "future-create-time-ms",
            "future-complete-time-ms", "future-pending-continuations")
        $resultType = @($lowered.types | Where-Object {
            $_.id -eq $operation.type
        })[0]
        $receiverDefinition = @($lowered.instructions | Where-Object {
            $_.result -eq $operation.operands[0]
        })[-1]
        $receiverType = if ( $null -ne $receiverDefinition ) {
            @($lowered.types | Where-Object {
                $_.id -eq $receiverDefinition.type
            })[0]
        } else { $null }
        if ( $operation.kindName -ne "runtime-call" -or
             $operation.operandCount -ne $(if ( $takesTime -or $takesName ) { 2 } else { 1 }) -or
             $null -eq $resultType -or
             ($returnsError -and $resultType.name -ne "error") -or
             ($returnsName -and $resultType.name -ne "string") -or
             ($returnsI64 -and ($resultType.name -ne "int" -or
                                $resultType.integerTargetName -ne "i64")) -or
             (!$returnsError -and !$returnsName -and !$returnsI64 -and
              $resultType.name -ne "bool") -or
             $null -eq $receiverType -or $receiverType.name -ne "future" -or
             @($receiverType.params).Count -ne 1 ) {
            throw "XLIR Future member Lowered RuntimeOp ABI is incomplete for $($entry.Key)"
        }
        if ( $takesTime ) {
            $timeDefinition = @($lowered.instructions | Where-Object {
                $_.result -eq $operation.operands[1]
            })[-1]
            $timeType = if ( $null -ne $timeDefinition ) {
                @($lowered.types | Where-Object {
                    $_.id -eq $timeDefinition.type
                })[0]
            } else { $null }
            if ( $null -eq $timeType -or $timeType.name -ne "int" -or
                 $timeType.integerTargetName -ne "i64" ) {
                throw "XLIR Future member Lowered I64 time ABI is incomplete for $($entry.Key)"
            }
        }
        if ( $takesName ) {
            $nameDefinition = @($lowered.instructions | Where-Object {
                $_.result -eq $operation.operands[1]
            })[-1]
            $nameType = if ( $null -ne $nameDefinition ) {
                @($lowered.types | Where-Object {
                    $_.id -eq $nameDefinition.type
                })[0]
            } else { $null }
            if ( $null -eq $nameType -or $nameType.name -ne "string" ) {
                throw "XLIR Future metadata lowered name ABI is incomplete"
            }
        }
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Future member C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Future member repeat C generation failed" }
$firstBytes = [System.IO.File]::ReadAllBytes($generatedC)
$secondBytes = [System.IO.File]::ReadAllBytes($generatedCSecond)
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]]$firstBytes, [byte[]]$secondBytes) ) {
    throw "XLIR Future member generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static bool xlir_future_wait(xfuture* future)",
    "static bool xlir_future_wait_timeout(xfuture* future, int64_t milliseconds)",
    "static bool xlir_future_wait_until(xfuture* future, int64_t milliseconds)",
    "static bool xlir_future_is_done(xfuture* future)",
    "static bool xlir_future_is_pending(xfuture* future)",
    "static bool xlir_future_is_cancelled(xfuture* future)",
    "static bool xlir_future_is_failed(xfuture* future)",
    "static int64_t xlir_future_state(xfuture* future)",
    "static int64_t xlir_future_status(xfuture* future)",
    "static xerror* xlir_future_error_ref(xfuture* future)",
    "static bool xlir_future_set_debug_name(xfuture* future, const char* name)",
    "static char* xlir_future_debug_name_copy(xfuture* future)",
    "static int64_t xlir_future_create_time_ms(xfuture* future)",
    "static int64_t xlir_future_complete_time_ms(xfuture* future)",
    "static int64_t xlir_future_pending_continuations(xfuture* future)",
    "xrtFutureWaitFor(future, xlir_future_milliseconds(milliseconds))",
    "xrtFutureWaitUntil(future, xlir_future_milliseconds(milliseconds))",
    "xrtFutureState(future) == XFUTURE_RESOLVED",
    " = xlir_future_wait(", " = xlir_future_wait_timeout(",
    " = xlir_future_wait_until(", " = xlir_future_is_done(",
    " = xlir_future_is_pending(", " = xlir_future_is_cancelled(",
    " = xlir_future_is_failed(", " = xlir_future_state(",
    " = xlir_future_status(", " = xlir_future_error_ref(",
    " = xlir_future_set_debug_name(", " = xlir_future_debug_name_copy(",
    " = xlir_future_create_time_ms(", " = xlir_future_complete_time_ms(",
    " = xlir_future_pending_continuations("
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR Future member generated C is missing mechanical RuntimeOp emission: $required"
    }
}
foreach ( $forbidden in @(
    "xl6FutureWait(", "xl6FutureWaitFor(", "xl6FutureWaitUntil(",
    "xl6FutureState(", "waitTimeout", "isCancelled"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR Future member Cgen guessed legacy/source semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_future_member_status_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_future_member_status_abi.exe && build\test\xlir_future_member_status_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR Future member strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR Future member ABI gate passed: fifteen distinct verified IntrinsicCalls and RuntimeOps, borrowed Future, I64 millisecond operands, borrowed managed-string name operand, owned string/error projection, deterministic mechanical C, and strict runtime."
