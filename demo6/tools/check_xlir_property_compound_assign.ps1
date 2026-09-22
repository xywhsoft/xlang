param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$propertySource = Join-Path $Root "test\cases\cgen_class_property_compound_managed.xl"
$operatorSource = Join-Path $Root "test\cases\cgen_protocol_compound_assign.xl"
$outputDir = Join-Path $Root "build\test"
foreach ( $path in @($xl, $propertySource, $operatorSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR property-compound gate input is missing: $path"
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

$propertyTypedPath = Join-Path $outputDir "xlir_property_compound.typed.json"
$propertyLoweredPath = Join-Path $outputDir "xlir_property_compound.lowered.json"
$operatorTypedPath = Join-Path $outputDir "xlir_direct_operator_compound.typed.json"
$operatorLoweredPath = Join-Path $outputDir "xlir_direct_operator_compound.lowered.json"
$propertyC = Join-Path $outputDir "xlir_property_compound.c"
$propertyRepeatC = Join-Path $outputDir "xlir_property_compound.repeat.c"
$operatorC = Join-Path $outputDir "xlir_direct_operator_compound.c"
$operatorRepeatC = Join-Path $outputDir "xlir_direct_operator_compound.repeat.c"

Push-Location $Root
try {
    Invoke-Xl @("ir", $propertySource, "--format", "json", "--emit", $propertyTypedPath) `
        "XLIR property-compound Typed IR failed"
    Invoke-Xl @("ir", $propertySource, "--lowered", "--format", "json", "--emit", $propertyLoweredPath) `
        "XLIR property-compound lowering failed"
    Invoke-Xl @("ir", $operatorSource, "--format", "json", "--emit", $operatorTypedPath) `
        "XLIR direct overloaded compound Typed IR failed"
    Invoke-Xl @("ir", $operatorSource, "--lowered", "--format", "json", "--emit", $operatorLoweredPath) `
        "XLIR direct overloaded compound lowering failed"

    $typed = [IO.File]::ReadAllText($propertyTypedPath) | ConvertFrom-Json
    $propertyAssignments = @($typed.nodes | Where-Object {
        $_.kindName -eq "assign" -and $_.opcodeName -eq "add" -and
        $_.callKindName -eq "property-set" -and $_.line -in @(65, 69, 73, 88)
    })
    if ( $propertyAssignments.Count -ne 4 ) {
        throw "expected four canonical property compound assignments"
    }
    $overloadedProperty = @($propertyAssignments | Where-Object { $_.line -eq 69 })
    $builtinProperties = @($propertyAssignments | Where-Object { $_.line -ne 69 })
    if ( $overloadedProperty.Count -ne 1 -or
         [uint64]$overloadedProperty[0].compoundOperatorSymbol -eq 0 -or
         @($builtinProperties | Where-Object {
             [uint64]$_.compoundOperatorSymbol -ne 0
         }).Count -ne 0 ) {
        throw "Typed XLIR did not separate the selected class operator from builtin property opcodes"
    }

    $operatorTyped = [IO.File]::ReadAllText($operatorTypedPath) | ConvertFrom-Json
    $directAssignments = @($operatorTyped.nodes | Where-Object {
        $_.kindName -eq "assign" -and $_.line -in @(22, 25, 28) -and
        $_.opcodeName -ne "none"
    })
    if ( $directAssignments.Count -ne 3 -or
         @($directAssignments | Where-Object {
             [uint64]$_.compoundOperatorSymbol -eq 0
         }).Count -ne 0 ) {
        throw "Typed XLIR direct class compound assignments lost their exact selected operator symbols"
    }

    $lowered = [IO.File]::ReadAllText($propertyLoweredPath) | ConvertFrom-Json
    $nodes = @{}
    foreach ( $node in $lowered.nodes ) { $nodes[[int]$node.id] = $node }
    foreach ( $line in @(65, 69, 73, 88) ) {
        $calls = @($lowered.instructions | Where-Object {
            $nodes.ContainsKey([int]$_.origin) -and
            [int]$nodes[[int]$_.origin].line -eq $line -and
            $_.kindName -in @("call", "virtual-call")
        } | Sort-Object index)
        $expected = if ( $line -eq 69 ) { 3 } else { 2 }
        if ( $calls.Count -ne $expected -or
             @($calls[0].operands).Count -lt 1 -or
             @($calls[-1].operands).Count -lt 2 -or
             [uint64]$calls[0].operands[0] -ne
                 [uint64]$calls[-1].operands[0] ) {
            throw "property compound line $line is not one shared-receiver getter/operator/setter transaction"
        }
    }
    foreach ( $case in @(
        @{ Line = 65; Kind = "binary"; Runtime = "none" },
        @{ Line = 73; Kind = "runtime-call"; Runtime = "xvalue-array-concat" },
        @{ Line = 88; Kind = "runtime-call"; Runtime = "string-concat" }
    )) {
        $operations = @($lowered.instructions | Where-Object {
            $nodes.ContainsKey([int]$_.origin) -and
            [int]$nodes[[int]$_.origin].line -eq $case.Line -and
            $_.kindName -eq $case.Kind -and $_.runtimeOpName -eq $case.Runtime
        })
        if ( $operations.Count -ne 1 ) {
            throw "property compound line $($case.Line) lost its explicit builtin operation ABI"
        }
    }

    $operatorLowered = [IO.File]::ReadAllText($operatorLoweredPath) | ConvertFrom-Json
    $operatorNodes = @{}
    foreach ( $node in $operatorLowered.nodes ) { $operatorNodes[[int]$node.id] = $node }
    foreach ( $assignment in $directAssignments ) {
        $calls = @($operatorLowered.instructions | Where-Object {
            [int]$_.origin -eq [int]$assignment.id -and
            $_.kindName -in @("call", "virtual-call")
        })
        if ( $calls.Count -ne 1 ) {
            throw "direct class compound line $($assignment.line) has no unique explicit operator call"
        }
    }

    Invoke-Xl @("cgen", $propertySource, "--backend", "xlir", "--emit", $propertyC) `
        "XLIR property-compound Cgen failed"
    Invoke-Xl @("cgen", $propertySource, "--backend", "xlir", "--emit", $propertyRepeatC) `
        "XLIR repeated property-compound Cgen failed"
    Invoke-Xl @("cgen", $operatorSource, "--backend", "xlir", "--emit", $operatorC) `
        "XLIR direct operator compound Cgen failed"
    Invoke-Xl @("cgen", $operatorSource, "--backend", "xlir", "--emit", $operatorRepeatC) `
        "XLIR repeated direct operator compound Cgen failed"
    foreach ( $pair in @(
        @($propertyC, $propertyRepeatC),
        @($operatorC, $operatorRepeatC)
    )) {
        if ( ! [Linq.Enumerable]::SequenceEqual(
                [byte[]][IO.File]::ReadAllBytes($pair[0]),
                [byte[]][IO.File]::ReadAllBytes($pair[1])) ) {
            throw "XLIR property/operator compound generated C is not byte-for-byte deterministic"
        }
    }

    Invoke-CheckedCmd `
        "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! -Werror -c test\cases\xlir_expect_support.c -o build\test\xlir_property_compound_support.o && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\test\xlir_property_compound.c test\cases\xlir_generated_module_host.c build\test\xlir_property_compound_support.o !XL6_CGEN_LINKS! -o build\test\xlir_property_compound.exe && build\test\xlir_property_compound.exe" `
        "XLIR property-compound strict-C/runtime gate failed"
    Invoke-CheckedCmd `
        "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\test\xlir_direct_operator_compound.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\xlir_direct_operator_compound.exe && build\test\xlir_direct_operator_compound.exe" `
        "XLIR direct operator compound strict-C/runtime gate failed"
} finally {
    Pop-Location
}

Write-Output "XLIR property/operator compound gate passed: semantic operator symbols are independent from setters, property receiver evaluation is shared across explicit getter/operator/setter transactions, scalar/string/collection operations are closed IR facts, and C is deterministic and strict-runtime clean."
