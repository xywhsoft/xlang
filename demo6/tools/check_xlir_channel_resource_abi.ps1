param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_channel_resource_abi.xl"
$capturedSource = Join-Path $Root "test\cases\xlir_channel_captured_output_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$testDir = Join-Path $Root "build\test"
$typedJson = Join-Path $outputDir "xlir_channel_resource_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_channel_resource_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_channel_resource_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_channel_resource_abi.second.c"
$capturedTypedJson = Join-Path $outputDir "xlir_channel_captured_output_abi.typed.json"
$capturedLoweredJson = Join-Path $outputDir "xlir_channel_captured_output_abi.lowered.json"
$capturedGeneratedC = Join-Path $outputDir "xlir_channel_captured_output_abi.c"
$capturedGeneratedCSecond = Join-Path $outputDir "xlir_channel_captured_output_abi.second.c"

foreach ( $path in @($xl, $source, $capturedSource, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR channel resource ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir, $testDir | Out-Null

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR channel resource Typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$channelTypes = @($typed.types | Where-Object {
    $_.runtimeResourceAbiName -eq "channel" -and $_.params.Count -eq 1
})
$channelCarrierTypes = @($typed.types | Where-Object {
    $_.runtimeResourceAbiName -eq "channel" -and $_.params.Count -eq 0
})
if ( $channelTypes.Count -ne 3 -or $channelCarrierTypes.Count -ne 1 ) {
    throw "XLIR must intern three exact channel<T> instances and one bare generic carrier"
}
foreach ( $channelType in $channelTypes ) {
    $payloadType = @($typed.types | Where-Object {
        $_.id -eq $channelType.params[0]
    })[0]
    if ( $channelType.kind -ne 6 -or $channelType.cAbi -ne 26 -or
         $channelType.ownershipAbi -ne 2 -or $channelType.nullAbi -ne 2 -or
         $channelType.typeValueId -eq "0x0000000000000000" -or
         $null -eq $payloadType -or
         $payloadType.typeValueId -eq "0x0000000000000000" ) {
        throw "XLIR channel<T> type has no exact managed xvalue/null/payload identity ABI"
    }
}

$expectedCounts = [ordered]@{
    "channel-create" = 3
    "channel-send" = 4
    "channel-try-send" = 1
    "channel-recv-into" = 3
    "channel-try-recv-into" = 3
    "channel-recv-timeout-into" = 1
    "channel-close" = 3
    "channel-is-closed" = 3
    "channel-count" = 2
}
$calls = @($typed.intrinsicCalls | Where-Object {
    $_.runtimeResourceOpName -like "channel-*"
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
    throw "XLIR channel calls are not a closed verified runtime-resource family"
}
$expectedPayloadCounts = [ordered]@{
    "trivial-copy" = 8
    "string-copy" = 7
    "xvalue-retain" = 8
}
foreach ( $entry in $expectedPayloadCounts.GetEnumerator() ) {
    $actual = @($calls | Where-Object {
        $_.runtimeResourcePayloadAbiName -eq $entry.Key
    }).Count
    if ( $actual -ne $entry.Value ) {
        throw "XLIR channel payload ABI count changed for $($entry.Key): $actual"
    }
}
foreach ( $call in $calls ) {
    $creates = $call.runtimeResourceOpName -eq "channel-create"
    $closes = $call.runtimeResourceOpName -eq "channel-close"
    $sends = $call.runtimeResourceOpName -eq "channel-send" -or
        $call.runtimeResourceOpName -eq "channel-try-send"
    $receives = $call.runtimeResourceOpName -eq "channel-recv-into" -or
        $call.runtimeResourceOpName -eq "channel-try-recv-into" -or
        $call.runtimeResourceOpName -eq "channel-recv-timeout-into"
    $channelTypeId = if ( $creates ) { $call.resultType } else { $call.receiverType }
    $channelType = @($channelTypes | Where-Object { $_.id -eq $channelTypeId })[0]
    if ( $null -eq $channelType -or
         $call.payloadType -ne $channelType.params[0] ) {
        throw "XLIR channel intrinsic lost its exact channel/payload type relation"
    }
    if ( $creates ) {
        if ( $call.resultAbiName -ne "owned" -or
             $call.errorAbiName -ne "may-raise" -or
             $call.receiver -ne 0 -or $call.receiverType -ne 0 ) {
            throw "XLIR channel constructor result/effect ABI is incomplete"
        }
    } elseif ( $closes ) {
        if ( $call.resultType -ne 0 -or $call.resultAbiName -ne "void" -or
             $call.errorAbiName -ne "no-raise" ) {
            throw "XLIR channel close is not a strict void/no-raise intrinsic"
        }
    } elseif ( $call.resultAbiName -ne "value" -or
               $call.errorAbiName -ne $(if ($sends) { "may-raise" } else { "no-raise" }) ) {
        throw "XLIR channel member result/effect ABI is incomplete"
    }
    $operands = @($typed.intrinsicOperands |
        Select-Object -Skip $call.firstOperand -First $call.operandCount)
    if ( $creates ) {
        if ( $operands.Count -gt 1 -or
             ($operands.Count -eq 1 -and $operands[0].abiName -ne "value") ) {
            throw "XLIR channel constructor capacity ABI is not canonical"
        }
        continue
    }
    if ( $operands.Count -eq 0 -or $operands[0].abiName -ne "borrow" -or
         $operands[0].type -ne $channelType.id ) {
        throw "XLIR channel member receiver is not an exact borrowed resource"
    }
    if ( $sends ) {
        $expectedPayloadOperandAbi = if (
            $call.runtimeResourcePayloadAbiName -eq "trivial-copy"
        ) { "value" } else { "borrow" }
        if ( $operands.Count -ne 2 -or
             $operands[1].type -ne $call.payloadType -or
             $operands[1].abiName -ne $expectedPayloadOperandAbi ) {
            throw "XLIR channel send payload operand ABI is not exact"
        }
    } elseif ( $receives ) {
        $takesTimeout = $call.runtimeResourceOpName -eq
            "channel-recv-timeout-into"
        $referenceType = if ( $operands.Count -ge 2 ) {
            @($typed.types | Where-Object { $_.id -eq $operands[1].type })[0]
        } else { $null }
        if ( $operands.Count -ne $(if ($takesTimeout) { 3 } else { 2 }) -or
             $operands[1].abiName -ne "storage-ref" -or
             $null -eq $referenceType -or $referenceType.params.Count -ne 1 -or
             $referenceType.params[0] -ne $call.payloadType -or
             ($takesTimeout -and $operands[2].abiName -ne "value") ) {
            throw "XLIR channel receive has no exact storage-ref<T>/timeout ABI"
        }
    } elseif ( $operands.Count -ne 1 ) {
        throw "XLIR channel observation/close operand ABI is not closed"
    }
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR channel resource Lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -like "channel-*"
})
foreach ( $entry in $expectedCounts.GetEnumerator() ) {
    $matches = @($operations | Where-Object {
        $_.runtimeOpName -eq $entry.Key
    })
    if ( $matches.Count -ne $entry.Value -or
         @($matches | Where-Object {
             $_.kindName -ne "runtime-call" -or
             $_.targetSymbol -ne 0 -or $_.hasImmediate -ne 0 -or
             $_.payloadType -eq 0 -or
             $_.runtimeResourcePayloadAbiName -eq "invalid"
         }).Count -ne 0 ) {
        throw "XLIR Lowered $($entry.Key) RuntimeOp ABI is incomplete"
    }
}
if ( $operations.Count -ne ($expectedCounts.Values | Measure-Object -Sum).Sum ) {
    throw "XLIR Lowered channel RuntimeOp family contains unaccounted operations"
}
foreach ( $operation in $operations ) {
    $receives = $operation.runtimeOpName -eq "channel-recv-into" -or
        $operation.runtimeOpName -eq "channel-try-recv-into" -or
        $operation.runtimeOpName -eq "channel-recv-timeout-into"
    if ( $operation.runtimeOpName -eq "channel-close" ) {
        if ( $operation.result -ne 0 -or $operation.type -ne 0 -or
             $operation.operandCount -ne 1 ) {
            throw "XLIR Lowered channel close lost its strict void RuntimeOp ABI"
        }
    } elseif ( $receives ) {
        $referenceValue = $operation.operands[1]
        $reference = @($lowered.instructions | Where-Object {
            $_.result -eq $referenceValue
        })[0]
        $referenceType = if ( $null -ne $reference ) {
            @($lowered.types | Where-Object { $_.id -eq $reference.type })[0]
        } else { $null }
        if ( $null -eq $reference -or $reference.kindName -ne "storage-address" -or
             $null -eq $referenceType -or $referenceType.params.Count -ne 1 -or
             $referenceType.params[0] -ne $operation.payloadType ) {
            throw "XLIR Lowered channel receive did not preserve storage-address<T>"
        }
    }
}
$channelErrorSources = @()
foreach ( $test in @($lowered.instructions | Where-Object {
    $_.kindName -eq "error-test"
}) ) {
    $sourceValue = $test.operands[0]
    $definition = @($lowered.instructions | Where-Object {
        $_.result -eq $sourceValue
    })[0]
    if ( $null -ne $definition -and
         $definition.runtimeOpName -in @(
            "channel-create", "channel-send", "channel-try-send"
         ) ) {
        $channelErrorSources += $definition
    }
}
if ( $channelErrorSources.Count -ne 8 ) {
    throw "XLIR channel may-raise RuntimeOps do not all own explicit error edges"
}
$captures = @($lowered.closureCaptures | Where-Object {
    $_.type -in @($channelTypes.id)
})
if ( $captures.Count -ne 1 -or $captures[0].abiName -ne "retain-xvalue" ) {
    throw "XLIR local channel capture does not retain the exact xvalue handle"
}
if ( @($lowered.instructions | Where-Object {
         $_.kindName -eq "storage-address"
     }).Count -ne 7 ) {
    throw "XLIR channel receive storage-address count changed"
}

# Captured output targets are shared cells, not caller stack addresses.  The
# runtime call writes through a function-local temporary and lowering commits
# that temporary to the cell only along the operation's true edge.  Keep this
# contract in JSON so Cgen never has to rediscover closure representation.
& $xl ir $capturedSource --format json --emit $capturedTypedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR captured channel output Typed IR failed" }
$capturedTyped = [System.IO.File]::ReadAllText(
    $capturedTypedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$capturedTypedCalls = @($capturedTyped.intrinsicCalls | Where-Object {
    $_.runtimeResourceOpName -eq "channel-recv-into" -or
    $_.runtimeResourceOpName -eq "channel-try-recv-into" -or
    $_.runtimeResourceOpName -eq "channel-recv-timeout-into"
})
if ( $capturedTypedCalls.Count -ne 6 ) {
    throw "XLIR captured channel output must contain six verified receive operations"
}
foreach ( $payloadAbi in @("trivial-copy", "string-copy", "xvalue-retain") ) {
    if ( @($capturedTypedCalls | Where-Object {
             $_.runtimeResourcePayloadAbiName -eq $payloadAbi
         }).Count -ne 2 ) {
        throw "XLIR captured channel output payload ABI coverage changed: $payloadAbi"
    }
}
foreach ( $call in $capturedTypedCalls ) {
    $operands = @($capturedTyped.intrinsicOperands |
        Select-Object -Skip $call.firstOperand -First $call.operandCount)
    $takesTimeout = $call.runtimeResourceOpName -eq
        "channel-recv-timeout-into"
    if ( $call.admissionName -ne "verified" -or
         $operands.Count -ne $(if ($takesTimeout) { 3 } else { 2 }) -or
         $operands[0].abiName -ne "borrow" -or
         $operands[1].abiName -ne "storage-ref" -or
         ($takesTimeout -and $operands[2].abiName -ne "value") ) {
        throw "XLIR captured channel output lost its verified storage-ref contract"
    }
}

& $xl ir $capturedSource --lowered --format json --emit $capturedLoweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR captured channel output Lowered IR failed" }
$capturedLowered = [System.IO.File]::ReadAllText(
    $capturedLoweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$capturedOperations = @($capturedLowered.instructions | Where-Object {
    $_.runtimeOpName -eq "channel-recv-into" -or
    $_.runtimeOpName -eq "channel-try-recv-into" -or
    $_.runtimeOpName -eq "channel-recv-timeout-into"
})
$capturedCells = @($capturedLowered.closureCaptures | Where-Object {
    $_.abiName -eq "retain-cell"
})
if ( $capturedOperations.Count -ne 6 -or $capturedCells.Count -ne 6 -or
     @($capturedCells | Group-Object type).Count -ne 3 -or
     @($capturedCells | Group-Object type | Where-Object Count -ne 2).Count -ne 0 ) {
    throw "XLIR captured channel outputs do not have exact scalar/string/xvalue cell captures"
}
$definitions = @{}
foreach ( $instruction in $capturedLowered.instructions ) {
    if ( $instruction.result -ne 0 ) {
        $definitions[[string]$instruction.result] = $instruction
    }
}
$storages = @{}
foreach ( $storage in $capturedLowered.storages ) {
    $storages[[string]$storage.symbol] = $storage
}
$cellSymbols = New-Object System.Collections.Generic.HashSet[string]
foreach ( $capture in $capturedCells ) {
    $cellSymbols.Add([string]$capture.storageSymbol) | Out-Null
}
foreach ( $operation in $capturedOperations ) {
    $address = $definitions[[string]$operation.operands[1]]
    $temporary = if ( $null -ne $address ) {
        $storages[[string]$address.targetSymbol]
    } else { $null }
    if ( $null -eq $address -or $address.kindName -ne "storage-address" -or
         $null -eq $temporary -or $temporary.kindName -ne "temporary" -or
         $cellSymbols.Contains([string]$address.targetSymbol) ) {
        throw "XLIR captured channel output exposes a cell or outer activation address"
    }
    $originInstructions = @($capturedLowered.instructions | Where-Object {
        $_.origin -eq $operation.origin
    })
    $commitBranches = @($originInstructions | Where-Object {
        $_.kindName -eq "branch" -and $_.operands.Count -eq 1 -and
        $_.operands[0] -eq $operation.result
    })
    if ( $commitBranches.Count -ne 1 ) {
        throw "XLIR captured channel output has no unique boolean commit edge"
    }
    $initialStores = @($originInstructions | Where-Object {
        $_.kindName -eq "store" -and
        $_.targetSymbol -eq $address.targetSymbol
    })
    $initialValue = if ( $initialStores.Count -eq 1 -and
                         $initialStores[0].operands.Count -eq 1 ) {
        $definitions[[string]$initialStores[0].operands[0]]
    } else { $null }
    $expectedInitialKind = if (
        $operation.runtimeResourcePayloadAbiName -eq "trivial-copy"
    ) { "const" } else { "null" }
    if ( $null -eq $initialValue -or
         $initialValue.kindName -ne $expectedInitialKind -or
         $initialValue.type -ne $operation.payloadType ) {
        throw "XLIR captured channel output temporary has no canonical initial owner"
    }
    if ( $operation.runtimeResourcePayloadAbiName -eq "trivial-copy" ) {
        $loads = @($originInstructions | Where-Object {
            $_.kindName -eq "load" -and
            $_.targetSymbol -eq $address.targetSymbol
        })
        $stores = @($originInstructions | Where-Object {
            $_.kindName -eq "cell-store" -and
            $cellSymbols.Contains([string]$_.targetSymbol)
        })
        if ( $loads.Count -ne 1 -or $stores.Count -ne 1 -or
             $stores[0].operands.Count -ne 1 -or
             $stores[0].operands[0] -ne $loads[0].result ) {
            throw "XLIR captured trivial output has no exact load/cell-store commit"
        }
    } else {
        $takes = @($originInstructions | Where-Object {
            $_.kindName -eq "storage-take" -and
            $_.targetSymbol -eq $address.targetSymbol
        })
        $exchanges = @($originInstructions | Where-Object {
            $_.kindName -eq "cell-exchange" -and
            $cellSymbols.Contains([string]$_.targetSymbol)
        })
        $failureReleases = @($originInstructions | Where-Object {
            $_.kindName -eq "release" -and
            $_.targetSymbol -eq $address.targetSymbol
        })
        if ( $takes.Count -ne 1 -or $exchanges.Count -ne 1 -or
             $failureReleases.Count -ne 1 -or
             $exchanges[0].operands.Count -ne 1 -or
             $exchanges[0].operands[0] -ne $takes[0].result ) {
            throw "XLIR captured managed output has no exact take/exchange/failure-release ownership split"
        }
    }
}
foreach ( $address in @($capturedLowered.instructions | Where-Object {
    $_.kindName -eq "storage-address"
}) ) {
    $storage = $storages[[string]$address.targetSymbol]
    if ( $null -eq $storage -or $storage.kindName -ne "temporary" ) {
        throw "XLIR captured channel output contains a non-local storage address"
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR channel resource C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR channel resource repeat C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
$second = [System.IO.File]::ReadAllText(
    $generatedCSecond, [System.Text.Encoding]::UTF8)
if ( $c -cne $second ) {
    throw "XLIR channel resource generated C is not byte-for-byte deterministic"
}

& $xl cgen $capturedSource --backend xlir --emit $capturedGeneratedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR captured channel output C generation failed" }
& $xl cgen $capturedSource --backend xlir --emit $capturedGeneratedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR captured channel output repeat C generation failed" }
$capturedC = [System.IO.File]::ReadAllText(
    $capturedGeneratedC, [System.Text.Encoding]::UTF8)
$capturedSecond = [System.IO.File]::ReadAllText(
    $capturedGeneratedCSecond, [System.Text.Encoding]::UTF8)
if ( $capturedC -cne $capturedSecond ) {
    throw "XLIR captured channel output generated C is not byte-for-byte deterministic"
}
if ( ([regex]::Matches($capturedC, 'v[0-9]+ = xlir_channel_recv\(')).Count -ne 6 -or
     ! $capturedC.Contains('(xl6CellRuntimeV1Get()->Store)') -or
     ! $capturedC.Contains('(xl6CellRuntimeV1Get()->Exchange)') ) {
    throw "XLIR captured channel output mechanical C lost its verified receive/cell operations"
}
foreach ( $forbidden in @("XL6_AST_", "xl6IrNodeGet", "xl6IrCgenLegacy") ) {
    if ( $capturedC.Contains($forbidden) ) {
        throw "XLIR captured channel output Cgen leaked source/legacy machinery: $forbidden"
    }
}
foreach ( $required in @(
    "typedef struct xlir_channel_handle", "volatile int32_t refs",
    "xrtRefRetain(&handle->refs)", "xrtRefRelease(&handle->refs)",
    "static const xvaluehandleops xlir_channel_handle_ops",
    "xrtValueHandleTake((void**)&handle, &xlir_channel_handle_ops, NULL)",
    "xrtValueTypeIdBind(value, channelTypeId)", "xrtValueGetHandle(value",
    "ops != &xlir_channel_handle_ops", "handle->payload_type_id != payloadTypeId",
    "handle->payload_abi != payloadAbi", "handle->payload_size != payloadSize",
    "xrtChannelDrain(handle->channel, xlir_channel_drain_item, handle)",
    "xlir_channel_payload_copy", "xlir_channel_payload_move",
    "XLIR_CHANNEL_PAYLOAD_TRIVIAL_COPY", "XLIR_CHANNEL_PAYLOAD_STRING_COPY",
    "XLIR_CHANNEL_PAYLOAD_XVALUE_RETAIN", "xlir_channel_timeout_us"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR channel mechanical C ABI emission is missing: $required"
    }
}
foreach ( $channelType in $channelTypes ) {
    $operationCount = @($calls | Where-Object {
        $_.receiverType -eq $channelType.id -or
        ($_.runtimeResourceOpName -eq "channel-create" -and
         $_.resultType -eq $channelType.id)
    }).Count
    $literal = $channelType.typeValueId + "ULL"
    if ( ([regex]::Matches($c, [regex]::Escape($literal))).Count -ne
         $operationCount ) {
        throw "XLIR channel C emission does not use the verified TypeValueId at every RuntimeOp"
    }
}
$helperCounts = [ordered]@{
    'v[0-9]+ = xlir_channel_create\(' = 3
    'v[0-9]+ = xlir_channel_send\(' = 5
    'v[0-9]+ = xlir_channel_recv\(' = 7
    '(?m)^\s+xlir_channel_close\(v' = 3
    'v[0-9]+ = xlir_channel_is_closed\(' = 3
    'v[0-9]+ = xlir_channel_count\(' = 2
}
foreach ( $entry in $helperCounts.GetEnumerator() ) {
    $actual = ([regex]::Matches($c, $entry.Key)).Count
    if ( $actual -ne $entry.Value ) {
        throw "XLIR channel mechanical helper emission count changed: $($entry.Key)"
    }
}
foreach ( $forbidden in @(
    "XL6_AST_", "xl6IrNodeGet", "xl6IrCgenLegacy",
    "__xl6_channel_dispatch", "xl6ChannelCreate(", "xl6ChannelSend("
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR channel Cgen leaked source/legacy dispatch machinery: $forbidden"
    }
}

$xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_channel_resource_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\test\xlir_channel_resource_abi.exe && build\test\xlir_channel_resource_abi.exe'
& $env:ComSpec /d /v:on /c $xlirCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR channel resource strict-C/runtime gate failed"
}

$capturedCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_channel_captured_output_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\test\xlir_channel_captured_output_abi.exe && build\test\xlir_channel_captured_output_abi.exe'
& $env:ComSpec /d /v:on /c $capturedCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR captured channel output strict-C/runtime gate failed"
}

Write-Output "XLIR channel resource ABI gate passed: exact channel<T>/payload identities, explicit storage refs and error edges, captured scalar/string/xvalue outputs through local temporaries and success-only cell commits, deterministic mechanical C, and strict runtime."
exit 0
