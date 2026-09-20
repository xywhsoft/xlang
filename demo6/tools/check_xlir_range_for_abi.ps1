param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_range_for_abi.xl"
$compatSource = Join-Path $Root "test\cases\cgen_range_assert.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedJsonPath = Join-Path $outputDir "xlir_range_for_abi.typed.json"
$loweredJsonPath = Join-Path $outputDir "xlir_range_for_abi.lowered.json"
$cPath = Join-Path $outputDir "xlir_range_for_abi_generated.c"
$compatCPath = Join-Path $outputDir "cgen_range_assert_xlir.c"
$exePath = Join-Path $outputDir "xlir_range_for_abi_host.exe"

foreach ( $path in @($xl, $source, $compatSource, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR range-for ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR range-for typed JSON failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$rangeLoops = @($typed.nodes | Where-Object { $_.loopAbiName -ne "none" })
if ( $rangeLoops.Count -ne 5 -or
     @($rangeLoops | Where-Object {
         $_.loopAbiName -eq "range-i64-value" -and
         $_.loopErrorAbiName -eq "no-raise"
     }).Count -ne 1 -or
     @($rangeLoops | Where-Object {
         $_.loopAbiName -eq "range-i64-index-value" -and
         $_.loopErrorAbiName -eq "may-raise"
     }).Count -ne 1 -or
     @($rangeLoops | Where-Object {
         $_.loopAbiName -eq "range-i64-value" -and
         $_.loopErrorAbiName -eq "may-raise"
     }).Count -ne 3 ) {
    throw "Typed XLIR range-for target layout or error effect is not exact"
}
$rangeDescriptors = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "range"
})
if ( $rangeDescriptors.Count -ne 5 -or @($rangeDescriptors | Where-Object {
        $_.admissionName -ne "verified" -or
        $_.resultAbiName -ne "descriptor" -or
        $_.errorAbiName -ne "no-raise" -or
        $_.operandCount -lt 1 -or $_.operandCount -gt 3
    }).Count -ne 0 ) {
    throw "Typed XLIR range descriptor is not a verified non-materialized ABI"
}
$raisingLoopIds = @($rangeLoops | Where-Object {
    $_.loopErrorAbiName -eq "may-raise"
} | ForEach-Object { $_.id })
$loopExitPlans = @($typed.exitPlans | Where-Object { $_.origin -in $raisingLoopIds })
if ( $loopExitPlans.Count -ne 4 -or @($loopExitPlans | Where-Object {
        $_.kind -ne 4 -or $_.targetScopeDepth -ne 0 -or $_.targetRegion -ne 0
    }).Count -ne 0 ) {
    throw "Typed XLIR range-for may-raise effect has no exact throw exit plan"
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR range-for lowered JSON failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$definitionByResult = @{}
foreach ( $instruction in @($lowered.instructions | Where-Object { $_.result -ne 0 }) ) {
    $definitionByResult[[string]$instruction.result] = $instruction
}
$raises = @($lowered.instructions | Where-Object { $_.kindName -eq "error-raise" })
$badRaise = $false
foreach ( $raise in $raises ) {
    $message = if ( $raise.operandCount -eq 1 ) {
        $definitionByResult[[string]$raise.operands[0]]
    } else { $null }
    if ( $raise.origin -notin $raisingLoopIds -or $raise.hasImmediate -ne 1 -or
         $null -eq $message -or $message.kindName -ne "runtime-call" -or
         $message.runtimeOpName -ne "string-dup" -or $message.hasImmediate -ne 1 ) {
        $badRaise = $true
    }
}
if ( $raises.Count -ne 4 -or $badRaise ) {
    throw "Lowered XLIR range-for zero-step failure is not an explicit RangeError CFG edge"
}
if ( @($lowered.instructions | Where-Object {
        $_.runtimeOpName -ne "none" -and $_.runtimeOpName -match "range"
    }).Count -ne 0 ) {
    throw "Lowered XLIR range-for leaked an iterable/runtime range operation to Cgen"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR range-for Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($generated,
        [regex]::Escape('xrtStrDup("range step cannot be 0")'))).Count -ne 4 -or
     ([regex]::Matches($generated,
        'xlir_error_create_at\("RangeError", v[0-9]+, (?:10, 1|19, 1|44, 1|59, 5)\)')).Count -ne 4 -or
     $generated.Contains('xlir_error_create("RangeError"') -or
     ([regex]::Matches($generated, "\brange\s*\(")).Count -ne 0 -or
     $generated.Contains("loopAbi") ) {
    throw "XLIR range-for C rendering is not mechanical from verified CFG"
}

& $xl cgen $compatSource --backend xlir --emit $compatCPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR established single/double/negative range-for compatibility Cgen failed"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR range-for strict generated-C runtime failed"
}

Write-Output "XLIR range-for ABI gate ok: verified descriptor consumption, single and index/value targets, continue-safe ordinal progression, positive/negative steps, evaluate-once operands, explicit dynamic-zero RangeError CFG, mechanical C, strict compile, and runtime semantics."
exit 0
