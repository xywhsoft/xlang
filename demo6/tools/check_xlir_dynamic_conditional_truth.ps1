param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_dynamic_conditional_truth.xl"
$hostSource = Join-Path $Root "test\cases\xlir_dynamic_conditional_truth_host.c"
$outputDir = Join-Path $Root "build\test"
$loweredPath = Join-Path $outputDir "xlir_dynamic_conditional_truth.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_dynamic_conditional_truth_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_dynamic_conditional_truth_generated_again.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR dynamic conditional truth gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR dynamic conditional truth lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$truth = @($instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and $_.runtimeOpName -eq "xvalue-truthy"
})
if ( $truth.Count -ne 2 ) {
    throw "Lowered XLIR did not preserve both dynamic conditional truth operations"
}
foreach ( $operation in $truth ) {
    $nextIndex = [int]$operation.id
    if ( $operation.operandCount -ne 1 -or $nextIndex -ge $instructions.Count -or
         $instructions[$nextIndex].kindName -ne "branch" -or
         ! (@($instructions[$nextIndex].operands) -contains [int]$operation.result) ) {
        throw "Dynamic conditional truth result is not the exact adjacent branch condition"
    }
}
$stores = @($instructions | Where-Object { $_.kindName -eq "store" })
if ( @($stores | Where-Object { $_.type -eq 0 }).Count -ne 0 ) {
    throw "Dynamic conditional branch storage lost an exact result TypeId"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR dynamic conditional truth Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR dynamic conditional truth deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR dynamic conditional truth C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
if ( ! $generated.Contains("xrtValueTruthy(") ) {
    throw "XLIR dynamic conditional truth C emission lost its mechanical runtime operation"
}

Push-Location $Root
try {
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_dynamic_conditional_truth.exe && build\test\xlir_dynamic_conditional_truth.exe"
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR dynamic conditional truth strict-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR dynamic conditional truth gate ok: open-xvalue conditions become explicit XVALUE_TRUTHY results, adjacent branches, exact merge storage, deterministic strict C and runtime semantics."
