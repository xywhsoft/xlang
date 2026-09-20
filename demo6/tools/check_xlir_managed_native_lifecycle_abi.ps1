param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_managed_native_explicit_destroy.xl"
$hostSource = Join-Path $Root "test\cases\xlir_managed_native_explicit_destroy_host.c"
$supportSource = Join-Path $Root "test\cases\xlir_managed_native_explicit_destroy_support.c"
$outputDir = Join-Path $Root "build\test"
$loweredJson = Join-Path $outputDir "xlir_managed_native_lifecycle.lowered.json"
$generatedC = Join-Path $outputDir "xlir_managed_native_explicit_destroy_generated.c"
$generatedCSecond = Join-Path $outputDir "xlir_managed_native_lifecycle.second.c"
$exe = Join-Path $outputDir "xlir_managed_native_lifecycle.exe"
$artifactExe = Join-Path $outputDir "xlir_managed_native_lifecycle.artifact.exe"

foreach ( $path in @($xl, $source, $hostSource, $supportSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR managed native lifecycle gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR managed native lifecycle lowering failed"
}
$ir = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$type = @($ir.types | Where-Object { $_.name -eq "ManagedCounter" })
if ( $type.Count -ne 1 -or $type[0].cAbi -ne 26 -or
     $type[0].ownershipAbi -ne 2 -or $type[0].nullAbi -ne 2 -or
     $type[0].nominalSymbol -eq 0 ) {
    throw "Managed native class has no exact nominal managed-xvalue ABI"
}
$layout = @($ir.classLayouts | Where-Object { $_.type -eq $type[0].id })
$field = @($ir.classFields | Where-Object { $_.ownerType -eq $type[0].id })
if ( $layout.Count -ne 1 -or $field.Count -ne 1 -or
     $layout[0].copyAbiName -ne "noncopyable-null" -or
     $layout[0].cloneAbiName -ne "noncopyable-null" -or
     $field[0].nativeDestroySymbol -eq 0 ) {
    throw "Managed native class has no noncopyable contract or owned physical field"
}

$factory = @($ir.callTargets | Where-Object { $_.cName -eq "managed_counter_open" })
$add = @($ir.callTargets | Where-Object { $_.cName -eq "managed_counter_add" })
$destroy = @($ir.callTargets | Where-Object { $_.cName -eq "managed_counter_close" })
if ( $factory.Count -ne 1 -or $factory[0].kind -ne 3 -or
     $factory[0].returnType -ne $type[0].id -or
     $factory[0].resultPhysicalAbiName -ne "native-class-field-take" -or
     $factory[0].resultPhysicalFieldDeclaration -ne $field[0].declaration -or
     $factory[0].resultPhysicalDestroySymbol -ne $field[0].nativeDestroySymbol -or
     $add.Count -ne 1 -or $add[0].parameterCount -ne 2 -or
     @($add[0].parameterTypes)[0] -ne $type[0].id -or
     @($add[0].parameterAbis)[0] -ne 2 -or
     @($add[0].parameterPhysicalAbis)[0] -ne "native-class-handle-borrow" -or
     @($add[0].parameterPhysicalFieldDeclarations)[0] -ne $field[0].declaration -or
     $destroy.Count -ne 1 -or $destroy[0].parameterCount -ne 1 -or
     $destroy[0].symbol -ne $field[0].nativeDestroySymbol -or
     @($destroy[0].parameterPhysicalFieldDeclarations)[0] -ne $field[0].declaration ) {
    throw "Managed native factory/method/destroy CallTarget physical ABIs are incomplete"
}

$addCalls = @($ir.instructions | Where-Object {
    $_.kindName -eq "call" -and $_.targetSymbol -eq $add[0].symbol
})
$destroyCalls = @($ir.instructions | Where-Object {
    $_.kindName -eq "call" -and $_.targetSymbol -eq $destroy[0].symbol
})
if ( $addCalls.Count -ne 2 -or $destroyCalls.Count -ne 1 ) {
    throw "Managed native lifecycle fixture has an unexpected physical call count"
}
foreach ( $call in $addCalls ) {
    $projection = @($ir.instructions | Where-Object {
        $_.result -eq @($call.operands)[0] -and
        $_.runtimeOpName -eq "xvalue-object-get-literal-native-handle-borrow"
    })
    if ( $projection.Count -ne 1 -or $projection[0].type -ne $field[0].type -or
         $projection[0].operandCount -ne 1 ) {
        throw "Ordinary managed native method CALL does not consume an exact handle borrow"
    }
}
$take = @($ir.instructions | Where-Object {
    $_.result -eq @($destroyCalls[0].operands)[0] -and
    $_.runtimeOpName -eq "xvalue-object-take-literal-native-handle"
})
if ( $take.Count -ne 1 -or $take[0].type -ne $field[0].type -or
     $take[0].operandCount -ne 1 ) {
    throw "Explicit managed native destroy CALL does not consume its exact field handle"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed native lifecycle C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed native lifecycle repeat C generation failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR managed native lifecycle generated C is not deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_object_get_literal_native_handle(",
    "xlir_xvalue_object_take_literal_native_handle(",
    "xrtValueTakeHandle(item, &value)",
    "managed_counter_add(v",
    "managed_counter_close(v"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR managed native lifecycle generated C is missing: $required"
    }
}

Push-Location $Root
try {
    $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_managed_native_explicit_destroy_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_managed_native_lifecycle.exe && build\test\xlir_managed_native_lifecycle.exe'
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native lifecycle strict-C/runtime gate failed"
    }
    & $xl build $source $supportSource --backend xlir -o $artifactExe
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native lifecycle standalone artifact build failed"
    }
    & $artifactExe
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native lifecycle standalone artifact runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR managed native lifecycle ABI gate passed: noncopyable class, factory field TAKE, method BORROW, explicit destroy TAKE, automatic cleanup, deterministic strict C, and runtime."
