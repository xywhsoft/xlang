$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot -Parent
$xl = Join-Path $root "release\xl.exe"
$source = Join-Path $root "test\cases\xlir_stack_resource_abi.xl"
$classDefaultSource = Join-Path $root "test\cases\xlir_stack_class_default_abi.xl"
$typedJson = Join-Path $root "build\tmp\xlir_stack_resource_abi.typed.json"
$loweredJson = Join-Path $root "build\tmp\xlir_stack_resource_abi.lowered.json"
$classDefaultTypedJson = Join-Path $root "build\tmp\xlir_stack_class_default_abi.typed.json"
$classDefaultLoweredJson = Join-Path $root "build\tmp\xlir_stack_class_default_abi.lowered.json"
$generatedC = Join-Path $root "build\tmp\xlir_stack_resource_abi.c"
$generatedCSecond = Join-Path $root "build\tmp\xlir_stack_resource_abi.second.c"
$classDefaultC = Join-Path $root "build\tmp\xlir_stack_class_default_abi.c"

foreach ( $path in @($xl, $source, $classDefaultSource,
        (Join-Path $root "test\cases\xlir_stack_resource_abi_host.c")) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "missing XLIR Stack resource gate input: $path"
    }
}
New-Item -ItemType Directory -Force (Join-Path $root "build\tmp") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "build\test") | Out-Null

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Stack resource Typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$stackTypes = @($typed.types | Where-Object {
    $_.runtimeResourceAbiName -eq "stack" -and $_.params.Count -eq 1
})
if ( $stackTypes.Count -ne 3 -or @($stackTypes | Where-Object {
        $_.kind -ne 6 -or $_.cAbi -ne 26 -or $_.ownershipAbi -ne 2 -or
        $_.nullAbi -ne 2 -or $_.typeValueId -eq "0x0000000000000000"
    }).Count -ne 0 ) {
    throw "XLIR Stack<T> types lost their exact managed xvalue identities"
}

$resourceInitializers = @($typed.valueInitializers | Where-Object {
    $_.abiName -eq "runtime-resource-default" -and
    $_.runtimeResourceOpName -eq "stack-create"
})
if ( $resourceInitializers.Count -ne 1 -or
     $resourceInitializers[0].payloadType -eq 0 -or
     $resourceInitializers[0].runtimeResourcePayloadAbiName -ne "trivial-copy" -or
     $resourceInitializers[0].errorAbiName -ne "may-raise" ) {
    throw "XLIR default Stack<T> declaration has no exact resource initializer ABI"
}
$resourceFields = @($typed.classFields | Where-Object {
    $_.initializerAbiName -eq "runtime-resource-default"
})
if ( $resourceFields.Count -ne 1 -or
     $resourceFields[0].initializerRuntimeResourceOpName -ne "stack-create" -or
     $resourceFields[0].initializerPayloadType -eq 0 -or
     $resourceFields[0].initializerRuntimeResourcePayloadAbiName -ne "string-copy" -or
     $resourceFields[0].initializerErrorAbiName -ne "may-raise" ) {
    throw "XLIR Stack<T> class field has no exact default resource/effect contract"
}

$expectedTypedCounts = [ordered]@{
    "stack-create" = 2
    "stack-push" = 7
    "stack-pop" = 3
    "stack-peek" = 3
    "stack-peek-at" = 2
    "stack-reserve" = 1
    "stack-clone" = 1
    "stack-equals" = 2
    "stack-trim" = 1
    "stack-clear" = 1
    "stack-count" = 6
    "stack-capacity" = 2
}
$calls = @($typed.intrinsicCalls | Where-Object {
    $_.runtimeResourceOpName -like "stack-*"
})
foreach ( $entry in $expectedTypedCounts.GetEnumerator() ) {
    if ( @($calls | Where-Object {
             $_.runtimeResourceOpName -eq $entry.Key
         }).Count -ne $entry.Value ) {
        throw "XLIR Typed Stack operation count changed: $($entry.Key)"
    }
}
if ( $calls.Count -ne ($expectedTypedCounts.Values | Measure-Object -Sum).Sum ) {
    throw "XLIR Typed Stack family contains unaccounted operations"
}
foreach ( $call in $calls ) {
    $operands = @($typed.intrinsicOperands | Select-Object -Skip $call.firstOperand |
        Select-Object -First $call.operandCount)
    if ( $call.kindName -ne "runtime-resource" -or
         $call.admissionName -ne "verified" -or $call.payloadType -eq 0 -or
         $call.runtimeResourcePayloadAbiName -eq "invalid" ) {
        throw "XLIR Typed Stack call has ambiguous resource/payload identity"
    }
    if ( $call.runtimeResourceOpName -eq "stack-create" ) {
        if ( $call.resultAbiName -ne "owned" -or
             $call.errorAbiName -ne "may-raise" -or
             $call.receiver -ne 0 -or $operands.Count -ne 0 ) {
            throw "XLIR Stack create ABI is not exact"
        }
        continue
    }
    if ( $operands.Count -eq 0 -or $operands[0].abiName -ne "borrow" -or
         $operands[0].type -ne $call.receiverType ) {
        throw "XLIR Stack member has no exact borrowed receiver ABI"
    }
    if ( $call.runtimeResourceOpName -eq "stack-push" ) {
        $payloadAbi = if (
            $call.runtimeResourcePayloadAbiName -eq "trivial-copy"
        ) { "value" } else { "borrow" }
        if ( $operands.Count -ne 2 -or $operands[1].type -ne $call.payloadType -or
             $operands[1].abiName -ne $payloadAbi -or
             $call.resultAbiName -ne "value" -or
             $call.errorAbiName -ne "may-raise" ) {
            throw "XLIR Stack push payload/effect ABI is not exact"
        }
    } elseif ( $call.runtimeResourceOpName -in @(
            "stack-pop", "stack-peek", "stack-peek-at") ) {
        $indexed = $call.runtimeResourceOpName -eq "stack-peek-at"
        $payloadType = @($typed.types | Where-Object { $_.id -eq $call.payloadType })[0]
        $resultAbi = if ($payloadType.ownershipAbi -eq 1) { 'value' } else { 'owned' }
        if ( $operands.Count -ne $(if ($indexed) { 2 } else { 1 }) -or
             $call.resultType -ne $call.payloadType -or
             $call.resultAbiName -ne $resultAbi -or
             $call.errorAbiName -ne 'may-raise' -or
             @($operands | Where-Object abiName -eq 'storage-ref').Count -ne 0 -or
             ($indexed -and $operands[1].abiName -ne 'value') ) {
            throw "XLIR Stack pop/peek exact result or effect ABI is not exact"
        }
    }
}

& $xl ir $classDefaultSource --format json --emit $classDefaultTypedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Stack class-default Typed IR failed" }
$classDefaultTyped = [System.IO.File]::ReadAllText(
    $classDefaultTypedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$classDefaults = @($classDefaultTyped.valueInitializers | Where-Object {
    $_.abiName -eq "class-default"
})
$classResourceFields = @($classDefaultTyped.classFields | Where-Object {
    $_.initializerAbiName -eq "runtime-resource-default"
})
if ( $classDefaults.Count -ne 1 -or
     $classDefaults[0].errorAbiName -ne "may-raise" -or
     $classResourceFields.Count -ne 1 -or
     $classResourceFields[0].initializerRuntimeResourcePayloadAbiName -ne
        "string-copy" ) {
    throw "XLIR class-default initializer does not aggregate its resource-field effect"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Stack resource Lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -like "stack-*"
})
$expectedLoweredCounts = [ordered]@{}
foreach ( $entry in $expectedTypedCounts.GetEnumerator() ) {
    $expectedLoweredCounts[$entry.Key] = if (
        $entry.Key -eq "stack-create"
    ) { 6 } else { $entry.Value }
}
foreach ( $entry in $expectedLoweredCounts.GetEnumerator() ) {
    $matches = @($operations | Where-Object { $_.runtimeOpName -eq $entry.Key })
    if ( $matches.Count -ne $entry.Value -or @($matches | Where-Object {
            $_.kindName -ne "runtime-call" -or $_.targetSymbol -ne 0 -or
            $_.hasImmediate -ne 0 -or $_.payloadType -eq 0 -or
            $_.runtimeResourcePayloadAbiName -eq "invalid"
        }).Count -ne 0 ) {
        throw "XLIR Lowered Stack RuntimeOp ABI is incomplete: $($entry.Key)"
    }
}
$definitions = @{}
foreach ( $instruction in $lowered.instructions ) {
    if ( $instruction.result -ne 0 ) {
        $definitions[[string]$instruction.result] = $instruction
    }
}
$stackErrorSources = @()
foreach ( $test in @($lowered.instructions | Where-Object {
        $_.kindName -eq "error-test"
    }) ) {
    $definition = $definitions[[string]$test.operands[0]]
    if ( $null -ne $definition -and $definition.runtimeOpName -like "stack-*" ) {
        $stackErrorSources += $definition
    }
}
if ( $stackErrorSources.Count -ne @($operations | Where-Object {$_.runtimeOpName -ne 'stack-clear'}).Count ) {
    throw "XLIR Stack may-raise RuntimeOps lost explicit error-test CFG edges"
}
$stackTypeIds = @($stackTypes.id)
$captures = @($lowered.closureCaptures | Where-Object {
    $_.type -in $stackTypeIds
})
if ( $captures.Count -ne 1 -or $captures[0].abiName -ne "retain-xvalue" ) {
    throw "XLIR Stack closure capture does not retain its exact xvalue handle"
}

& $xl ir $classDefaultSource --lowered --format json --emit $classDefaultLoweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Stack class-default Lowered IR failed" }
$classDefaultLowered = [System.IO.File]::ReadAllText(
    $classDefaultLoweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$classCreates = @($classDefaultLowered.instructions | Where-Object {
    $_.runtimeOpName -eq "stack-create"
})
$classErrorTests = @($classDefaultLowered.instructions | Where-Object {
    $_.kindName -eq "error-test" -and $_.operands[0] -eq $classCreates[0].result
})
$classFailureBlock = @($classDefaultLowered.blocks | Where-Object {
    $_.id -eq $classErrorTests[0].falseTarget
})[0]
$classFailureInstructions = @($classDefaultLowered.instructions | Select-Object `
    -Skip $classFailureBlock.firstInstruction -First $classFailureBlock.instructionCount)
$classFailureReleases = @($classFailureInstructions | Where-Object {
    $_.kindName -eq "release" -and $_.origin -eq $classCreates[0].origin
})
if ( $classCreates.Count -ne 1 -or $classErrorTests.Count -ne 1 -or
     $classFailureReleases.Count -ne 1 ) {
    throw "XLIR Stack class-field create has no explicit error test and partial-object cleanup"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Stack resource C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Stack repeat C generation failed" }
& $xl cgen $classDefaultSource --backend xlir --emit $classDefaultC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Stack class-default C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
$second = [System.IO.File]::ReadAllText(
    $generatedCSecond, [System.Text.Encoding]::UTF8)
if ( $c -cne $second ) {
    throw "XLIR Stack generated C is not byte-for-byte deterministic"
}
foreach ( $required in @(
    "xl6StackCreateV1", "xl6StackPushV1", "xl6StackTakeV1",
    "xl6StackReserveV1", "xl6StackCloneV1", "xl6StackEqualsV1", "xl6StackTrimV1",
    "xl6StackClearV1", "xl6StackSizeV1", "xrtTypeStringView()", "xrtTypeValue()"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR Stack mechanical resident ABI emission is missing: $required"
    }
}
$helperCounts = [ordered]@{
    'v[0-9]+ = xl6StackCreateV1\(' = 6
    'v[0-9]+ = xl6StackPushV1\(' = 7
    '(?m)^\s+xl6StackTakeV1\(v' = 8
    'v[0-9]+ = xl6StackCloneV1\(' = 1
    'v[0-9]+ = xl6StackEqualsV1\(' = 2
    'v[0-9]+ = xl6StackReserveV1\(' = 1
    'v[0-9]+ = xl6StackTrimV1\(' = 1
    '(?m)^\s+xl6StackClearV1\(v' = 1
    'v[0-9]+ = xl6StackSizeV1\(' = 8
}
foreach ( $entry in $helperCounts.GetEnumerator() ) {
    if ( ([regex]::Matches($c, $entry.Key)).Count -ne $entry.Value ) {
        throw "XLIR Stack helper emission count changed: $($entry.Key)"
    }
}
foreach ( $forbidden in @(
    "XL6_AST_", "xl6IrNodeGet", "xl6IrCgenLegacy",
    "xl6SpecialStack", "__xl6_special_stack_dispatch", "typedef struct xlir_stack_handle",
    "xlir_stack_payload_", "xlir_stack_reserve_handle", "xlir_stack_raise",
    "xl6StackPopOrPeekV1", "xl6StackPeekAtV1"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR Stack Cgen leaked source/legacy dispatch: $forbidden"
    }
}

$xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_stack_resource_abi.c test\cases\xlir_stack_resource_abi_host.c !XL6_CGEN_LINKS! -o build\test\xlir_stack_resource_abi.exe && build\test\xlir_stack_resource_abi.exe'
& $env:ComSpec /d /v:on /c $xlirCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Stack strict-C/runtime gate failed" }
$classDefaultCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_stack_class_default_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\test\xlir_stack_class_default_abi.exe && build\test\xlir_stack_class_default_abi.exe'
& $env:ComSpec /d /v:on /c $classDefaultCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR Stack class-default strict-C/runtime gate failed"
}

Write-Output "XLIR Stack resource ABI gate passed: exact Stack<T>/payload identities, class-field resource defaults and partial-construction cleanup, owned/value results without storage refs, retained closure capture, deterministic mechanical C, and strict runtime."
exit 0
