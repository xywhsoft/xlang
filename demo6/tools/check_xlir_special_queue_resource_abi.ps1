$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot -Parent
$xl = Join-Path $root "release\xl.exe"
$source = Join-Path $root "test\cases\stdlib_special_collections_concurrent.xl"
$typedJson = Join-Path $root "build\tmp\xlir_special_queue_resource_abi.typed.json"
$loweredJson = Join-Path $root "build\tmp\xlir_special_queue_resource_abi.lowered.json"
$generatedC = Join-Path $root "build\tmp\xlir_special_queue_resource_abi.c"
$generatedCSecond = Join-Path $root "build\tmp\xlir_special_queue_resource_abi.second.c"
$legacyC = Join-Path $root "build\tmp\xlir_special_queue_resource_abi.legacy.c"

foreach ( $path in @($xl, $source) ) {
    if ( ! (Test-Path $path) ) { throw "missing XLIR special queue gate input: $path" }
}
New-Item -ItemType Directory -Force (Join-Path $root "build\tmp") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "build\test") | Out-Null

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR special queue Typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$queueTypes = @($typed.types | Where-Object {
    $_.runtimeResourceAbiName -in @("mpmc-queue", "wait-queue") -and
    $_.params.Count -eq 1
})
if ( $queueTypes.Count -ne 2 ) {
    throw "XLIR special queue gate lost exact generic MpmcQueue/WaitQueue types"
}
foreach ( $queueType in $queueTypes ) {
    if ( $queueType.kind -ne 6 -or $queueType.cAbi -ne 26 -or
         $queueType.ownershipAbi -ne 2 -or $queueType.nullAbi -ne 2 -or
         $queueType.typeValueId -eq "0x0000000000000000" ) {
        throw "XLIR special queue type has no exact managed xvalue identity ABI"
    }
}

$expectedCounts = [ordered]@{
    "special-queue-create" = 2
    "special-queue-try-push" = 5
    "special-queue-try-pop-into" = 1
    "special-queue-pop-timeout-into" = 1
    "special-queue-close" = 1
    "special-queue-count" = 2
    "special-queue-is-closed" = 1
    "special-queue-is-drained" = 1
    "special-queue-push-batch" = 1
    "special-queue-pop-batch-into" = 3
}
$calls = @($typed.intrinsicCalls | Where-Object {
    $_.runtimeResourceOpName -like "special-queue-*"
})
foreach ( $entry in $expectedCounts.GetEnumerator() ) {
    if ( @($calls | Where-Object {
             $_.runtimeResourceOpName -eq $entry.Key
         }).Count -ne $entry.Value ) {
        throw "XLIR Typed special queue operation count changed: $($entry.Key)"
    }
}
if ( $calls.Count -ne ($expectedCounts.Values | Measure-Object -Sum).Sum ) {
    throw "XLIR Typed special queue family contains unaccounted operations"
}
foreach ( $call in $calls ) {
    $operands = @($typed.intrinsicOperands | Select-Object -Skip $call.firstOperand |
        Select-Object -First $call.operandCount)
    if ( $call.kindName -ne "runtime-resource" -or
         $call.admissionName -ne "verified" -or
         $call.collectionAbiName -ne "none" -or
         $call.payloadType -eq 0 -or
         $call.runtimeResourcePayloadAbiName -eq "invalid" ) {
        throw "XLIR Typed special queue call has ambiguous identity metadata"
    }
    if ( $call.runtimeResourceOpName -eq "special-queue-create" ) {
        if ( $call.resultAbiName -ne "owned" -or
             $call.errorAbiName -ne "may-raise" -or
             $call.receiver -ne 0 -or $operands.Count -ne 1 -or
             $operands[0].abiName -ne "value" ) {
            throw "XLIR special queue create ABI is not exact"
        }
    } elseif ( $call.runtimeResourceOpName -eq "special-queue-try-push" ) {
        $expectedPayloadAbi = if (
            $call.runtimeResourcePayloadAbiName -eq "trivial-copy"
        ) { "value" } else { "borrow" }
        if ( $call.resultAbiName -ne "value" -or
             $call.errorAbiName -ne "may-raise" -or
             $operands.Count -ne 2 -or
             $operands[0].abiName -ne "borrow" -or
             $operands[1].abiName -ne $expectedPayloadAbi -or
             $operands[1].type -ne $call.payloadType ) {
            throw "XLIR special queue tryPush payload/effect ABI is not exact"
        }
    } elseif ( $call.runtimeResourceOpName -in @(
        "special-queue-try-pop-into", "special-queue-pop-timeout-into"
    ) ) {
        $takesTimeout = $call.runtimeResourceOpName -eq
            "special-queue-pop-timeout-into"
        $referenceType = @($typed.types | Where-Object {
            $_.id -eq $operands[1].type
        })[0]
        if ( $call.errorAbiName -ne "no-raise" -or
             $operands.Count -ne $(if ($takesTimeout) { 3 } else { 2 }) -or
             $operands[0].abiName -ne "borrow" -or
             $operands[1].abiName -ne "storage-ref" -or
             $null -eq $referenceType -or $referenceType.params.Count -ne 1 -or
             $referenceType.params[0] -ne $call.payloadType -or
             ($takesTimeout -and $operands[2].abiName -ne "value") ) {
            throw "XLIR special queue receive storage-ref/timeout ABI is not exact"
        }
    } elseif ( $call.runtimeResourceOpName -in @(
        "special-queue-push-batch", "special-queue-pop-batch-into"
    ) ) {
        $isPop = $call.runtimeResourceOpName -eq
            "special-queue-pop-batch-into"
        $arrayType = @($typed.types | Where-Object {
            $_.id -eq $operands[1].type
        })[0]
        if ( $call.resultAbiName -ne "value" -or
             $call.errorAbiName -ne "no-raise" -or
             $operands.Count -ne $(if ($isPop) { 3 } else { 2 }) -or
             $operands[0].abiName -ne "borrow" -or
             $operands[1].abiName -ne "borrow" -or
             $null -eq $arrayType -or $arrayType.collectionAbiName -ne "array" -or
             $arrayType.params.Count -ne 1 -or
             $arrayType.params[0] -ne $call.payloadType -or
             ($isPop -and $operands[2].abiName -ne "value") ) {
            throw "XLIR special queue batch array/capacity/effect ABI is not exact"
        }
    } elseif ( $call.runtimeResourceOpName -eq "special-queue-close" ) {
        if ( $call.resultAbiName -ne "void" -or
             $call.errorAbiName -ne "no-raise" -or
             $operands.Count -ne 1 -or $operands[0].abiName -ne "borrow" ) {
            throw "XLIR special queue close ABI is not exact"
        }
    } elseif ( $call.errorAbiName -ne "no-raise" -or
               $operands.Count -ne 1 -or
               $operands[0].abiName -ne "borrow" ) {
        throw "XLIR special queue observation ABI is not exact"
    }
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR special queue Lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -like "special-queue-*"
})
foreach ( $entry in $expectedCounts.GetEnumerator() ) {
    $matches = @($operations | Where-Object { $_.runtimeOpName -eq $entry.Key })
    if ( $matches.Count -ne $entry.Value -or
         @($matches | Where-Object {
             $_.kindName -ne "runtime-call" -or $_.targetSymbol -ne 0 -or
             $_.hasImmediate -ne 0 -or $_.payloadType -eq 0 -or
             $_.runtimeResourcePayloadAbiName -eq "invalid"
         }).Count -ne 0 ) {
        throw "XLIR Lowered special queue RuntimeOp ABI is incomplete: $($entry.Key)"
    }
}
foreach ( $operation in $operations ) {
    if ( $operation.runtimeOpName -eq "special-queue-close" ) {
        if ( $operation.result -ne 0 -or $operation.type -ne 0 -or
             $operation.operandCount -ne 1 ) {
            throw "XLIR Lowered special queue close lost its void ABI"
        }
    } elseif ( $operation.runtimeOpName -in @(
        "special-queue-try-pop-into", "special-queue-pop-timeout-into"
    ) ) {
        $reference = @($lowered.instructions | Where-Object {
            $_.result -eq $operation.operands[1]
        })[0]
        $referenceType = if ( $null -ne $reference ) {
            @($lowered.types | Where-Object { $_.id -eq $reference.type })[0]
        } else { $null }
        if ( $null -eq $reference -or $reference.kindName -ne "storage-address" -or
             $null -eq $referenceType -or $referenceType.params.Count -ne 1 -or
             $referenceType.params[0] -ne $operation.payloadType ) {
            throw "XLIR Lowered special queue receive lost storage-address<T>"
        }
    } elseif ( $operation.runtimeOpName -in @(
        "special-queue-push-batch", "special-queue-pop-batch-into"
    ) ) {
        $arrayDefinition = @($lowered.instructions | Where-Object {
            $_.result -eq $operation.operands[1]
        })[0]
        $arrayType = if ( $null -ne $arrayDefinition ) {
            @($lowered.types | Where-Object { $_.id -eq $arrayDefinition.type })[0]
        } else { $null }
        if ( $operation.result -eq 0 -or $operation.operandCount -ne
                 $(if ($operation.runtimeOpName -eq
                     "special-queue-pop-batch-into") { 3 } else { 2 }) -or
             $null -eq $arrayType -or $arrayType.collectionAbiName -ne "array" -or
             $arrayType.params.Count -ne 1 -or
             $arrayType.params[0] -ne $operation.payloadType ) {
            throw "XLIR Lowered special queue batch lost exact array<T> ABI"
        }
    }
}
$queueErrorSources = @()
foreach ( $test in @($lowered.instructions | Where-Object {
    $_.kindName -eq "error-test"
}) ) {
    $definition = @($lowered.instructions | Where-Object {
        $_.result -eq $test.operands[0]
    })[0]
    if ( $null -ne $definition -and $definition.runtimeOpName -in @(
        "special-queue-create", "special-queue-try-push"
    ) ) { $queueErrorSources += $definition }
}
if ( $queueErrorSources.Count -ne 7 ) {
    throw "XLIR special queue may-raise RuntimeOps lost explicit error edges"
}
$queueTypeIds = @($queueTypes.id)
$captures = @($lowered.closureCaptures | Where-Object {
    $_.type -in $queueTypeIds
})
if ( $captures.Count -ne 5 -or
     @($captures | Where-Object { $_.abiName -ne "retain-xvalue" }).Count -ne 0 ) {
    throw "XLIR special queue closure captures do not retain exact xvalue handles"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR special queue C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR special queue repeat C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
$second = [System.IO.File]::ReadAllText(
    $generatedCSecond, [System.Text.Encoding]::UTF8)
if ( $c -cne $second ) {
    throw "XLIR special queue generated C is not byte-for-byte deterministic"
}
foreach ( $required in @(
    "typedef struct xlir_channel_handle", "xrtRefRetain(&handle->refs)",
    "xrtRefRelease(&handle->refs)", "ops != &xlir_channel_handle_ops",
    "handle->payload_type_id != payloadTypeId",
    "xrtChannelDrain(handle->channel, xlir_channel_drain_item, handle)",
    "xlir_special_queue_create", "xlir_special_queue_try_push",
    "xlir_special_queue_pop", "xlir_special_queue_close",
    "xlir_special_queue_count", "xlir_special_queue_capacity",
    "xlir_special_queue_reset", "xlir_special_queue_is_closed",
    "xlir_special_queue_is_drained", "xrtChannelRecvFor",
    "xrtChannelIsDrained", "xrtChannelReset",
    "xlir_special_queue_push_batch", "xlir_special_queue_pop_batch_into",
    "xrtValueToTyped", "xrtValueFromTyped", "xrtValueTypeIdRebind"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR special queue mechanical C ABI emission is missing: $required"
    }
}
foreach ( $queueType in $queueTypes ) {
    $operationCount = @($calls | Where-Object {
        $_.receiverType -eq $queueType.id -or
        ($_.runtimeResourceOpName -eq "special-queue-create" -and
         $_.resultType -eq $queueType.id)
    }).Count
    $literal = $queueType.typeValueId + "ULL"
    if ( ([regex]::Matches($c, [regex]::Escape($literal))).Count -ne
         $operationCount ) {
        throw "XLIR special queue C does not use exact TypeValueId per RuntimeOp"
    }
}
$helperCounts = [ordered]@{
    'v[0-9]+ = xlir_special_queue_create\(' = 2
    'v[0-9]+ = xlir_special_queue_try_push\(' = 5
    'v[0-9]+ = xlir_special_queue_pop\(' = 2
    '(?m)^\s+xlir_special_queue_close\(v' = 1
    'v[0-9]+ = xlir_special_queue_count\(' = 2
    'v[0-9]+ = xlir_special_queue_is_closed\(' = 1
    'v[0-9]+ = xlir_special_queue_is_drained\(' = 1
    'v[0-9]+ = xlir_special_queue_push_batch\(' = 1
    'v[0-9]+ = xlir_special_queue_pop_batch_into\(' = 3
}
foreach ( $entry in $helperCounts.GetEnumerator() ) {
    if ( ([regex]::Matches($c, $entry.Key)).Count -ne $entry.Value ) {
        throw "XLIR special queue helper emission count changed: $($entry.Key)"
    }
}
foreach ( $forbidden in @(
    "XL6_AST_", "xl6IrNodeGet", "xl6IrCgenLegacy",
    "xl6SpecialQueue", "__xl6_special_queue_dispatch"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR special queue Cgen leaked source/legacy dispatch: $forbidden"
    }
}

$xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_special_queue_resource_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\test\xlir_special_queue_resource_abi.exe && build\test\xlir_special_queue_resource_abi.exe'
& $env:ComSpec /d /v:on /c $xlirCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR special queue strict-C/runtime gate failed"
}

& $xl cgen $source --backend legacy --emit $legacyC
if ( $LASTEXITCODE -ne 0 ) { throw "special queue legacy C generation failed" }
$legacyCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! build\tmp\xlir_special_queue_resource_abi.legacy.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_special_queue_resource_abi.legacy.exe && build\test\xlir_special_queue_resource_abi.legacy.exe'
& $env:ComSpec /d /v:on /c $legacyCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR special queue legacy differential runtime gate failed"
}

Write-Output "XLIR special queue resource ABI gate passed: exact queue/payload/array identities, scalar and batch effects, storage refs and error edges, retained thread captures, deterministic mechanical C, strict runtime, and legacy differential semantics."
exit 0
