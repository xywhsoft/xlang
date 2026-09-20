param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$moduleHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$classHost = Join-Path $Root "test\cases\cgen_class_host.c"
$fixtures = @(
    "cgen_member_function_class_return",
    "cgen_member_function_multi_return",
    "cgen_typed_container_for_in_returned"
)
foreach ( $path in @($xl, $moduleHost, $classHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR callable-field frame gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path `
    (Join-Path $Root "build\tmp"),(Join-Path $Root "build\test") | Out-Null

function Invoke-CheckedCmd([string]$command, [string]$failure)
{
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) { throw $failure }
}

Push-Location $Root
try {
    $hostObject = "build\tmp\xlir_callable_field_class_host.o"
    Invoke-CheckedCmd `
        "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c test\cases\cgen_class_host.c -o $hostObject" `
        "XLIR callable-field helper host strict-C compile failed"

    foreach ( $name in $fixtures ) {
        $source = Join-Path $Root "test\cases\$name.xl"
        $typedPath = Join-Path $Root "build\tmp\$name.callable-field.typed.json"
        $loweredPath = Join-Path $Root "build\tmp\$name.callable-field.lowered.json"
        $generatedPath = Join-Path $Root "build\test\$name.callable-field.xlir.c"
        $repeatPath = Join-Path $Root "build\tmp\$name.callable-field.repeat.c"
        $legacyPath = Join-Path $Root "build\test\$name.callable-field.legacy.c"
        if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
            throw "XLIR callable-field fixture is missing: $source"
        }

        & $xl ir $source --typed --format json --emit $typedPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR callable-field Typed dump failed: $name"
        }
        $typed = [System.IO.File]::ReadAllText(
            $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $typeById = @{}
        foreach ( $type in @($typed.types) ) {
            $typeById[[int]$type.id] = $type
        }

        & $xl ir $source --lowered --format json --emit $loweredPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR callable-field lowering failed: $name"
        }
        $lowered = [System.IO.File]::ReadAllText(
            $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $fieldOps = @($lowered.instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-object-set-literal-closure" -or
            $_.runtimeOpName -eq "xvalue-object-get-literal-closure"
        })
        if ( @($fieldOps | Where-Object {
                $_.runtimeOpName -eq "xvalue-object-set-literal-closure"
            }).Count -lt 1 -or
             @($fieldOps | Where-Object {
                $_.runtimeOpName -eq "xvalue-object-get-literal-closure"
            }).Count -lt 1 ) {
            throw "Lowered XLIR lost its typed callable field store/load frame: $name"
        }
        $signatureIds = @($fieldOps | ForEach-Object {
            [int]$_.calleeType
        } | Sort-Object -Unique)
        foreach ( $signatureId in $signatureIds ) {
            $signature = $typeById[$signatureId]
            if ( $null -eq $signature -or [int]$signature.kind -ne 4 -or
                 [int]$signature.cAbi -ne 31 ) {
                throw "Callable field operation lost its exact function TypeId: $name / t$signatureId"
            }
        }

        if ( $name -eq "cgen_member_function_class_return" ) {
            $hasVoid = @($signatureIds | Where-Object {
                @($typeById[$_].returns).Count -eq 0
            }).Count -gt 0
            $hasString = @($signatureIds | Where-Object {
                $signature = $typeById[$_]
                @($signature.params | Where-Object {
                    [int]$typeById[[int]$_].cAbi -eq 25
                }).Count -gt 0 -or
                @($signature.returns | Where-Object {
                    [int]$typeById[[int]$_].cAbi -eq 25
                }).Count -gt 0
            }).Count -gt 0
            $hasNominalXValue = @($signatureIds | Where-Object {
                $signature = $typeById[$_]
                @($signature.params + $signature.returns | Where-Object {
                    [int]$typeById[[int]$_].cAbi -eq 26 -and
                    [int]$typeById[[int]$_].nominalSymbol -ne 0
                }).Count -gt 0
            }).Count -gt 0
            if ( ! $hasVoid -or ! $hasString -or ! $hasNominalXValue ) {
                throw "Typed XLIR callable-field frame lost void, managed-string or nominal-xvalue slots"
            }
        } elseif ( $name -eq "cgen_member_function_multi_return" ) {
            $parents = @($signatureIds | Where-Object {
                $signature = $typeById[$_]
                @($signature.returns).Count -gt 1 -and
                @($signature.returns | Where-Object {
                    [int]$typeById[[int]$_].cAbi -eq 31
                }).Count -gt 0
            })
            if ( $parents.Count -ne 1 ) {
                throw "Typed XLIR lost the unique nested-closure multi-result field frame"
            }
            $parent = $typeById[[int]$parents[0]]
            $carrier = $typeById[[int]$parent.callableResultType]
            $nested = @($parent.returns | Where-Object {
                [int]$typeById[[int]$_].cAbi -eq 31
            })[0]
            if ( $null -eq $carrier -or [int]$carrier.cAbi -ne 32 -or
                 @($carrier.params).Count -ne @($parent.returns).Count ) {
                throw "Typed XLIR multi-result callable field lost its product carrier"
            }
            $releaseIndex = -1
            for ( $i = 0; $i -lt @($lowered.instructions).Count; ++$i ) {
                $instruction = $lowered.instructions[$i]
                if ( $instruction.kindName -eq "release" -and
                     [int]$instruction.type -eq [int]$carrier.id ) {
                    $releaseIndex = $i
                    break
                }
            }
            if ( $releaseIndex -le 0 -or
                 $lowered.instructions[$releaseIndex - 1].kindName -ne "store" -or
                 [uint64]$lowered.instructions[$releaseIndex - 1].targetSymbol -ne
                     [uint64]$lowered.instructions[$releaseIndex].targetSymbol ) {
                throw "Lowered XLIR ignored managed product has no adjacent temporary STORE/RELEASE sink"
            }

            $ownedReturns = @($lowered.instructions | Where-Object {
                $_.kindName -eq "return" -and $_.sourceLine -eq 31 -and
                $_.operandCount -eq 1
            } | Where-Object {
                $returned = [int]@($_.operands)[0]
                @($lowered.instructions | Where-Object {
                    $_.result -eq $returned -and $_.kindName -eq "product-make" -and
                    $_.operandCount -eq 2
                }).Count -eq 1
            })
            if ( $ownedReturns.Count -ne 1 ) {
                throw "Lowered XLIR multi-result local return lost its unique product owner"
            }
            $ownedReturn = $ownedReturns[0]
            $productValue = [int]@($ownedReturn.operands)[0]
            $productMake = @($lowered.instructions | Where-Object {
                $_.result -eq $productValue -and $_.kindName -eq "product-make"
            })[0]
            $localValue = [int]@($productMake.operands)[0]
            $localLoad = @($lowered.instructions | Where-Object {
                $_.result -eq $localValue -and $_.kindName -eq "load" -and
                $_.targetSymbol -ne 0
            })
            if ($localLoad.Count -ne 1) {
                throw "Lowered XLIR multi-result local owner is not rooted in one exact LOAD"
            }
            $localMoves = @($lowered.instructions | Where-Object {
                $_.index -gt $productMake.index -and
                $_.index -lt $ownedReturn.index -and
                $_.kindName -eq "move" -and
                [uint64]$_.targetSymbol -eq [uint64]$localLoad[0].targetSymbol -and
                $_.operandCount -eq 1 -and
                [int]@($_.operands)[0] -eq $localValue
            })
            $localRetains = @($lowered.instructions | Where-Object {
                $_.index -gt $localLoad[0].index -and
                $_.index -lt $productMake.index -and
                $_.kindName -eq "retain" -and
                $_.operandCount -eq 1 -and
                [int]@($_.operands)[0] -eq $localValue
            })
            if ( $localMoves.Count -ne 1 -or $localRetains.Count -ne 0 ) {
                throw "Lowered XLIR multi-result local return is not LOAD/PRODUCT_MAKE/MOVE ownership transfer"
            }
        } else {
            $hasTypedCollectionResult = @($signatureIds | Where-Object {
                $signature = $typeById[$_]
                @($signature.returns | Where-Object {
                    [int]$typeById[[int]$_].cAbi -eq 26 -and
                    [int]$typeById[[int]$_].collectionAbi -ne 0
                }).Count -gt 0
            }).Count -gt 0
            if ( ! $hasTypedCollectionResult ) {
                throw "Typed XLIR callable-field frame lost its typed collection xvalue result slot"
            }
        }

        & $xl cgen $source --backend xlir --emit $generatedPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR callable-field Cgen failed: $name"
        }
        & $xl cgen $source --backend xlir --emit $repeatPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR callable-field repeat Cgen failed: $name"
        }
        if ( ! [System.Linq.Enumerable]::SequenceEqual(
                [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
                [byte[]][System.IO.File]::ReadAllBytes($repeatPath)
            ) ) {
            throw "XLIR callable-field C output is not byte-for-byte deterministic: $name"
        }
        $generated = [System.IO.File]::ReadAllText(
            $generatedPath, [System.Text.Encoding]::UTF8)
        foreach ( $required in @(
            "xlir_xvalue_object_set_literal_closure(",
            "xlir_xvalue_object_get_literal_closure(",
            "xrtCallResultSetTake(", "xrtCallResultCount("
        ) ) {
            if ( ! $generated.Contains($required) ) {
                throw "XLIR callable-field generated bridge is incomplete: $name / $required"
            }
        }
        if ( $name -eq "cgen_member_function_class_return" ) {
            foreach ( $required in @(
                "xrtValueGetString(", "xrtStrDupN(",
                "xrtValueString(", "xrtCallResultUnit(callResult)"
            ) ) {
                if ( ! $generated.Contains($required) ) {
                    throw "XLIR callable-field string/void frame is incomplete: $required"
                }
            }
        } elseif ( $name -eq "cgen_member_function_multi_return" ) {
            $parentId = [int]$parent.id
            $nestedId = [int]$nested
            $carrierId = [int]$carrier.id
            foreach ( $required in @(
                "static bool xlir_xcall_entry_t$nestedId(void*,",
                "static bool xlir_xvalue_closure_entry_t$nestedId(void*,",
                "xlir_closure_to_xvalue_scalar(value.e1, xlir_xcall_entry_t$nestedId,",
                "xlir_xvalue_closure_entry_t$nestedId)",
                "static void xlir_product_drop_t$carrierId(",
                "xlir_product_drop_t$carrierId(&"
            ) ) {
                if ( ! $generated.Contains($required) ) {
                    throw "XLIR nested callable/product frame is incomplete: $required"
                }
            }
            if ( ! $generated.Contains("xlir_xcall_entry_t$parentId") ) {
                throw "XLIR parent multi-result callable adapter is missing"
            }
        }

        & $xl cgen $source --backend legacy --emit $legacyPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "Legacy callable-field differential Cgen failed: $name"
        }
        $relativeGenerated = "build\test\$name.callable-field.xlir.c"
        $relativeLegacy = "build\test\$name.callable-field.legacy.c"
        $xlirExe = "build\tmp\$name.callable-field.xlir.exe"
        $legacyExe = "build\tmp\$name.callable-field.legacy.exe"
        Invoke-CheckedCmd `
            "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $relativeGenerated test\cases\xlir_generated_module_host.c $hostObject !XL6_CGEN_LINKS! -o $xlirExe && $xlirExe" `
            "XLIR callable-field strict-C/runtime gate failed: $name"
        Invoke-CheckedCmd `
            "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $relativeLegacy test\cases\cgen_class_host.c !XL6_CGEN_LINKS! -o $legacyExe && $legacyExe" `
            "Legacy callable-field differential runtime failed: $name"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR callable-field frame ABI gate passed: void, scalar, managed-string, nominal/collection xvalue and recursive multi-result closure slots remain verifier-owned; ignored managed products use adjacent temporary STORE/RELEASE, C is byte-for-byte deterministic, and strict XLIR/legacy runtimes agree."
