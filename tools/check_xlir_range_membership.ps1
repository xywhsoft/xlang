param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_range_membership.xl"
$hostSource = Join-Path $Root "test\cases\xlir_range_membership_host.c"
$compatSource = Join-Path $Root "test\cases\cgen_in_range.xl"
$outputDir = Join-Path $Root "build\test"
$textPath = Join-Path $outputDir "xlir_range_membership.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_range_membership.lowered.json"
$cPath = Join-Path $outputDir "xlir_range_membership_generated.c"
$compatCPath = Join-Path $outputDir "cgen_in_range_xlir.c"
$exePath = Join-Path $outputDir "xlir_range_membership_host.exe"

foreach ( $path in @($xl, $source, $hostSource, $compatSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR range membership gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format text --emit $textPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR range membership text IR failed" }
$text = [System.IO.File]::ReadAllText($textPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($text, "runtime=i64-range-contains")).Count -ne 11 -or
     ([regex]::Matches($text, "kind=temporary")).Count -ne 0 -or
     ([regex]::Matches($text, " release ")).Count -ne 0 ) {
    throw "XLIR range membership has no exact four-scalar operation ABI"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR range membership JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$ops = @($json.instructions | Where-Object {
    $_.runtimeOpName -eq "i64-range-contains"
})
$typeById = @{}
foreach ( $type in @($json.types) ) { $typeById[[string]$type.id] = $type }
$definitionByResult = @{}
foreach ( $instruction in @($json.instructions | Where-Object { $_.result -ne 0 }) ) {
    $definitionByResult[[string]$instruction.result] = $instruction
}
$badOperand = $false
foreach ( $op in $ops ) {
    foreach ( $valueId in @($op.operands) ) {
        $definition = $definitionByResult[[string]$valueId]
        $type = if ( $null -ne $definition ) { $typeById[[string]$definition.type] } else { $null }
        if ( $null -eq $type -or $type.cAbi -ne 3 ) { $badOperand = $true }
    }
}
if ( $ops.Count -ne 11 -or @($ops | Where-Object {
        $_.kindName -ne "runtime-call" -or $_.operandCount -ne 4 -or
        $_.hasImmediate -ne 0 -or $_.targetSymbol -ne 0 -or
        $_.targetValue -ne 0 -or $_.calleeType -ne 0 -or
        $_.payloadType -ne 0 -or $_.opcodeName -ne "none"
    }).Count -ne 0 -or $badOperand ) {
    throw "XLIR range membership JSON has no exact item/start/stop/step i64 -> bool ABI"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR range membership Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static bool xlir_i64_range_contains(int64_t item, int64_t start, int64_t stop, int64_t step)",
    "uint64_t distance; uint64_t magnitude;",
    "if (step == 0) return false;",
    "if (item < start || item >= stop) return false;",
    "distance = (uint64_t)item - (uint64_t)start;",
    "magnitude = (uint64_t)step;",
    "if (item > start || item <= stop) return false;",
    "distance = (uint64_t)start - (uint64_t)item;",
    "magnitude = (uint64_t)0 - (uint64_t)step;",
    "return distance % magnitude == 0;"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR range membership C rendering is incomplete or signed-overflow-prone: $required"
    }
}
if ( ([regex]::Matches($generated,
        "xlir_i64_range_contains\(v[0-9]+, v[0-9]+, v[0-9]+, v[0-9]+\)")).Count -ne 11 ) {
    throw "XLIR range membership C rendering did not preserve operation count"
}

& $xl cgen $compatSource --backend xlir --emit $compatCPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR established range-membership compatibility Cgen failed"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR range membership strict generated-C runtime failed" }

Write-Output "XLIR range membership gate ok: verified RANGE descriptor consumption, default start/step materialization, four-i64 bool ABI, positive/negative/runtime-zero step, overflow-safe unsigned distance, established fixture composition, strict C and runtime."
exit 0
