param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$moduleHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\test"
$cases = @(
    @{ Name = "xlir_internal_property_call_abi"; Source = "test\cases\xlir_internal_property_call_abi.xl" },
    @{ Name = "xlir_internal_property_error_propagation"; Source = "test\cases\xlir_internal_property_error_propagation.xl" }
)

foreach ( $path in @($xl, $moduleHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR internal property CallTarget gate input is missing: $path"
    }
}
foreach ( $case in $cases ) {
    $path = Join-Path $Root $case.Source
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR internal property CallTarget fixture is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure)
{
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

function Invoke-CheckedCmd([string]$Command, [string]$Failure)
{
    & $env:ComSpec /d /v:on /c $Command
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

function Get-CallTargetForDeclarationLine($Ir, [int]$Line)
{
    $declarations = @($Ir.nodes | Where-Object {
        $_.kindName -eq "function-declaration" -and
        $_.sourceKindName -eq "get" -and $_.line -eq $Line
    })
    if ( $declarations.Count -ne 1 ) {
        throw "Internal property getter declaration is not unique at line $Line"
    }
    $targets = @($Ir.callTargets | Where-Object {
        $_.declaration -eq $declarations[0].id
    })
    if ( $targets.Count -ne 1 ) {
        throw "Internal property getter has no unique CallTarget at line $Line"
    }
    return $targets[0]
}

$loweredByName = @{}
Push-Location $Root
try {
    foreach ( $case in $cases ) {
        $name = $case.Name
        $source = Join-Path $Root $case.Source
        $loweredPath = Join-Path $outputDir "$name.lowered.json"
        $generatedPath = Join-Path $outputDir "$name.xlir.c"
        $repeatPath = Join-Path $outputDir "$name.xlir.repeat.c"
        $legacyPath = Join-Path $outputDir "$name.legacy.c"

        Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
            "XLIR internal property lowering failed: $name"
        $lowered = [System.IO.File]::ReadAllText(
            $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $loweredByName[$name] = $lowered

        Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
            "XLIR internal property Cgen failed: $name"
        Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $repeatPath) `
            "XLIR internal property repeat Cgen failed: $name"
        if ( ! [System.Linq.Enumerable]::SequenceEqual(
                [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
                [byte[]][System.IO.File]::ReadAllBytes($repeatPath)
            ) ) {
            throw "XLIR internal property C output is not byte-for-byte deterministic: $name"
        }
        Invoke-Xl @("cgen", $source, "--backend", "legacy", "--emit", $legacyPath) `
            "Legacy internal property differential Cgen failed: $name"

        $xlirExe = "build\test\$name.xlir.exe"
        $legacyExe = "build\test\$name.legacy.exe"
        Invoke-CheckedCmd `
            "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\test\$name.xlir.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o $xlirExe && $xlirExe" `
            "XLIR internal property strict-C/runtime gate failed: $name"
        Invoke-CheckedCmd `
            "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror build\test\$name.legacy.c !XL6_CGEN_LINKS! -o $legacyExe && $legacyExe" `
            "Legacy internal property differential runtime failed: $name"
    }
} finally {
    Pop-Location
}

$normal = $loweredByName["xlir_internal_property_call_abi"]
$normalSpecs = @(
    @{ DeclarationLine = 9; UseLine = 24; Parameters = 1; OriginKind = "member" },
    @{ DeclarationLine = 13; UseLine = 27; Parameters = 2; OriginKind = "index" },
    @{ DeclarationLine = 17; UseLine = 30; Parameters = 1; OriginKind = "member" }
)
foreach ( $spec in $normalSpecs ) {
    $target = Get-CallTargetForDeclarationLine $normal $spec.DeclarationLine
    if ( $target.kind -ne 1 -or $target.errorAbiName -ne "no-raise" -or
         $target.returnType -eq 0 -or
         $target.parameterCount -ne $spec.Parameters -or
         @($target.parameterTypes).Count -ne $spec.Parameters -or
         @($target.parameterAbis).Count -ne $spec.Parameters -or
         $target.parameterAbis[0] -ne 2 ) {
        throw "Internal property getter lost its exact synchronous CallTarget ABI at line $($spec.DeclarationLine)"
    }
    $propertyNodes = @($normal.nodes | Where-Object {
        $_.line -eq $spec.UseLine -and $_.kindName -eq "member" -and
        $_.callKindName -eq "property-get" -and
        $_.targetSymbol -eq $target.symbol
    })
    if ( $propertyNodes.Count -ne 1 ) {
        throw "Internal property use lost its resolved semantic target at line $($spec.UseLine)"
    }
    $property = $propertyNodes[0]
    $calls = @($normal.instructions | Where-Object {
        $_.kindName -eq "call" -and $_.targetSymbol -eq $target.symbol
    })
    if ( $calls.Count -ne 1 -or $calls[0].operandCount -ne $spec.Parameters -or
         $calls[0].type -ne $target.returnType -or $calls[0].result -eq 0 ) {
        throw "Internal property did not lower to one exact typed CALL at line $($spec.UseLine)"
    }
    $origin = @($normal.nodes | Where-Object { $_.id -eq $calls[0].origin })
    if ( $origin.Count -ne 1 -or $origin[0].kindName -ne $spec.OriginKind -or
         ($spec.OriginKind -eq "member" -and $origin[0].id -ne $property.id) -or
         ($spec.OriginKind -eq "index" -and $property.parent -ne $origin[0].id) ) {
        throw "Container-property/indexed-property origin partition is ambiguous at line $($spec.UseLine)"
    }
    if ( @($normal.exitPlans | Where-Object {
            $_.origin -eq $property.id -and $_.kind -eq 4
         }).Count -ne 1 ) {
        throw "Resolved property has no frontend-owned throw cleanup plan at line $($spec.UseLine)"
    }
    if ( @($normal.instructions | Where-Object {
            $_.kindName -eq "error-test" -and $_.targetSymbol -eq $target.symbol
         }).Count -ne 0 ) {
        throw "No-raise property acquired a spurious error edge at line $($spec.UseLine)"
    }
}

$failing = $loweredByName["xlir_internal_property_error_propagation"]
$failingTarget = Get-CallTargetForDeclarationLine $failing 8
$failingProperty = @($failing.nodes | Where-Object {
    $_.line -eq 20 -and $_.kindName -eq "member" -and
    $_.callKindName -eq "property-get" -and
    $_.targetSymbol -eq $failingTarget.symbol
})
$failingCall = @($failing.instructions | Where-Object {
    $_.kindName -eq "call" -and $_.targetSymbol -eq $failingTarget.symbol
})
if ( $failingTarget.kind -ne 1 -or
     $failingTarget.errorAbiName -ne "may-raise" -or
     $failingTarget.returnType -eq 0 -or $failingTarget.parameterCount -ne 1 -or
     $failingProperty.Count -ne 1 -or $failingCall.Count -ne 1 ) {
    throw "May-raise internal property has no exact typed CallTarget/CALL contract"
}
$instructions = @($failing.instructions)
$callIndex = [Array]::IndexOf($instructions, $failingCall[0])
$errorTestIndex = $callIndex + 1
while ( $errorTestIndex -gt 0 -and $errorTestIndex -lt $instructions.Count -and
        $instructions[$errorTestIndex].kindName -eq "release" ) {
    ++$errorTestIndex
}
$errorTest = if ( $callIndex -ge 0 -and $errorTestIndex -lt $instructions.Count ) {
    $instructions[$errorTestIndex]
} else { $null }
if ( $errorTest -eq $null -or $errorTest.kindName -ne "error-test" -or
     $errorTest.origin -ne $failingProperty[0].id -or
     $errorTest.targetSymbol -ne $failingTarget.symbol -or
     @($errorTest.operands).Count -ne 1 -or
     $errorTest.operands[0] -ne $failingCall[0].result -or
     $errorTest.trueTarget -eq 0 -or $errorTest.falseTarget -eq 0 -or
     @($failing.exitPlans | Where-Object {
         $_.origin -eq $failingProperty[0].id -and $_.kind -eq 4
     }).Count -ne 1 ) {
    throw "May-raise property CALL is not guarded by its cleanup-complete explicit ERROR_TEST/cleanup CFG"
}

$failingGenerated = [System.IO.File]::ReadAllText(
    (Join-Path $outputDir "xlir_internal_property_error_propagation.xlir.c"),
    [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "typedef struct xlir_result_i64", "if (r", ".ok)",
    "xrtTakeError()", "xlir_error_kind_is"
) ) {
    if ( ! $failingGenerated.Contains($required) ) {
        throw "Mechanical property error-carrier C is missing: $required"
    }
}

Write-Output "XLIR internal property CallTarget ABI gate passed: container getter then ordinary index, indexed getter argument frame, exact no-raise calls, may-raise result carrier with cleanup-complete ERROR_TEST and frontend-owned cleanup CFG, byte-for-byte deterministic strict C, and legacy differential runtime semantics."
