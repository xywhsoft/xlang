param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_sync.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$testDir = Join-Path $Root "build\test"
$typedJson = Join-Path $outputDir "xlir_sync_resource_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_sync_resource_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_sync_resource_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_sync_resource_abi.second.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR synchronization resource ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir, $testDir | Out-Null

$typedExpected = [ordered]@{
    "mutex-create" = 4
    "mutex-lock" = 7
    "mutex-try-lock" = 2
    "mutex-unlock" = 10
    "sem-create" = 5
    "sem-wait" = 0
    "sem-try-wait" = 4
    "sem-wait-timeout" = 10
    "sem-post" = 7
    "sem-post-multiple" = 2
    "cond-create" = 2
    "cond-wait" = 0
    "cond-wait-timeout" = 2
    "cond-signal" = 4
    "cond-broadcast" = 2
    "rwlock-create" = 3
    "rwlock-read-lock" = 3
    "rwlock-try-read-lock" = 1
    "rwlock-read-unlock" = 5
    "rwlock-write-lock" = 4
    "rwlock-try-write-lock" = 2
    "rwlock-write-unlock" = 6
    "rwlock-downgrade" = 2
    "rwlock-upgrade" = 2
}
$initializerExpected = [ordered]@{
    "mutex-create" = 2
    "sem-create-default" = 2
    "cond-create" = 2
    "rwlock-create" = 2
}
$loweredExpected = [ordered]@{}
foreach ( $entry in $typedExpected.GetEnumerator() ) {
    $loweredExpected[$entry.Key] = $entry.Value
}
foreach ( $entry in $initializerExpected.GetEnumerator() ) {
    $loweredExpected[$entry.Key] =
        $(if ( $loweredExpected.Contains($entry.Key) ) {
            $loweredExpected[$entry.Key] + $entry.Value
        } else { $entry.Value })
}

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR synchronization resource Typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$resourceNames = @("mutex", "sem", "cond", "rwlock")
$resourceTypes = @($typed.types | Where-Object {
    $_.runtimeResourceAbiName -in $resourceNames
})
if ( $resourceTypes.Count -ne 4 ) {
    throw "XLIR must intern exactly one type for each synchronization resource"
}
foreach ( $name in $resourceNames ) {
    $type = @($resourceTypes | Where-Object {
        $_.runtimeResourceAbiName -eq $name
    })[0]
    if ( $null -eq $type -or $type.kind -ne 1 -or $type.params.Count -ne 0 -or
         $type.cAbi -ne 26 -or $type.ownershipAbi -ne 2 -or
         $type.nullAbi -ne 2 -or $type.typeValueId -eq "0x0000000000000000" ) {
        throw "XLIR $name has no exact managed xvalue/null/TypeValueId ABI"
    }
}

$calls = @($typed.intrinsicCalls | Where-Object {
    $_.runtimeResourceOpName -match '^(mutex|sem|cond|rwlock)-'
})
foreach ( $entry in $typedExpected.GetEnumerator() ) {
    $actual = @($calls | Where-Object {
        $_.runtimeResourceOpName -eq $entry.Key
    }).Count
    if ( $actual -ne $entry.Value ) {
        throw "XLIR Typed $($entry.Key) count changed: expected $($entry.Value), got $actual"
    }
}
if ( $calls.Count -ne ($typedExpected.Values | Measure-Object -Sum).Sum -or
     @($calls | Where-Object {
         $_.kindName -ne "runtime-resource" -or
         $_.admissionName -ne "verified"
     }).Count -ne 0 ) {
    throw "XLIR synchronization calls are not one closed verified RuntimeOp family"
}
$createOps = @("mutex-create", "sem-create", "cond-create", "rwlock-create")
foreach ( $call in $calls ) {
    $creates = $call.runtimeResourceOpName -in $createOps
    $family = ($call.runtimeResourceOpName -split '-')[0]
    $resourceType = @($resourceTypes | Where-Object {
        $_.runtimeResourceAbiName -eq $family
    })[0]
    $operands = @($typed.intrinsicOperands |
        Select-Object -Skip $call.firstOperand -First $call.operandCount)
    if ( $creates ) {
        if ( $call.resultType -ne $resourceType.id -or
             $call.resultAbiName -ne "owned" -or
             $call.errorAbiName -ne "may-raise" -or
             $call.receiver -ne 0 -or $call.receiverType -ne 0 ) {
            throw "XLIR synchronization constructor result/effect ABI is incomplete"
        }
        if ( $call.runtimeResourceOpName -eq "sem-create" ) {
            if ( $operands.Count -ne 2 -or
                 @($operands | Where-Object { $_.abiName -ne "value" }).Count -ne 0 ) {
                throw "XLIR semaphore constructor has no exact pair of value operands"
            }
        } elseif ( $operands.Count -ne 0 ) {
            throw "XLIR zero-argument synchronization constructor gained operands"
        }
        continue
    }
    if ( $call.receiverType -ne $resourceType.id -or
         $call.resultAbiName -ne "value" -or
         $call.errorAbiName -ne "no-raise" -or
         $operands.Count -eq 0 -or
         $operands[0].type -ne $resourceType.id -or
         $operands[0].abiName -ne "borrow" ) {
        throw "XLIR synchronization member lost its exact borrowed receiver ABI"
    }
    if ( $call.runtimeResourceOpName -in @(
            "sem-wait-timeout", "sem-post-multiple"
         ) -and
         ($operands.Count -ne 2 -or $operands[1].abiName -ne "value") ) {
        throw "XLIR semaphore scalar member operand ABI is not canonical"
    }
    if ( $call.runtimeResourceOpName -in @("cond-wait", "cond-wait-timeout") ) {
        $mutexType = @($resourceTypes | Where-Object {
            $_.runtimeResourceAbiName -eq "mutex"
        })[0]
        if ( $operands.Count -ne $(if (
                $call.runtimeResourceOpName -eq "cond-wait"
            ) { 2 } else { 3 }) -or
             $operands[1].type -ne $mutexType.id -or
             $operands[1].abiName -ne "borrow" -or
             ($operands.Count -eq 3 -and $operands[2].abiName -ne "value") ) {
            throw "XLIR condition wait has no exact borrowed mutex/timeout ABI"
        }
    }
}

$initializers = @($typed.valueInitializers | Where-Object {
    $_.runtimeResourceOpName -match '^(mutex|sem|cond|rwlock)-'
})
foreach ( $entry in $initializerExpected.GetEnumerator() ) {
    $matches = @($initializers | Where-Object {
        $_.runtimeResourceOpName -eq $entry.Key
    })
    if ( $matches.Count -ne $entry.Value -or
         @($matches | Where-Object {
             $_.abiName -ne "runtime-resource-default" -or
             $_.errorAbiName -ne "may-raise" -or $_.expression -ne 0
         }).Count -ne 0 ) {
        throw "XLIR default $($entry.Key) initializer ABI is incomplete"
    }
}
if ( $initializers.Count -ne
     ($initializerExpected.Values | Measure-Object -Sum).Sum ) {
    throw "XLIR synchronization default initializer family is not closed"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR synchronization resource Lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -match '^(mutex|sem|cond|rwlock)-'
})
foreach ( $entry in $loweredExpected.GetEnumerator() ) {
    $matches = @($operations | Where-Object {
        $_.runtimeOpName -eq $entry.Key
    })
    if ( $matches.Count -ne $entry.Value -or
         @($matches | Where-Object {
             $_.kindName -ne "runtime-call" -or
             $_.targetSymbol -ne 0 -or $_.hasImmediate -ne 0 -or
             $_.payloadType -ne 0 -or
             $_.runtimeResourcePayloadAbiName -ne "invalid"
         }).Count -ne 0 ) {
        throw "XLIR Lowered $($entry.Key) RuntimeOp ABI is incomplete"
    }
}
if ( $operations.Count -ne ($loweredExpected.Values | Measure-Object -Sum).Sum ) {
    throw "XLIR Lowered synchronization RuntimeOp family contains unaccounted operations"
}
$constructorErrorEdges = 0
$loweredCreateOps = @(
    "mutex-create", "sem-create", "sem-create-default",
    "cond-create", "rwlock-create"
)
foreach ( $test in @($lowered.instructions | Where-Object {
    $_.kindName -eq "error-test"
}) ) {
    $definition = @($lowered.instructions | Where-Object {
        $_.result -eq $test.operands[0]
    })[0]
    if ( $null -ne $definition -and
         $definition.runtimeOpName -in $loweredCreateOps ) {
        $constructorErrorEdges += 1
    }
}
if ( $constructorErrorEdges -ne 22 ) {
    throw "XLIR synchronization constructors/defaults do not own 22 explicit error edges"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR synchronization resource C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR synchronization repeat C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
$second = [System.IO.File]::ReadAllText(
    $generatedCSecond, [System.Text.Encoding]::UTF8)
if ( $c -cne $second ) {
    throw "XLIR synchronization generated C is not byte-for-byte deterministic"
}
foreach ( $required in @(
    "typedef struct xlir_sync_handle", "volatile int32_t refs",
    "static const xvaluehandleops xlir_sync_handle_ops",
    "xrtRefRetain(&handle->refs)", "xrtRefRelease(&handle->refs)",
    "xrtValueHandleTake((void**)&handle, &xlir_sync_handle_ops, NULL)",
    "xrtValueTypeIdBind(value, typeId)", "xrtValueGetHandle(value",
    "ops != &xlir_sync_handle_ops", "handle->type_id != typeId",
    "handle->kind != kind", "xlir_sync_destroy_object",
    "xlir_sync_timeout_us", "xlir_sem_create_default",
    "xrtCondWaitFor((xcond*)h->object, (xmutex*)m->object"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR synchronization mechanical C ABI emission is missing: $required"
    }
}
foreach ( $entry in $loweredExpected.GetEnumerator() ) {
    if ( $entry.Value -eq 0 ) { continue }
    $helper = "xlir_" + $entry.Key.Replace('-', '_')
    $pattern = '(?m)^\s+v[0-9]+ = ' + [regex]::Escape($helper) + '\('
    $actual = ([regex]::Matches($c, $pattern)).Count
    if ( $actual -ne $entry.Value ) {
        throw "XLIR synchronization helper emission count changed for $helper"
    }
}
$expectedTypeUses = @{
    "mutex" = 27
    "sem" = 30
    "cond" = 12
    "rwlock" = 30
}
foreach ( $name in $resourceNames ) {
    $type = @($resourceTypes | Where-Object {
        $_.runtimeResourceAbiName -eq $name
    })[0]
    $literal = $type.typeValueId + "ULL"
    $actual = ([regex]::Matches($c, [regex]::Escape($literal))).Count
    if ( $actual -ne $expectedTypeUses[$name] ) {
        throw "XLIR $name C emission does not use the verified TypeValueId at every RuntimeOp"
    }
}
foreach ( $forbidden in @(
    "XL6_AST_", "xl6IrNodeGet", "xl6IrCgenLegacy",
    "__xl6_sync_dispatch", "xl6MutexCreate(", "xl6SemCreate("
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR synchronization Cgen leaked source/legacy dispatch machinery: $forbidden"
    }
}

$xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_sync_resource_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\test\xlir_sync_resource_abi.exe && build\test\xlir_sync_resource_abi.exe'
& $env:ComSpec /d /v:on /c $xlirCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR synchronization resource strict-C/runtime gate failed"
}

Write-Output "XLIR synchronization resource ABI gate passed: exact mutex/sem/cond/rwlock identities, default constructors and explicit error edges, deterministic mechanical C, and strict runtime."
exit 0
