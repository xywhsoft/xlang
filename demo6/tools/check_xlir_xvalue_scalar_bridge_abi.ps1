param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\hir_xvalue_scalar_bridge_matrix.xl"
$runtimeHost = Join-Path $Root "test\cases\hir_xvalue_scalar_bridge_matrix_host.c"
$outputDir = Join-Path $Root "build\test"
$typedJson = Join-Path $outputDir "xlir_xvalue_scalar_bridge.typed.json"
$loweredJson = Join-Path $outputDir "xlir_xvalue_scalar_bridge.lowered.json"
$generatedC = Join-Path $outputDir "xlir_xvalue_scalar_bridge.c"
$generatedCSecond = Join-Path $outputDir "xlir_xvalue_scalar_bridge.second.c"
$xlirExe = Join-Path $outputDir "xlir_xvalue_scalar_bridge.exe"

foreach ( $path in @($xl, $source, $runtimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR xvalue scalar-bridge gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR xvalue scalar-bridge Typed IR construction failed"
}
& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR xvalue scalar-bridge lowering failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$refinedName = @($typed.nodes | Where-Object {
    $_.kindName -eq "name" -and $_.line -eq 19 -and
    $_.targetSymbol -ne 0 -and $_.type -ne $_.storageType
})
if ( $refinedName.Count -ne 1 ) {
    throw "Typed XLIR has no unique flow-refined string name over open-xvalue storage"
}
$stringType = @($typed.types | Where-Object { $_.id -eq $refinedName[0].type })
$dynamicType = @($typed.types | Where-Object { $_.id -eq $refinedName[0].storageType })
if ( $stringType.Count -ne 1 -or $dynamicType.Count -ne 1 -or
     $stringType[0].name -ne "string" -or $stringType[0].cAbi -ne 25 -or
     $stringType[0].ownershipAbi -ne 2 -or $stringType[0].nullAbi -ne 1 -or
     $dynamicType[0].name -ne "var" -or $dynamicType[0].cAbi -ne 26 -or
     $dynamicType[0].ownershipAbi -ne 2 -or $dynamicType[0].nullAbi -ne 2 ) {
    throw "Typed XLIR refined string and physical open-xvalue types have no exact ABI"
}

$returnOps = @($lowered.instructions | Where-Object { $_.sourceLine -eq 6 })
$returnBox = @($returnOps | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-string-copy"
})
$return = @($returnOps | Where-Object { $_.kindName -eq "return" })
if ( $returnBox.Count -ne 1 -or $return.Count -ne 1 -or
     $returnBox[0].type -ne $dynamicType[0].id -or
     $returnBox[0].operandCount -ne 1 -or
     $return[0].operandCount -ne 1 -or
     $return[0].operands[0] -ne $returnBox[0].result -or
     @($returnOps | Where-Object { $_.runtimeOpName -eq "string-copy" }).Count -ne 0 ) {
    throw "Lowered XLIR managed-string return has no single owned xvalue conversion ABI"
}

$typeIs = @($typed.nodes | Where-Object {
    $_.sourceKindName -eq "type_is" -and $_.line -eq 18
})
if ( $typeIs.Count -ne 1 ) {
    throw "Typed XLIR has no unique exact non-class type-is expression"
}
$typeIsOps = @($lowered.instructions | Where-Object {
    $_.origin -eq $typeIs[0].id
})
$typeId = @($typeIsOps | Where-Object {
    $_.kindName -eq "runtime-call" -and $_.runtimeOpName -eq "xvalue-type-id"
})
$typeConst = @($typeIsOps | Where-Object {
    $_.kindName -eq "const" -and $_.hasImmediate -eq 1
})
$typeEq = @($typeIsOps | Where-Object {
    $_.kindName -eq "binary" -and $_.opcodeName -eq "eq"
})
if ( $typeId.Count -ne 1 -or $typeConst.Count -ne 1 -or $typeEq.Count -ne 1 -or
     $typeId[0].type -ne $typeConst[0].type -or
     $typeEq[0].operandCount -ne 2 -or
     $typeEq[0].operands[0] -ne $typeId[0].result -or
     $typeEq[0].operands[1] -ne $typeConst[0].result ) {
    throw "Lowered XLIR non-class type-is is not the exact xvalue-type-id/TypeId/EQ composition"
}

$projection = @($lowered.instructions | Where-Object {
    $_.origin -eq $refinedName[0].id -and
    $_.runtimeOpName -eq "xvalue-project-string-copy"
})
if ( $projection.Count -ne 1 -or $projection[0].operandCount -ne 1 -or
     $projection[0].type -ne $stringType[0].id -or
     $projection[0].payloadType -ne $stringType[0].id -or
     $projection[0].runtimeIdentityType -ne 0 -or
     $projection[0].calleeType -ne 0 -or
     @($lowered.instructions | Where-Object {
         $_.sourceLine -eq 19 -and
         $_.runtimeOpName -in @("string-copy", "xvalue-to-string")
     }).Count -ne 0 ) {
    throw "Lowered XLIR refined string has no single exact owned projection ABI"
}

$checked = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "integer-convert-checked"
})
if ( $checked.Count -ne 7 -or @($checked | Where-Object {
        $_.kindName -ne "runtime-call" -or $_.operandCount -ne 1 -or
        $_.integerTargetName -ne "i64" -or
        $_.integerSourceAbiName -ne "xvalue"
    }).Count -ne 0 ) {
    throw "Lowered XLIR checked xvalue-to-i64 bridge matrix is not exact"
}

$createTarget = @($lowered.callTargets | Where-Object { $_.symbol -eq 7 })
$constructors = @($typed.nodes | Where-Object {
    $_.callKindName -eq "constructor" -and $_.targetSymbol -eq 7
})
if ( $createTarget.Count -ne 1 -or $constructors.Count -ne 2 -or
     $createTarget[0].kind -ne 1 -or
     $createTarget[0].errorAbiName -ne "may-raise" -or
     $createTarget[0].returnType -ne 0 -or
     $createTarget[0].parameterCount -ne 2 -or
     $createTarget[0].parameterAbis[0] -ne 2 -or
     $createTarget[0].parameterAbis[1] -ne 2 ) {
    throw "XLIR create target has no exact borrowed receiver, dynamic input and may-raise ABI"
}
foreach ( $constructor in $constructors ) {
    $ops = @($lowered.instructions | Where-Object { $_.origin -eq $constructor.id })
    $call = @($ops | Where-Object {
        $_.kindName -eq "call" -and $_.targetSymbol -eq 7
    })
    $test = @($ops | Where-Object {
        $_.kindName -eq "error-test" -and $_.targetSymbol -eq 7
    })
    if ( $call.Count -ne 1 -or $test.Count -ne 1 -or
         $call[0].operandCount -ne 2 -or $call[0].result -eq 0 -or
         $test[0].operandCount -ne 1 -or
         $test[0].operands[0] -ne $call[0].result -or
         $test[0].trueTarget -eq 0 -or $test[0].falseTarget -eq 0 -or
         $test[0].trueTarget -eq $test[0].falseTarget ) {
        throw "XLIR constructor call has no explicit may-raise carrier and branch CFG"
    }
    $ownerStores = @($ops | Where-Object {
        if ( $_.kindName -ne "store" -or $_.type -ne $constructor.storageType -or
             $_.targetSymbol -eq 0 -or $_.id -ge $call[0].id ) { return $false }
        $symbol = $_.targetSymbol
        return @($ops | Where-Object {
            $_.kindName -eq "release" -and $_.targetSymbol -eq $symbol -and
            $_.id -gt $test[0].id
        }).Count -eq 1 -and @($ops | Where-Object {
            $_.kindName -eq "move" -and $_.targetSymbol -eq $symbol -and
            $_.id -gt $test[0].id
        }).Count -eq 1
    })
    if ( $ownerStores.Count -ne 1 ) {
        throw "XLIR may-raise constructor does not anchor, release and move one exact object owner"
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR xvalue scalar-bridge C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR xvalue scalar-bridge repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR xvalue scalar-bridge generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_project_string_copy(",
    "xlir_xvalue_type_id(",
    "xlir_xvalue_to_i64_checked(",
    "xlir_result_void",
    "if (r"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR scalar-bridge C is missing mechanical IR emission: $required"
    }
}
foreach ( $forbidden in @(
    "hir_xvalue_scalar_bridge_matrix.xl", "sourceKind", "callKind",
    "intrinsic", "flow-refined"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR Cgen reconstructed scalar-bridge source semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\test\xlir_xvalue_scalar_bridge.c test\cases\hir_xvalue_scalar_bridge_matrix_host.c !XL6_CGEN_LINKS! -o build\test\xlir_xvalue_scalar_bridge.exe && build\test\xlir_xvalue_scalar_bridge.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR xvalue scalar-bridge strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR xvalue scalar-bridge gate passed: owned return conversion, exact TypeId type-is, refined string projection, checked integer bridges, may-raise constructor cleanup CFG, deterministic strict C, and failure catch runtime."
exit 0
