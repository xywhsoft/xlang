param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_thread_resource_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$testDir = Join-Path $Root "build\test"
$typedJson = Join-Path $outputDir "xlir_thread_resource_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_thread_resource_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_thread_resource_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_thread_resource_abi.second.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR thread resource ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir, $testDir | Out-Null

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR thread resource Typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$threadTypes = @($typed.types | Where-Object {
    $_.runtimeResourceAbiName -eq "thread"
})
if ( $threadTypes.Count -ne 1 ) {
    throw "XLIR must intern exactly one canonical thread runtime resource type"
}
$threadType = $threadTypes[0]
if ( $threadType.cAbi -ne 26 -or $threadType.ownershipAbi -ne 2 -or
     $threadType.nullAbi -ne 2 -or
     $threadType.typeValueId -eq "0x0000000000000000" ) {
    throw "XLIR thread type has no exact managed xvalue/null/TypeValueId ABI"
}

$expectedCounts = [ordered]@{
    "thread-create" = 4
    "thread-wait" = 2
    "thread-wait-timeout" = 1
    "thread-exit-code" = 2
    "thread-state" = 1
    "thread-stop" = 1
}
$calls = @($typed.intrinsicCalls | Where-Object {
    $_.runtimeResourceOpName -like "thread-*"
})
foreach ( $entry in $expectedCounts.GetEnumerator() ) {
    $actual = @($calls | Where-Object {
        $_.runtimeResourceOpName -eq $entry.Key
    }).Count
    if ( $actual -ne $entry.Value ) {
        throw "XLIR Typed $($entry.Key) count changed: expected $($entry.Value), got $actual"
    }
}
if ( $calls.Count -ne ($expectedCounts.Values | Measure-Object -Sum).Sum -or
     @($calls | Where-Object {
         $_.kindName -ne "runtime-resource" -or
         $_.admissionName -ne "verified"
     }).Count -ne 0 ) {
    throw "XLIR thread calls are not a closed verified runtime-resource family"
}
foreach ( $call in $calls ) {
    $creates = $call.runtimeResourceOpName -eq "thread-create"
    if ( $creates ) {
        if ( $call.resultType -ne $threadType.id -or
             $call.resultAbiName -ne "owned" -or
             $call.errorAbiName -ne "may-raise" -or
             $call.receiver -ne 0 -or $call.receiverType -ne 0 ) {
            throw "XLIR thread constructor result/error ABI is incomplete"
        }
    } elseif ( $call.resultAbiName -ne "value" -or
               $call.errorAbiName -ne "no-raise" -or
               $call.receiverType -ne $threadType.id ) {
        throw "XLIR thread member receiver/result/effect ABI is incomplete"
    }
    $operands = @($typed.intrinsicOperands |
        Select-Object -Skip $call.firstOperand -First $call.operandCount)
    $actualAbi = ($operands | ForEach-Object { $_.abiName }) -join ','
    $expectedAbi = if ( $call.runtimeResourceOpName -eq
            "thread-wait-timeout" ) { "borrow,value" } else { "borrow" }
    if ( $actualAbi -ne $expectedAbi -or
         (! $creates -and $operands[0].type -ne $threadType.id) ) {
        throw "XLIR $($call.runtimeResourceOpName) canonical operand ABI mismatch: $actualAbi"
    }
    if ( $creates ) {
        $callableType = @($typed.types | Where-Object {
            $_.id -eq $operands[0].type
        })[0]
        $callableResultType = @($typed.types | Where-Object {
            $_.id -eq $callableType.callableResultType
        })[0]
        if ( $null -eq $callableType -or $callableType.kind -ne 4 -or
             $callableType.cAbi -ne 31 -or
             $callableType.ownershipAbi -ne 2 -or
             $callableType.nullAbi -ne 1 -or
             $callableType.callableErasureAbiName -ne "fixed" -or
             $callableType.params.Count -ne 0 -or
             $callableType.returns.Count -ne 1 -or
             $null -eq $callableResultType -or
             $callableResultType.cAbi -ne 3 -or
             $callableResultType.integerTargetName -ne "i64" ) {
            throw "XLIR thread constructor callable ABI is not exact func() as int"
        }
    }
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR thread resource Lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -like "thread-*"
})
foreach ( $entry in $expectedCounts.GetEnumerator() ) {
    $matches = @($operations | Where-Object {
        $_.runtimeOpName -eq $entry.Key
    })
    if ( $matches.Count -ne $entry.Value -or
         @($matches | Where-Object {
             $_.kindName -ne "runtime-call" -or
             $_.targetSymbol -ne 0 -or $_.hasImmediate -ne 0
         }).Count -ne 0 ) {
        throw "XLIR Lowered $($entry.Key) RuntimeOp ABI is incomplete"
    }
}
if ( $operations.Count -ne ($expectedCounts.Values | Measure-Object -Sum).Sum ) {
    throw "XLIR Lowered thread RuntimeOp family contains unaccounted operations"
}
foreach ( $operation in $operations ) {
    if ( $operation.runtimeOpName -eq "thread-create" ) {
        if ( $operation.type -ne $threadType.id -or
             $operation.calleeType -eq 0 -or $operation.operandCount -ne 1 ) {
            throw "XLIR Lowered thread constructor lost callable/resource identity"
        }
    } else {
        $expectedOperands = if ( $operation.runtimeOpName -eq
            "thread-wait-timeout" ) { 2 } else { 1 }
        if ( $operation.calleeType -ne 0 -or
             $operation.operandCount -ne $expectedOperands ) {
            throw "XLIR Lowered thread member lost its closed physical ABI"
        }
    }
}

$threadCaptures = @($lowered.closureCaptures | Where-Object {
    $_.type -eq $threadType.id
})
if ( $threadCaptures.Count -ne 1 -or
     $threadCaptures[0].abiName -ne "retain-xvalue" ) {
    throw "XLIR closure capture does not retain the exact thread xvalue resource"
}
$selectedCreate = @($operations | Where-Object {
    $_.runtimeOpName -eq "thread-create"
} | Sort-Object sourceLine)[0]
$conditionalInstructions = @($lowered.instructions | Where-Object {
    $_.sourceLine -eq $selectedCreate.sourceLine
})
$closureType = $selectedCreate.calleeType
if ( @($conditionalInstructions | Where-Object {
         $_.kindName -eq "retain" -and $_.type -eq $closureType
     }).Count -ne 2 -or
     @($conditionalInstructions | Where-Object {
         $_.kindName -eq "move" -and $_.type -eq $closureType
     }).Count -ne 1 ) {
    throw "XLIR managed conditional closure ownership is not explicit before thread construction"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR thread resource C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR thread resource repeat C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
$second = [System.IO.File]::ReadAllText(
    $generatedCSecond, [System.Text.Encoding]::UTF8)
if ( $c -cne $second ) {
    throw "XLIR thread resource generated C is not byte-for-byte deterministic"
}
foreach ( $required in @(
    "volatile int32_t refs", "xrtRefRetain(&closure->refs)",
    "xrtRefRelease(&closure->refs)",
    "static const xvaluehandleops xlir_thread_handle_ops",
    "xrtValueHandleTake((void**)&thread, &xlir_thread_handle_ops, NULL)",
    "xrtValueTypeIdBind(value, typeId)", "xrtValueGetHandle(value",
    "ops != &xlir_thread_handle_ops", "xlir_closure_retain(callable)",
    "thread requires a callable func value",
    "xlir_thread_wait(", "xlir_thread_wait_timeout(",
    "xlir_thread_exit_code(", "xlir_thread_state(", "xlir_thread_stop("
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR thread mechanical C ABI emission is missing: $required"
    }
}
$typeIdLiteral = $threadType.typeValueId + "ULL"
if ( ([regex]::Matches($c, [regex]::Escape($typeIdLiteral))).Count -ne
        $calls.Count ) {
    throw "XLIR thread C emission does not use the verified TypeValueId at every RuntimeOp"
}
foreach ( $entry in $expectedCounts.GetEnumerator() ) {
    $helper = $entry.Key.Replace('-', '_')
    $pattern = 'v[0-9]+ = xlir_' + $helper + '\('
    if ( ([regex]::Matches($c, $pattern)).Count -ne $entry.Value ) {
        throw "XLIR thread C emission count changed for $($entry.Key)"
    }
}
foreach ( $forbidden in @(
    "XL6_AST_", "xl6IrNodeGet", "xl6IrCgenLegacy", "__xl6_thread_dispatch"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR thread Cgen leaked source/legacy dispatch machinery: $forbidden"
    }
}

$xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_thread_resource_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\test\xlir_thread_resource_abi.exe && build\test\xlir_thread_resource_abi.exe'
& $env:ComSpec /d /v:on /c $xlirCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR thread resource strict-C/runtime gate failed"
}

Write-Output "XLIR thread resource ABI gate passed: exact managed xvalue identity, verified RuntimeOps, retained local capture, owned conditional merge, deterministic mechanical C, and strict runtime."
exit 0
