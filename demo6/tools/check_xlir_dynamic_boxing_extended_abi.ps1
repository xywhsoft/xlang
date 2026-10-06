param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_dynamic_boxing_extended_abi.xl"
$support = Join-Path $Root "test\cases\xlir_dynamic_boxing_extended_support.c"
$runtimeHost = Join-Path $Root "test\cases\xlir_dynamic_boxing_extended_host.c"
$devSource = Join-Path $Root "test\cases\cgen_dev_handfeel_regression.xl"
$unionSource = Join-Path $Root "test\cases\cgen_union.xl"
$expectSupport = Join-Path $Root "test\cases\xlir_expect_support.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_dynamic_boxing_extended.typed.json"
$loweredPath = Join-Path $outputDir "xlir_dynamic_boxing_extended.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_dynamic_boxing_extended.c"
$exePath = Join-Path $outputDir "xlir_dynamic_boxing_extended.exe"

foreach ( $path in @($xl, $source, $support, $runtimeHost, $devSource,
                      $unionSource, $expectSupport) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR extended dynamic-boxing gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --typed --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR extended dynamic-boxing Typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$inlineMembers = @($typed.nodes | Where-Object {
    $_.kindName -eq "member" -and
    $_.memberAbiName -eq "class-inline-union-field"
})
if ( $inlineMembers.Count -ne 4 ) {
    throw "Typed XLIR did not resolve every inline-union read/write member"
}

& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR extended dynamic-boxing lowering failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$foreign = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-foreign-scalar"
})
$errorBox = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-error-ref"
})
if ( $foreign.Count -ne 1 -or $errorBox.Count -ne 1 ) {
    throw "Lowered XLIR has no unique native-scalar and error-reference boxes"
}
foreach ( $box in @($foreign[0], $errorBox[0]) ) {
    $sourceValue = $box.operands[0]
    $definition = @($lowered.instructions | Where-Object {
        $_.result -eq $sourceValue
    })
    $identity = @($lowered.types | Where-Object {
        $_.id -eq $box.runtimeIdentityType
    })
    if ( $box.kindName -ne "runtime-call" -or $box.operandCount -ne 1 -or
         $box.runtimeIdentityType -eq 0 -or $definition.Count -ne 1 -or
         $definition[0].type -ne $box.runtimeIdentityType -or
         $identity.Count -ne 1 -or $identity[0].typeValueId -eq 0 ) {
        throw "Lowered XLIR dynamic box lost its exact source TypeId identity"
    }
}
if ( @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-object-get-literal-i64"
    }).Count -ne 1 -or
     @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-object-get-literal-f64"
    }).Count -ne 1 ) {
    throw "Lowered XLIR inline union has no exact integer/floating projections"
}

& $xl cgen $source --backend xlir --emit $generatedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR extended dynamic-boxing Cgen failed" }
$generated = [System.IO.File]::ReadAllText(
    $generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "extern xvalue* xl6ErrorValueRefV1(const xerror*, uint64_t)",
    " = xl6ErrorValueRefV1(",
    "xlir_xvalue_object_get_literal_f64"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR extended dynamic-boxing C emission is incomplete: $required"
    }
}
if ( $generated -notmatch '(?m)^\s*v[0-9]+ = xl6ValueBoxNativeScalarV1\(&v[0-9]+, [0-9]+, 0x[0-9A-F]+ULL\);' ) {
    throw "XLIR foreign scalar boxing does not use one exact resident profile and logical TypeId"
}

Push-Location $Root
try {
    $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include "build\test\xlir_dynamic_boxing_extended.c" "test\cases\xlir_dynamic_boxing_extended_host.c" !XL6_CGEN_LINKS! -o "build\test\xlir_dynamic_boxing_extended.exe" && "build\test\xlir_dynamic_boxing_extended.exe"'
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR extended dynamic-boxing strict-C/runtime gate failed"
    }
    & $xl run $devSource --backend xlir
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR dynamic error-boxing package runtime failed"
    }
    & $xl run $unionSource $expectSupport --backend xlir
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR foreign-float/inline-union package runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR extended dynamic-boxing ABI gate passed: resolved inline-union members, exact foreign float/error identities, retained cloneable error handle, strict C, and package runtime."
exit 0
