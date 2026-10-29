param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_pow.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$loweredPath = Join-Path $outputDir "xlir_i64_pow_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_i64_pow_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_i64_pow_abi_generated_again.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR i64 power ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR i64 power lowering failed"
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "i64-pow"
})
if ( $operations.Count -ne 6 ) {
    throw "Lowered XLIR i64 power operation count is not canonical: $($operations.Count)"
}
foreach ( $operation in $operations ) {
    $type = @($lowered.types | Where-Object { $_.id -eq $operation.type })
    $origin = @($lowered.nodes | Where-Object { $_.id -eq $operation.origin })
    $definitions = @(
        foreach ( $value in @($operation.operands) ) {
            @($lowered.instructions | Where-Object { $_.result -eq $value })
        }
    )
    if ( $operation.kindName -ne "runtime-call" -or
         $operation.operandCount -ne 2 -or $operation.result -eq 0 -or
         $type.Count -ne 1 -or $type[0].name -ne "int" -or
         $type[0].cAbi -ne 3 -or $type[0].integerTargetName -ne "i64" -or
         $type[0].ownershipAbi -ne 1 -or
         $origin.Count -ne 1 -or $origin[0].opcodeName -ne "pow" -or
         $origin[0].operatorErrorAbiName -ne "no-raise" -or
         $definitions.Count -ne 2 -or
         @($definitions | Where-Object { $_.type -ne $operation.type }).Count -ne 0 ) {
        throw "i64-pow has no exact same-TypeId signed-64 trivial no-raise ABI"
    }
}
if ( @($lowered.instructions | Where-Object {
        $_.kindName -eq "binary" -and $_.opcodeName -eq "pow"
    }).Count -ne 0 ) {
    throw "Power semantics leaked into a physical BINARY instruction"
}

$compoundNode = @($lowered.nodes | Where-Object {
    $_.kindName -eq "assign" -and $_.line -eq 10 -and $_.opcodeName -eq "pow"
})
$compoundInstructions = if ( $compoundNode.Count -eq 1 ) {
    @($lowered.instructions | Where-Object {
        $_.origin -eq $compoundNode[0].id
    } | Sort-Object index)
} else { @() }
if ( $compoundInstructions.Count -ne 3 -or
     ! [System.Linq.Enumerable]::SequenceEqual(
        [string[]]@($compoundInstructions.kindName),
        [string[]]@("load", "runtime-call", "store")) -or
     $compoundInstructions[1].runtimeOpName -ne "i64-pow" -or
     $compoundInstructions[0].targetSymbol -eq 0 -or
     $compoundInstructions[0].targetSymbol -ne $compoundInstructions[2].targetSymbol -or
     $compoundInstructions[1].operands[0] -ne $compoundInstructions[0].result -or
     $compoundInstructions[2].operands[0] -ne $compoundInstructions[1].result ) {
    throw "Power compound assignment lost its exact LOAD -> i64-pow -> STORE storage contract"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR i64 power Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR i64 power deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)) ) {
    throw "XLIR i64 power generated C is not byte-for-byte deterministic"
}
$generated = [System.IO.File]::ReadAllText(
    $generatedPath, [System.Text.Encoding]::UTF8)
if ( ! $generated.Contains("static int64_t xlir_i64_pow(") -or
     ([regex]::Matches($generated, 'xlir_i64_pow\(').Count - 1) -ne 6 ) {
    throw "XLIR i64 power C emission is not a fixed helper plus six mechanical calls"
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_i64_pow_abi_xlir.exe && build\test\xlir_i64_pow_abi_xlir.exe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR i64 power strict-C runtime failed"
    }

} finally {
    Pop-Location
}

Write-Output "XLIR i64 power ABI gate passed: six exact same-TypeId signed-64 no-raise i64-pow RuntimeOps, right associativity, LOAD/pow/STORE compound assignment, and deterministic strict-C runtime semantics."
