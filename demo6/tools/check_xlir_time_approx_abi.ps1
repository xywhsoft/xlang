param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_time_literals.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$legacySupport = Join-Path $Root "test\cases\xlir_cgen_differential_expect_i64_legacy_support.c"
$outputDir = Join-Path $Root "build\test"
$loweredPath = Join-Path $outputDir "xlir_time_approx_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_time_approx_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_time_approx_abi_generated_again.c"
$legacyPath = Join-Path $outputDir "xlir_time_approx_abi_legacy.c"

foreach ( $path in @($xl, $source, $hostSource, $legacySupport) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR time approximate ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR time approximate lowering failed"
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "time-approx"
})
if ( $operations.Count -ne 1 ) {
    throw "Lowered XLIR time approximate operation count is not canonical: $($operations.Count)"
}
$operation = $operations[0]
$resultType = @($lowered.types | Where-Object { $_.id -eq $operation.type })
$origin = @($lowered.nodes | Where-Object { $_.id -eq $operation.origin })
$definitions = @(
    foreach ( $value in @($operation.operands) ) {
        @($lowered.instructions | Where-Object { $_.result -eq $value })
    }
)
$operandTypes = @(
    foreach ( $definition in $definitions ) {
        @($lowered.types | Where-Object { $_.id -eq $definition.type })
    }
)
if ( $operation.kindName -ne "runtime-call" -or
     $operation.operandCount -ne 2 -or $operation.result -eq 0 -or
     $resultType.Count -ne 1 -or $resultType[0].name -ne "bool" -or
     $resultType[0].cAbi -ne 2 -or $resultType[0].ownershipAbi -ne 1 -or
     $origin.Count -ne 1 -or $origin[0].opcodeName -ne "approx" -or
     $origin[0].operatorErrorAbiName -ne "no-raise" -or
     $definitions.Count -ne 2 -or $operandTypes.Count -ne 2 -or
     $definitions[0].type -ne $definitions[1].type -or
     @($operandTypes | Where-Object {
        $_.name -ne "time" -or $_.cAbi -ne 40 -or $_.ownershipAbi -ne 1
     }).Count -ne 0 ) {
    throw "time-approx has no exact same-TypeId time/time-to-bool trivial no-raise ABI"
}
if ( @($lowered.instructions | Where-Object {
        $_.kindName -eq "binary" -and $_.opcodeName -eq "approx"
    }).Count -ne 0 ) {
    throw "Time approximate semantics leaked into a physical BINARY instruction"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR time approximate Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR time approximate deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)) ) {
    throw "XLIR time approximate generated C is not byte-for-byte deterministic"
}
$generated = [System.IO.File]::ReadAllText(
    $generatedPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($generated, 'xrtTimeNear\(').Count) -ne 2 -or
     ! $generated.Contains("UINT64_C(10000000)") ) {
    throw "XLIR time approximate C emission lost its fixed xrtTimeNear tolerance ABI"
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_time_approx_abi_xlir.exe && build\test\xlir_time_approx_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR time approximate strict-C runtime failed"
    }

    Invoke-Xl @("cgen", $source, "--backend", "legacy", "--emit", $legacyPath) `
        "Legacy time approximate differential Cgen failed"
    $legacyCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $legacyPath $legacySupport !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_time_approx_abi_legacy.exe && build\test\xlir_time_approx_abi_legacy.exe"
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Legacy time approximate differential runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR time approximate ABI gate passed: exact same-TypeId time/time-to-bool no-raise RuntimeOp, fixed xrtTimeNear tolerance, deterministic strict C and legacy differential runtime semantics."
