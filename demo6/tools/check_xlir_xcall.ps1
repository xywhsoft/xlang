param(
    [string]$Root = "",
    [string]$SourceDriver = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

# Optional fresh-source compiler driver: do not overwrite the runtime DLL
# used by concurrent source audits. Keep the published-CLI lane as the default.
function Invoke-XlirLowered([string]$sourcePath, [string]$jsonPath) {
    if ($SourceDriver -ne "") {
        & $SourceDriver $sourcePath "$jsonPath.c" $jsonPath
    } else { & $xl ir $sourcePath --lowered --format json --emit $jsonPath }
}
function Invoke-XlirC([string]$sourcePath, [string]$cPath) {
    if ($SourceDriver -ne "") {
        & $SourceDriver $sourcePath $cPath "$cPath.json"
    } else { & $xl cgen $sourcePath --backend xlir --emit $cPath }
}
function Invoke-XlirStrict([string]$cPath, [string]$hostPath, [string]$exePath, [bool]$preincluded) {
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-O0',
        '-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-I.','-Ibuild/test')
    if ($preincluded) { $flags += '-DXLIR_GENERATED_MODULE_PREINCLUDED' }
    & gcc @flags -include $cPath $hostPath release/xl.dll -o $exePath
    if ($LASTEXITCODE -ne 0) { throw "XCall strict C compilation failed: $cPath" }
    & $exePath
    if ($LASTEXITCODE -ne 0) { throw "XCall strict runtime failed: $exePath" }
}
if ($SourceDriver -ne "") { $SourceDriver = (Resolve-Path -LiteralPath $SourceDriver).Path }
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_extern_function_value.xl"
$runtimeHost = Join-Path $Root "test\cases\xlir_xcall_i64_host.c"
$scalarSource = Join-Path $Root "test\cases\xlir_xcall_scalar_effect.xl"
$scalarHost = Join-Path $Root "test\cases\xlir_xcall_scalar_host.c"
$nullableSource = Join-Path $Root "test\cases\xlir_xcall_nullable_closure.xl"
$classSource = Join-Path $Root "test\cases\cgen_xcall_class.xl"
$pointerSource = Join-Path $Root "test\cases\xlir_xcall_raw_pointer.xl"
$moduleHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\test"
$json = Join-Path $outputDir "xlir_xcall.lowered.json"
$generated = Join-Path $outputDir "xlir_xcall.generated.c"
$exe = Join-Path $outputDir "xlir_xcall.exe"
$scalarJson = Join-Path $outputDir "xlir_xcall_scalar.lowered.json"
$scalarGenerated = Join-Path $outputDir "xlir_xcall_scalar.generated.c"
$scalarExe = Join-Path $outputDir "xlir_xcall_scalar.exe"
$nullableJson = Join-Path $outputDir "xlir_xcall_nullable.lowered.json"
$nullableGenerated = Join-Path $outputDir "xlir_xcall_nullable.generated.c"
$nullableExe = Join-Path $outputDir "xlir_xcall_nullable.exe"
$classJson = Join-Path $outputDir "xlir_xcall_class.lowered.json"
$classGenerated = Join-Path $outputDir "xlir_xcall_class.generated.c"
$classExe = Join-Path $outputDir "xlir_xcall_class.exe"
$pointerJson = Join-Path $outputDir "xlir_xcall_pointer.lowered.json"
$pointerGenerated = Join-Path $outputDir "xlir_xcall_pointer.generated.c"
$pointerExe = Join-Path $outputDir "xlir_xcall_pointer.exe"

foreach ( $path in @(
    $xl, $source, $runtimeHost, $scalarSource, $scalarHost,
    $nullableSource, $classSource, $pointerSource, $moduleHost
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR xcall gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$runtimeDll = Join-Path $outputDir 'xl.dll'
if (!(Test-Path -LiteralPath $runtimeDll) -or
    (Get-FileHash (Join-Path $Root 'release/xl.dll')).Hash -ne (Get-FileHash $runtimeDll).Hash) {
    Copy-Item -LiteralPath (Join-Path $Root 'release/xl.dll') -Destination $runtimeDll -Force
}

Invoke-XlirLowered $source $json
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR xcall lowering failed"
}
$module = [System.IO.File]::ReadAllText(
    $json, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$boxed = @($module.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-callable-from-closure"
})
$dynamic = @($module.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-dynamic-call"
})
if ( $boxed.Count -ne 2 -or $dynamic.Count -ne 2 ) {
    throw "XLIR xcall fixture has no explicit closure boxing and dynamic-call operations"
}
foreach ( $instruction in $boxed ) {
    if ( $instruction.calleeType -eq 0 -or $instruction.operandCount -ne 1 ) {
        throw "XLIR closure-to-callable operation lost its exact signature ABI"
    }
}
foreach ( $instruction in $dynamic ) {
    if ( $instruction.calleeType -ne 0 -or $instruction.operandCount -ne 3 ) {
        throw "XLIR dynamic call lost its borrowed callable or consumed argument ABI"
    }
}
foreach ( $instruction in @($boxed + $dynamic) ) {
    $test = @($module.instructions | Where-Object {
        $_.id -eq ([int]$instruction.id + 1) -and
        $_.kindName -eq "error-test" -and $_.targetSymbol -eq 0 -and
        $_.operandCount -eq 1 -and $_.operands[0] -eq $instruction.result
    })
    if ( $test.Count -ne 1 ) {
        throw "XLIR runtime callable operation is not immediately guarded by ERROR_TEST"
    }
}
if ( @($module.instructions | Where-Object {
         $_.runtimeOpName -eq "xvalue-i64"
     }).Count -lt 4 -or
     @($module.functions | Where-Object {
         $_.kind -eq 1 -and $_.errorAbi -eq "may-raise"
     }).Count -ne 1 ) {
    throw "XLIR dynamic call has no explicit argument boxing or escaping effect ABI"
}

Invoke-XlirC $source $generated
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR xcall Cgen failed"
}
$generatedText = [System.IO.File]::ReadAllText(
    $generated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtCallableCreate(",
    "xrtValueCallableTake(",
    "xrtValueInvoke(",
    "xrtCallResultSetTake(",
    "xlir_closure_to_xvalue_scalar(",
    "xlir_xcall_entry_t",
    "xlir_xvalue_dynamic_call_owned("
) ) {
    if ( ! $generatedText.Contains($required) ) {
        throw "XLIR generated C lost public runtime callable ABI: $required"
    }
}
foreach ( $forbidden in @(
    "XL6_Function",
    "xl6XrtCallableCreate",
    "xl6XrtValueInvoke",
    "xl6CallResult"
) ) {
    if ( $generatedText.Contains($forbidden) ) {
        throw "XLIR generated C regressed to a legacy callable compatibility ABI: $forbidden"
    }
}

Push-Location $Root
try {
    Invoke-XlirStrict $generated $runtimeHost $exe $false
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR xcall strict-C runtime failed"
    }
} finally {
    Pop-Location
}

Invoke-XlirLowered $scalarSource $scalarJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR scalar xcall lowering failed"
}
$scalarModule = [System.IO.File]::ReadAllText(
    $scalarJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$scalarBoxed = @($scalarModule.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-callable-from-closure"
})
$scalarDynamic = @($scalarModule.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-dynamic-call"
})
if ( $scalarBoxed.Count -ne 3 -or $scalarDynamic.Count -ne 3 -or
     @($scalarBoxed | Select-Object -ExpandProperty calleeType -Unique).Count -ne 3 -or
     ((@($scalarDynamic | Select-Object -ExpandProperty operandCount | Sort-Object) -join ",") -ne "1,2,4") ) {
    throw "XLIR scalar xcall fixture has no distinct zero/bool and mixed scalar callable ABIs"
}
foreach ( $instruction in @($scalarBoxed + $scalarDynamic) ) {
    $test = @($scalarModule.instructions | Where-Object {
        $_.id -eq ([int]$instruction.id + 1) -and
        $_.kindName -eq "error-test" -and $_.targetSymbol -eq 0 -and
        $_.operandCount -eq 1 -and $_.operands[0] -eq $instruction.result
    })
    if ( $test.Count -ne 1 ) {
        throw "XLIR scalar runtime callable operation is not immediately guarded by ERROR_TEST"
    }
}
foreach ( $boxing in @("xvalue-bool", "xvalue-i64", "xvalue-f64") ) {
    if ( @($scalarModule.instructions | Where-Object {
             $_.runtimeOpName -eq $boxing
         }).Count -eq 0 ) {
        throw "XLIR scalar dynamic call has no explicit argument boxing ABI: $boxing"
    }
}
if ( @($scalarModule.functions | Where-Object {
         $_.kind -eq 4 -and $_.errorAbi -eq "may-raise"
     }).Count -ne 1 ) {
    throw "XLIR scalar xcall lost its may-raise named-adapter effect ABI"
}

Invoke-XlirC $scalarSource $scalarGenerated
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR scalar xcall Cgen failed"
}
$scalarGeneratedText = [System.IO.File]::ReadAllText(
    $scalarGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_closure_to_xvalue_scalar(",
    "xrtValueGetBool(", "xrtValueGetInt(", "xrtValueGetFloat(",
    "xrtValueBool(", "xrtValueInt(", "xrtValueFloat(",
    "xrtTakeError(", "xrtErrorCause(", "xrtErrorRef(",
    "xl6ForeignCallBegin(&invocation)", "xl6ErrorScopeCaptureFailure(&invocation)",
    "xl6ForeignCallEnd(&invocation)"
) ) {
    if ( ! $scalarGeneratedText.Contains($required) ) {
        throw "XLIR generated scalar callable adapter is incomplete: $required"
    }
}
$scalarAdapterTypeIds = @([regex]::Matches(
    $scalarGeneratedText, 'static bool xlir_xcall_entry_t([0-9]+)\(') |
    ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
if ( $scalarAdapterTypeIds.Count -ne 3 ) {
    throw "XLIR Cgen did not deduplicate scalar callable adapters by verified TypeId"
}
foreach ( $forbidden in @(
    "XL6_Function", "xl6XrtCallableCreate", "xl6XrtValueInvoke", "xl6CallResult"
) ) {
    if ( $scalarGeneratedText.Contains($forbidden) ) {
        throw "XLIR scalar generated C regressed to a legacy callable ABI: $forbidden"
    }
}

Push-Location $Root
try {
    Invoke-XlirStrict $scalarGenerated $scalarHost $scalarExe $false
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR scalar xcall strict-C runtime failed"
    }
} finally {
    Pop-Location
}

Invoke-XlirLowered $nullableSource $nullableJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR narrowed nullable xcall lowering failed"
}
$nullableModule = [System.IO.File]::ReadAllText(
    $nullableJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$nullableIntrinsic = @($nullableModule.intrinsicCalls | Where-Object {
    $_.kindName -eq "xcall" -and $_.admissionName -eq "verified"
})
$nullableBoxed = @($nullableModule.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-callable-from-closure"
})
if ( $nullableIntrinsic.Count -ne 1 -or $nullableBoxed.Count -ne 1 ) {
    throw "XLIR narrowed nullable xcall has no unique verified intrinsic and callable construction"
}
$nullableBinding = $nullableModule.intrinsicOperands[
    [int]$nullableIntrinsic[0].firstOperand]
$nullableStorageType = @($nullableModule.types | Where-Object {
    $_.id -eq $nullableBinding.type
})
$narrowedValue = @($nullableModule.instructions | Where-Object {
    $_.result -eq [int]$nullableBoxed[0].operands[0]
})
# Nullable closure narrowing now has an explicit retained evaluation owner.
# Check the exact LOAD <- STORE <- RETAIN <- UNWRAP chain; do not mistake a
# borrowed load from that owner for a missing nullable conversion.
if ($narrowedValue.Count -eq 1 -and $narrowedValue[0].kindName -eq 'load') {
    $anchorLoad = $narrowedValue[0]
    $anchorStores = @($nullableModule.instructions | Where-Object {
        $_.kindName -eq 'store' -and $_.targetSymbol -eq $anchorLoad.targetSymbol -and $_.id -lt $anchorLoad.id
    })
    if ($anchorStores.Count -ne 1 -or $anchorStores[0].type -ne $anchorLoad.type -or
        $anchorStores[0].operandCount -ne 1) { throw 'Nullable callable borrow has no unique typed evaluation owner' }
    $retained = @($nullableModule.instructions | Where-Object { $_.result -eq $anchorStores[0].operands[0] })
    if ($retained.Count -ne 1 -or $retained[0].kindName -ne 'retain' -or
        $retained[0].type -ne $anchorLoad.type -or $retained[0].operandCount -ne 1 -or
        $retained[0].id -ge $anchorStores[0].id) { throw 'Nullable callable evaluation owner has no retained input' }
    $narrowedValue = @($nullableModule.instructions | Where-Object {
        $_.result -eq $retained[0].operands[0] -and $_.id -lt $retained[0].id
    })
}
if ( $nullableBinding.abiName -ne "borrow" -or
     $nullableStorageType.Count -ne 1 -or
     $nullableStorageType[0].kind -ne 8 -or
     @($nullableStorageType[0].params).Count -ne 1 -or
     $nullableStorageType[0].params[0] -ne $nullableBoxed[0].calleeType -or
     $nullableBoxed[0].callableErasureAbiName -ne "fixed" -or
     $narrowedValue.Count -ne 1 -or
     $narrowedValue[0].kindName -ne "nullable-unwrap" -or
     $narrowedValue[0].type -ne $nullableBoxed[0].calleeType ) {
    throw "XLIR narrowed nullable xcall lost its storage, unwrap, signature or erasure ABI"
}
$nullableTest = @($nullableModule.instructions | Where-Object {
    $_.id -eq ([int]$nullableBoxed[0].id + 1) -and
    $_.kindName -eq "error-test" -and $_.operandCount -eq 1 -and
    $_.operands[0] -eq $nullableBoxed[0].result
})
if ( $nullableTest.Count -ne 1 ) {
    throw "XLIR narrowed nullable xcall callable construction is not immediately guarded by ERROR_TEST"
}

Invoke-XlirC $nullableSource $nullableGenerated
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR narrowed nullable xcall Cgen failed"
}
$nullableGeneratedText = [System.IO.File]::ReadAllText(
    $nullableGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_closure_to_xvalue_scalar(", "xlir_xcall_entry_t",
    "xlir_xvalue_dynamic_call_owned("
) ) {
    if ( ! $nullableGeneratedText.Contains($required) ) {
        throw "XLIR narrowed nullable xcall generated C is incomplete: $required"
    }
}
Push-Location $Root
try {
    Invoke-XlirStrict $nullableGenerated $moduleHost $nullableExe $true
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR narrowed nullable xcall strict-C runtime failed"
    }
} finally {
    Pop-Location
}

Invoke-XlirLowered $classSource $classJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR nominal-class xcall lowering failed"
}
$classModule = [System.IO.File]::ReadAllText(
    $classJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$classIntrinsics = @($classModule.intrinsicCalls | Where-Object {
    $_.kindName -eq "xcall"
})
$counterTypes = @($classModule.types | Where-Object {
    $_.kind -eq 2 -and $_.name -eq "Counter" -and
    $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 2 -and
    $_.nominalSymbol -ne 0 -and $_.typeValueId -ne "0x0000000000000000"
})
$classBoxed = @($classModule.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-callable-from-closure"
})
$classDynamic = @($classModule.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-dynamic-call"
})
if ( $classIntrinsics.Count -ne 2 -or
     @($classIntrinsics | Where-Object {
         $_.admissionName -eq "verified" -and
         $_.resultAbiName -eq "owned" -and $_.errorAbiName -eq "may-raise"
     }).Count -ne 2 -or
     $counterTypes.Count -ne 1 -or
     $classBoxed.Count -ne 2 -or $classDynamic.Count -ne 2 ) {
    throw "XLIR nominal-class xcall lost its verified intrinsic, class identity or callable operations"
}
$counterType = $counterTypes[0]
$classFunctionTypes = @($classBoxed | ForEach-Object {
    $calleeType = $_.calleeType
    @($classModule.types | Where-Object { $_.id -eq $calleeType })
})
if ( $classFunctionTypes.Count -ne 2 -or
     @($classFunctionTypes | Where-Object {
         $_.kind -eq 4 -and $_.callableErasureAbiName -eq "fixed" -and
         @($_.params).Count -eq 1 -and $_.params[0] -eq $counterType.id -and
         @($_.parameterAbis).Count -eq 1 -and $_.parameterAbis[0] -eq "borrow"
     }).Count -ne 2 -or
     @($classFunctionTypes | Select-Object -ExpandProperty callableResultType -Unique).Count -ne 2 -or
     ! (@($classFunctionTypes | Select-Object -ExpandProperty callableResultType) -contains $counterType.id) ) {
    throw "XLIR nominal-class xcall lost its borrowed Counter parameter or distinct scalar/Counter result ABI"
}
foreach ( $instruction in @($classBoxed + $classDynamic) ) {
    $test = @($classModule.instructions | Where-Object {
        $_.id -eq ([int]$instruction.id + 1) -and
        $_.kindName -eq "error-test" -and $_.operandCount -eq 1 -and
        $_.operands[0] -eq $instruction.result
    })
    if ( $test.Count -ne 1 ) {
        throw "XLIR nominal-class callable operation is not immediately guarded by ERROR_TEST"
    }
}
foreach ( $instruction in $classDynamic ) {
    if ( $instruction.operandCount -ne 2 ) {
        throw "XLIR nominal-class dynamic call lost its callable/argument operand ABI"
    }
    $argument = [int]$instruction.operands[1]
    # The argument is protected in an evaluation-owner slot until the dynamic
    # call consumes it. Check the full ownership transaction, not a direct
    # VALUE operand that would omit the partial-evaluation lifetime boundary.
    $ownerLoad = @($classModule.instructions | Where-Object {
        $_.result -eq $argument -and $_.kindName -eq "load" -and
        $_.type -eq $classIntrinsics[0].resultType -and
        $_.id -eq ([int]$instruction.id - 2)
    })
    if ($ownerLoad.Count -ne 1) {
        throw "XLIR nominal-class dynamic argument has no protected owner load"
    }
    $ownerSymbol = $ownerLoad[0].targetSymbol
    $ownerStore = @($classModule.instructions | Where-Object {
        $_.kindName -eq "store" -and $_.targetSymbol -eq $ownerSymbol -and
        $_.type -eq $classIntrinsics[0].resultType -and $_.operandCount -eq 1 -and
        $_.id -eq ([int]$instruction.id - 3)
    })
    $ownerMove = @($classModule.instructions | Where-Object {
        $_.kindName -eq "move" -and $_.targetSymbol -eq $ownerSymbol -and
        $_.type -eq $classIntrinsics[0].resultType -and $_.operandCount -eq 1 -and
        $_.operands[0] -eq $argument -and $_.id -eq ([int]$instruction.id - 1)
    })
    if ($ownerStore.Count -ne 1 -or $ownerMove.Count -ne 1) {
        throw "XLIR nominal-class dynamic argument has no exact STORE/LOAD/MOVE commit"
    }
    $argument = [int]$ownerStore[0].operands[0]
    $boxedArgument = @($classModule.instructions | Where-Object {
        $_.result -eq $argument -and $_.kindName -eq "value" -and
        $_.type -eq $classIntrinsics[0].resultType -and $_.operandCount -eq 1
    })
    $retained = @()
    if ( $boxedArgument.Count -eq 1 ) {
        $retainedValue = [int]$boxedArgument[0].operands[0]
        $retained = @($classModule.instructions | Where-Object {
            $_.result -eq $retainedValue -and $_.kindName -eq "runtime-call" -and
            $_.runtimeOpName -eq "xvalue-retain" -and $_.type -eq $counterType.id
        })
    }
    if ( $boxedArgument.Count -ne 1 -or $retained.Count -ne 1 ) {
        throw "XLIR nominal-class dynamic call did not consume an explicitly boxed, retained Counter argument"
    }
}
$integerProjections = @($classModule.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and $_.runtimeOpName -eq "integer-convert-checked"
})
$classProjections = @($classModule.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-nominal-cast-checked" -and
    $_.type -eq $counterType.id
})
if ( $integerProjections.Count -ne 1 -or $classProjections.Count -ne 1 ) {
    throw "XLIR nominal-class xcall lost its explicit scalar or nominal result projection"
}
foreach ( $instruction in @($integerProjections + $classProjections) ) {
    $test = @($classModule.instructions | Where-Object {
        $_.id -eq ([int]$instruction.id + 1) -and
        $_.kindName -eq "error-test" -and $_.operandCount -eq 1 -and
        $_.operands[0] -eq $instruction.result
    })
    if ( $test.Count -ne 1 ) {
        throw "XLIR nominal-class xcall result projection is not immediately guarded by ERROR_TEST"
    }
}

Invoke-XlirC $classSource $classGenerated
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR nominal-class xcall Cgen failed"
}
$classGeneratedText = [System.IO.File]::ReadAllText(
    $classGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtCallableCreate(", "xrtValueCallableTake(", "xrtValueInvoke(",
    "xlir_xvalue_dynamic_call_owned(", "xlir_nominal_set_1_matches(",
    "dynamic value does not match requested nominal target"
) ) {
    if ( ! $classGeneratedText.Contains($required) ) {
        throw "XLIR nominal-class xcall generated C is incomplete: $required"
    }
}
$classAdapterTypeIds = @([regex]::Matches(
    $classGeneratedText, 'static bool xlir_xcall_entry_t([0-9]+)\(') |
    ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
if ( $classAdapterTypeIds.Count -ne 2 ) {
    throw "XLIR Cgen did not preserve distinct scalar/class callable adapters by verified TypeId"
}

Push-Location $Root
try {
    Invoke-XlirStrict $classGenerated $moduleHost $classExe $true
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR nominal-class xcall strict-C runtime failed"
    }
    & $xl run $classSource --backend legacy
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Legacy nominal-class xcall differential runtime failed"
    }
} finally {
    Pop-Location
}

Invoke-XlirLowered $pointerSource $pointerJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR raw-pointer xcall lowering failed"
}
$pointerModule = [System.IO.File]::ReadAllText(
    $pointerJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$pointerType = @($pointerModule.types | Where-Object {
    $_.name -eq "point" -and $_.cAbi -eq 28 -and
    $_.ownershipAbi -eq 1 -and $_.nullAbi -eq 1
})
$pointerBoxed = @($pointerModule.instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-callable-from-closure"
})
if ( $pointerType.Count -ne 1 -or $pointerBoxed.Count -ne 2 ) {
    throw "XLIR raw-pointer xcall lost its point representation or callable construction"
}
$pointerSignatures = @($pointerBoxed | ForEach-Object {
    $calleeType = $_.calleeType
    @($pointerModule.types | Where-Object { $_.id -eq $calleeType })
})
if ( $pointerSignatures.Count -ne 2 -or
     @($pointerSignatures | Where-Object {
         $_.kind -eq 4 -and $_.callableErasureAbiName -eq "fixed" -and
         @($_.params).Count -eq 1 -and $_.params[0] -eq $pointerType[0].id -and
         @($_.parameterAbis).Count -eq 1 -and $_.parameterAbis[0] -eq "value"
     }).Count -ne 2 -or
     ! (@($pointerSignatures | Select-Object -ExpandProperty callableResultType) -contains $pointerType[0].id) ) {
    throw "XLIR raw-pointer xcall lost its point value parameter or point result ABI"
}
if ( @($pointerModule.instructions | Where-Object {
         $_.runtimeOpName -eq "xvalue-raw-pointer-borrow"
     }).Count -lt 2 -or
     @($pointerModule.instructions | Where-Object {
         $_.castAbiName -eq "xvalue-to-raw-pointer-checked"
     }).Count -ne 1 ) {
    throw "XLIR raw-pointer xcall lost explicit pointer boxing or checked result projection"
}

Invoke-XlirC $pointerSource $pointerGenerated
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR raw-pointer xcall Cgen failed"
}
$pointerGeneratedText = [System.IO.File]::ReadAllText(
    $pointerGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtValuePointer(", "xrtValueGetPointer(",
    "xlir_closure_to_xvalue_scalar(", "xlir_xcall_entry_t"
) ) {
    if ( ! $pointerGeneratedText.Contains($required) ) {
        throw "XLIR raw-pointer xcall generated C is incomplete: $required"
    }
}
Push-Location $Root
try {
    Invoke-XlirStrict $pointerGenerated $moduleHost $pointerExe $true
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR raw-pointer xcall strict-C runtime failed"
    }
    & $xl run $pointerSource --backend legacy
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Legacy raw-pointer xcall differential runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR xcall gate ok: TypeId-deduplicated core bool/int/float, raw-pointer and nominal-class callable adapters, narrowed nullable closure storage, explicit closure boxing/dynamic-call IR, retained-and-consumed class arguments, scalar/pointer/nominal projections, projected callable error causes, immediate error CFG, public XRT APIs, strict C, XLIR runtimes and legacy differentials; uint64 callable ABI is covered by check_xlir_u64_abi.ps1."
exit 0
