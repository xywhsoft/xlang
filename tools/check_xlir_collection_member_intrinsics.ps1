param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
$defaultHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$programEntryHost = Join-Path $Root "test\cases\xlir_program_entry_host.c"
$fixtures = @(
    @{
        Name = "cgen_param_modes_container_matrix"
        Host = $programEntryHost
        GenericHost = $true
        Expected = @{
            "set-add:set" = 2
            "collection-contains:list" = 1
            "collection-contains:set" = 2
        }
    },
    @{
        Name = "cgen_typed_container_param_mutation"
        Host = $defaultHost
        Expected = @{
            "array-append:array" = 4
            "set-add:set" = 1
            "collection-remove:set" = 1
            "collection-contains:set" = 2
        }
    }
)

foreach ( $path in @($xl, $defaultHost, $programEntryHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR collection-member intrinsic gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Read-IrModule([string]$source, [bool]$lowered, [string]$output) {
    $arguments = @("ir", $source)
    if ( $lowered ) { $arguments += "--lowered" }
    $arguments += @("--format", "json", "--emit", $output)
    & $xl @arguments
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR collection-member IR dump failed: $source"
    }
    return [System.IO.File]::ReadAllText(
        $output, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Get-Type($module, [int]$id) {
    return @($module.types | Where-Object { [int]$_.id -eq $id })[0]
}

function Get-FunctionBody($module, $function) {
    $first = [int]$function.firstInstruction + 1
    $last = [int]$function.firstInstruction + [int]$function.instructionCount
    return @($module.instructions | Where-Object {
        [int]$_.id -ge $first -and [int]$_.id -le $last
    })
}

foreach ( $fixture in $fixtures ) {
    $source = Join-Path $Root "test\cases\$($fixture.Name).xl"
    if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
        throw "XLIR collection-member fixture is missing: $source"
    }
    $typedPath = Join-Path $outputDir "$($fixture.Name).collection-member.typed.json"
    $loweredPath = Join-Path $outputDir "$($fixture.Name).collection-member.lowered.json"
    $typed = Read-IrModule $source $false $typedPath
    $lowered = Read-IrModule $source $true $loweredPath
    $intrinsics = @($typed.intrinsicCalls | Where-Object {
        $_.kindName -in @(
            "array-append", "set-add", "collection-contains", "collection-remove")
    })

    foreach ( $expected in $fixture.Expected.GetEnumerator() ) {
        $parts = $expected.Key.Split(":")
        $matches = @($intrinsics | Where-Object {
            $_.kindName -eq $parts[0] -and $_.collectionAbiName -eq $parts[1]
        })
        if ( $matches.Count -ne [int]$expected.Value ) {
            throw "XLIR typed collection-member identity count changed: $($fixture.Name) $($expected.Key)"
        }
    }
    if ( $intrinsics.Count -ne ($fixture.Expected.Values | Measure-Object -Sum).Sum ) {
        throw "XLIR typed collection-member gate found an unexpected member intrinsic"
    }

    foreach ( $intrinsic in $intrinsics ) {
        $receiverType = Get-Type $typed ([int]$intrinsic.receiverType)
        $operands = @($typed.intrinsicOperands | Select-Object -Skip ([int]$intrinsic.firstOperand) -First ([int]$intrinsic.operandCount))
        $expectedErrorAbi = if ($intrinsic.kindName -in @('array-append', 'set-add')) { 'may-raise' } else { 'no-raise' }
        if ( $intrinsic.admissionName -ne "verified" -or
             $intrinsic.resultAbiName -ne "value" -or
             $intrinsic.errorAbiName -ne $expectedErrorAbi -or
             $null -eq $receiverType -or
             $receiverType.collectionAbiName -ne $intrinsic.collectionAbiName -or
             $operands.Count -ne 1 ) {
            throw "XLIR collection-member intrinsic has no exact verified receiver/result ABI"
        }
        $expectedOperandAbi = if ( $intrinsic.kindName -in @("array-append", "set-add") ) {
            "consume"
        } else {
            $operandType = Get-Type $typed ([int]$operands[0].type)
            if ( $null -ne $operandType -and [int]$operandType.ownershipAbi -eq 2 ) {
                "borrow"
            } else {
                "value"
            }
        }
        if ( $operands[0].abiName -ne $expectedOperandAbi ) {
            throw "XLIR collection-member operand ownership is not canonical: $($intrinsic.kindName)"
        }

        $expectedRuntime = switch ( $intrinsic.kindName ) {
            "array-append" { "xvalue-array-append-new" }
            "set-add" { "xvalue-set-add-new" }
            "collection-contains" {
                if ( $intrinsic.collectionAbiName -eq "set" ) {
                    "xvalue-set-has"
                } else {
                    "xvalue-contains"
                }
            }
            "collection-remove" { "xvalue-collection-remove" }
            default { throw "Unknown XLIR collection-member intrinsic" }
        }
        $runtime = @($lowered.instructions | Where-Object {
            [int]$_.origin -eq [int]$intrinsic.origin -and
            $_.runtimeOpName -eq $expectedRuntime
        })
        if ( $runtime.Count -ne 1 ) {
            throw "XLIR collection-member intrinsic has no unique lowered runtime operation"
        }
        if ($expectedErrorAbi -eq 'may-raise') {
            $test = $lowered.instructions[$runtime[0].index + 1]
            if ($test.kindName -ne 'error-test' -or $test.operands[0] -ne $runtime[0].result -or
                $test.trueTarget -eq $test.falseTarget) {
                throw 'Allocating collection mutation has no distinct checked success/failure edges'
            }
        }
        $expectedInstructionCollection = if ( $intrinsic.kindName -eq "collection-remove" ) {
            $intrinsic.collectionAbiName
        } else {
            "none"
        }
        if ( $runtime[0].collectionAbiName -ne $expectedInstructionCollection ) {
            throw "XLIR collection-member runtime operation lost its explicit collection ABI"
        }
    }

    if ( $fixture.Name -eq "cgen_typed_container_param_mutation" ) {
        $borrowedOwnerCount = 0
        foreach ( $function in @($lowered.functions | Where-Object {
            $_.kindName -eq "user" -and [int]$_.parameterCount -gt 0
        })) {
            $target = @($lowered.callTargets | Where-Object {
                [uint64]$_.symbol -eq [uint64]$function.symbol
            })[0]
            if ( $null -eq $target ) { continue }
            $body = Get-FunctionBody $lowered $function
            for ( $parameter = 0; $parameter -lt [int]$target.parameterCount; ++$parameter ) {
                $type = Get-Type $lowered ([int]$target.parameterTypes[$parameter])
                if ( [int]$target.parameterAbis[$parameter] -ne 2 -or
                     $null -eq $type -or [int]$type.ownershipAbi -ne 2 ) {
                    continue
                }
                ++$borrowedOwnerCount
                $symbol = [uint64]$lowered.functionParameters[
                    [int]$function.firstParameter + $parameter]
                $loads = @($body | Where-Object {
                    $_.kindName -eq "load" -and [uint64]$_.targetSymbol -eq $symbol
                })
                $stores = @($body | Where-Object {
                    $_.kindName -eq "store" -and [uint64]$_.targetSymbol -eq $symbol
                })
                $releases = @($body | Where-Object {
                    $_.kindName -eq "release" -and [uint64]$_.targetSymbol -eq $symbol
                })
                if ([int]$type.cAbi -eq 25 -and $stores.Count -eq 0) {
                    if ($loads.Count -lt 1 -or $releases.Count -ne 0) {
                        throw 'Read-only string transport borrow acquired cleanup or lost its incoming load'
                    }
                    continue
                }
                if ( $loads.Count -lt 1 -or $stores.Count -lt 1 -or $releases.Count -lt 1 ) {
                    throw "XLIR managed BORROW parameter lost its lexical-owner prologue or terminal cleanup"
                }
            }
        }
        if ( $borrowedOwnerCount -lt 8 ) {
            throw "XLIR managed BORROW parameter ownership fixture no longer covers enough callable shapes"
        }
        $rebindFunctions = @($lowered.functions | Where-Object {
            if ( $_.kindName -ne "user" -or [int]$_.parameterCount -ne 1 ) { return $false }
            $symbol = [uint64]$lowered.functionParameters[[int]$_.firstParameter]
            $body = Get-FunctionBody $lowered $_
            return @($body | Where-Object {
                $_.kindName -eq "store" -and [uint64]$_.targetSymbol -eq $symbol
            }).Count -ge 2 -and @($body | Where-Object {
                $_.kindName -eq "release" -and [uint64]$_.targetSymbol -eq $symbol
            }).Count -ge 2
        })
        if ( $rebindFunctions.Count -ne 1 ) {
            throw "XLIR borrowed managed parameter rebind is not an explicit release/store ownership transition"
        }
    }

    $generated = Join-Path $outputDir "$($fixture.Name).collection-member.c"
    $exe = Join-Path $outputDir "$($fixture.Name).collection-member.exe"
    & $xl cgen $source --backend xlir --emit $generated
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR collection-member Cgen failed: $($fixture.Name)"
    }
    $generatedText = [System.IO.File]::ReadAllText(
        $generated, [System.Text.Encoding]::UTF8)
    if ( $fixture.Name -eq "cgen_typed_container_param_mutation" -and
         (! $generatedText.Contains("xrtValueSetRemove(") -or
          ! $generatedText.Contains("xrtValueSetHas(")) ) {
        throw "XLIR generated C lost the mechanical set member runtime operations"
    }
    Push-Location $Root
    try {
        $preincludedDefine = if ( $fixture.GenericHost ) {
            "-DXLIR_GENERATED_MODULE_PREINCLUDED"
        } else {
            ""
        }
        $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $preincludedDefine -include $generated $($fixture.Host) !XL6_CGEN_LINKS! -o $exe && $exe"
        & $env:ComSpec /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR collection-member strict-C runtime failed: $($fixture.Name)"
        }
    } finally {
        Pop-Location
    }
}

$typedMethodName = "xlir_typed_collection_methods"
$typedMethodSource = Join-Path $Root "test\cases\xlir_typed_collection_methods.xl"
if ( ! (Test-Path -LiteralPath $typedMethodSource -PathType Leaf) ) {
    throw "XLIR typed collection-method fixture is missing: $typedMethodSource"
}
$typedMethodTypedPath = Join-Path $outputDir "$typedMethodName.collection-member.typed.json"
$typedMethodLoweredPath = Join-Path $outputDir "$typedMethodName.collection-member.lowered.json"
$typedMethodTyped = Read-IrModule $typedMethodSource $false $typedMethodTypedPath
$typedMethodLowered = Read-IrModule $typedMethodSource $true $typedMethodLoweredPath
$typedMethods = @($typedMethodTyped.intrinsicCalls | Where-Object {
    $_.kindName -eq "typed-collection-member"
})
$expectedTypedMethods = @{
    "capacity:array" = 2
    "capacity:dict" = 1
    "capacity:set" = 1
    "clear:array" = 1
    "clear:dict" = 1
    "clear:list" = 1
    "clear:set" = 1
    "index-of:array" = 1
    "insert:array" = 2
    "is-disjoint:set" = 1
    "pop:array" = 5
    "reserve:array" = 1
    "reserve:dict" = 1
    "reserve:set" = 1
    "resize:array" = 5
    "reverse:array" = 1
    "swap:array" = 1
    "take:array" = 5
    "take:dict" = 1
    "take:list" = 1
    "trim-to:list" = 1
    "trim:array" = 1
    "trim:dict" = 1
    "trim:list" = 1
    "trim:set" = 1
}
foreach ( $expected in $expectedTypedMethods.GetEnumerator() ) {
    $parts = $expected.Key.Split(":")
    $matches = @($typedMethods | Where-Object {
        $_.typedCollectionOpName -eq $parts[0] -and
        $_.collectionAbiName -eq $parts[1]
    })
    if ( $matches.Count -ne [int]$expected.Value ) {
        throw "XLIR typed collection operation count changed: $($expected.Key)"
    }
}
if ( $typedMethods.Count -ne
     ($expectedTypedMethods.Values | Measure-Object -Sum).Sum ) {
    throw "XLIR typed collection-method gate found an unexpected operation"
}
foreach ( $intrinsic in $typedMethods ) {
    $receiverType = Get-Type $typedMethodTyped ([int]$intrinsic.receiverType)
    $resultType = Get-Type $typedMethodTyped ([int]$intrinsic.resultType)
    $operands = @($typedMethodTyped.intrinsicOperands |
        Select-Object -Skip ([int]$intrinsic.firstOperand) `
                      -First ([int]$intrinsic.operandCount))
    $expectedResultAbi = if (
        $intrinsic.typedCollectionOpName -in @("pop", "take") -and
        $null -ne $resultType -and [int]$resultType.ownershipAbi -eq 2
    ) { "owned" } else { "value" }
    $expectedErrorAbi = if ($intrinsic.typedCollectionOpName -eq 'resize') { 'may-raise' } else { 'no-raise' }
    if ( $intrinsic.admissionName -ne "verified" -or
         $intrinsic.errorAbiName -ne $expectedErrorAbi -or
         $intrinsic.resultAbiName -ne $expectedResultAbi -or
         $null -eq $receiverType -or
         $receiverType.collectionAbiName -ne $intrinsic.collectionAbiName -or
         $intrinsic.typedCollectionOpName -eq "invalid" ) {
        throw "XLIR typed collection method lost its exact operation/result ABI"
    }
    foreach ( $operand in $operands ) {
        $operandType = Get-Type $typedMethodTyped ([int]$operand.type)
        $expectedOperandAbi = if (
            $null -ne $operandType -and [int]$operandType.ownershipAbi -eq 2
        ) { "borrow" } else { "value" }
        if ( $operand.abiName -ne $expectedOperandAbi ) {
            throw "XLIR typed collection method operand ownership is not canonical"
        }
    }
    $runtime = @($typedMethodLowered.instructions | Where-Object {
        [int]$_.origin -eq [int]$intrinsic.origin -and
        $_.runtimeOpName -eq "xvalue-typed-collection-method"
    })
    if ( $runtime.Count -ne 1 -or
         ([int]$runtime[0].collectionDefaultAbi -gt 0) -ne ($intrinsic.typedCollectionOpName -eq 'resize') -or
         $runtime[0].typedCollectionOpName -ne
             $intrinsic.typedCollectionOpName -or
         $runtime[0].collectionAbiName -ne $intrinsic.collectionAbiName -or
         [int]$runtime[0].type -ne [int]$intrinsic.resultType -or
         [int]$runtime[0].operandCount -ne
             ([int]$intrinsic.operandCount + 1) ) {
        throw "XLIR typed collection method has no unique mechanical lowered operation"
    }
}

$typedMethodGenerated = Join-Path $outputDir "$typedMethodName.collection-member.c"
$typedMethodExe = Join-Path $outputDir "$typedMethodName.collection-member.exe"
& $xl cgen $typedMethodSource --backend xlir --emit $typedMethodGenerated
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed collection-method Cgen failed"
}
$typedMethodGeneratedText = [System.IO.File]::ReadAllText(
    $typedMethodGenerated, [System.Text.Encoding]::UTF8)
$requiredTypedMethodTokens = @(
    "xlir_xvalue_typed_array_pop_bool",
    "xlir_xvalue_typed_array_pop_i64",
    "xlir_xvalue_typed_array_pop_u64",
    "xlir_xvalue_typed_array_pop_f64",
    "xlir_xvalue_typed_array_pop_string",
    "xlir_xvalue_typed_collection_take_bool",
    "xlir_xvalue_typed_collection_take_i64",
    "xlir_xvalue_typed_collection_take_u64",
    "xlir_xvalue_typed_collection_take_f64",
    "xlir_xvalue_typed_collection_take_string",
    "xlir_xvalue_typed_collection_take_xvalue",
    "xl6ArrayResizeDefaultV1",
    "xlir_xvalue_typed_array_index_of",
    "xlir_xvalue_typed_array_reverse",
    "xrtValueArrayInsert",
    "xrtValueCapacity",
    "xrtValueIntMapTrim",
    "xrtValueSetIsDisjoint"
)
foreach ( $token in $requiredTypedMethodTokens ) {
    if ( ! $typedMethodGeneratedText.Contains($token) ) {
        throw "XLIR generated C lost typed collection operation: $token"
    }
}
Push-Location $Root
try {
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $typedMethodGenerated $defaultHost !XL6_CGEN_LINKS! -o $typedMethodExe && $typedMethodExe"
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed collection-method strict-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR collection-member intrinsic gate ok: canonical typed identities and operations, explicit lowered collection ABI, managed BORROW lexical owners, strict C, and runtimes."
exit 0
