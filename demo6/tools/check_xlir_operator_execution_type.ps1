param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$compiler = Join-Path $Root "release\xl.exe"
$unsignedSource = Join-Path $Root "test\cases\type_int_minmax_bounds.xl"
$foreignSource = Join-Path $Root "test\cases\extlib_imagehash.xl"
$foreignBoolSource = Join-Path $Root "test\cases\stdlib_json.xl"
$languageBoolSource = Join-Path $Root "release\examples\class_field_persistence_attrs.xl"
$flowSource = Join-Path $Root "test\cases\runtime_var_type_storage_matrix.xl"
$moduleHost = Join-Path $Root "test\cases\xlir_module_init_host.c"
$outputDir = Join-Path $Root "build\test"
$unsignedTypedPath = Join-Path $outputDir "xlir_operator_execution_unsigned.typed.json"
$unsignedLoweredPath = Join-Path $outputDir "xlir_operator_execution_unsigned.lowered.json"
$unsignedCPath = Join-Path $outputDir "xlir_operator_execution_unsigned.generated.c"
$unsignedAgainPath = Join-Path $outputDir "xlir_operator_execution_unsigned.generated_again.c"
$unsignedExePath = Join-Path $outputDir "xlir_operator_execution_unsigned.exe"
$foreignTypedPath = Join-Path $outputDir "xlir_operator_execution_foreign.typed.json"
$foreignLoweredPath = Join-Path $outputDir "xlir_operator_execution_foreign.lowered.json"
$foreignCPath = Join-Path $outputDir "xlir_operator_execution_foreign.generated.c"
$foreignObjectPath = Join-Path $outputDir "xlir_operator_execution_foreign.o"
$foreignBoolTypedPath = Join-Path $outputDir "xlir_operator_execution_foreign_bool.typed.json"
$foreignBoolLoweredPath = Join-Path $outputDir "xlir_operator_execution_foreign_bool.lowered.json"
$foreignBoolCPath = Join-Path $outputDir "xlir_operator_execution_foreign_bool.generated.c"
$foreignBoolObjectPath = Join-Path $outputDir "xlir_operator_execution_foreign_bool.o"
$languageBoolTypedPath = Join-Path $outputDir "xlir_operator_execution_language_bool.typed.json"
$languageBoolLoweredPath = Join-Path $outputDir "xlir_operator_execution_language_bool.lowered.json"
$languageBoolCPath = Join-Path $outputDir "xlir_operator_execution_language_bool.generated.c"
$languageBoolObjectPath = Join-Path $outputDir "xlir_operator_execution_language_bool.o"
$flowTypedPath = Join-Path $outputDir "xlir_operator_execution_flow.typed.json"
$flowLoweredPath = Join-Path $outputDir "xlir_operator_execution_flow.lowered.json"
$flowCPath = Join-Path $outputDir "xlir_operator_execution_flow.generated.c"
$flowAgainPath = Join-Path $outputDir "xlir_operator_execution_flow.generated_again.c"
$flowObjectPath = Join-Path $outputDir "xlir_operator_execution_flow.o"

foreach ( $path in @($compiler, $unsignedSource, $foreignSource,
                      $foreignBoolSource, $languageBoolSource, $flowSource,
                      $moduleHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR operator execution-type gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure)
{
    & $compiler @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

function Read-Json([string]$Path)
{
    return [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8) |
        ConvertFrom-Json
}

Invoke-Xl @("ir", $unsignedSource, "--format", "json", "--emit", $unsignedTypedPath) `
    "XLIR unsigned operator Typed IR failed"
Invoke-Xl @("ir", $unsignedSource, "--lowered", "--format", "json", "--emit", $unsignedLoweredPath) `
    "XLIR unsigned operator lowering failed"
$unsignedTyped = Read-Json $unsignedTypedPath
$unsignedLowered = Read-Json $unsignedLoweredPath
$unsignedCompare = @($unsignedTyped.nodes | Where-Object {
    $_.line -eq 21 -and $_.sourceKindName -eq "binary" -and
    $_.opcodeName -eq "gt"
})
if ( $unsignedCompare.Count -ne 1 ) {
    throw "Typed XLIR did not retain the unique uint64/int64 comparison"
}
$unsignedExecutionType = @($unsignedTyped.types | Where-Object {
    $_.id -eq $unsignedCompare[0].operatorExecutionType -and
    $_.name -eq "uint64" -and $_.integerTargetName -eq "u64" -and
    $_.ownershipAbi -eq 1
})
if ( $unsignedExecutionType.Count -ne 1 ) {
    throw "Typed XLIR did not fix uint64/int64 comparison to the exact u64 execution domain"
}
$operatorConversions = @($unsignedLowered.instructions | Where-Object {
    $_.origin -eq $unsignedCompare[0].id -and
    $_.kindName -eq "scalar-convert" -and
    $_.scalarConversionAbiName -eq "operator-i64-to-u64" -and
    $_.type -eq $unsignedCompare[0].operatorExecutionType
})
if ( $operatorConversions.Count -ne 1 ) {
    throw "Lowered XLIR did not materialize the unique operator-local i64-to-u64 conversion"
}

Invoke-Xl @("cgen", $unsignedSource, "--backend", "xlir", "--emit", $unsignedCPath) `
    "XLIR unsigned operator Cgen failed"
Invoke-Xl @("cgen", $unsignedSource, "--backend", "xlir", "--emit", $unsignedAgainPath) `
    "XLIR unsigned operator deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($unsignedCPath),
        [byte[]][System.IO.File]::ReadAllBytes($unsignedAgainPath)
    ) ) {
    throw "XLIR unsigned operator C output is not deterministic"
}
$unsignedCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$unsignedCPath`" -DXLIR_GENERATED_MODULE_PREINCLUDED=1 `"$moduleHost`" !XL6_CGEN_LINKS! -o `"$unsignedExePath`" && `"$unsignedExePath`""
& $env:ComSpec /d /v:on /c $unsignedCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR unsigned operator strict-C/runtime gate failed"
}

Invoke-Xl @("ir", $foreignSource, "--format", "json", "--emit", $foreignTypedPath) `
    "XLIR foreign operator Typed IR failed"
Invoke-Xl @("ir", $foreignSource, "--lowered", "--format", "json", "--emit", $foreignLoweredPath) `
    "XLIR foreign operator lowering failed"
$foreignTyped = Read-Json $foreignTypedPath
$foreignLowered = Read-Json $foreignLoweredPath
$foreignCompare = @($foreignTyped.nodes | Where-Object {
    $_.line -eq 18 -and $_.sourceKindName -eq "binary" -and
    $_.opcodeName -eq "eq"
})
if ( $foreignCompare.Count -ne 1 -or
     @($foreignTyped.types | Where-Object {
         $_.id -eq $foreignCompare[0].operatorExecutionType -and
         $_.name -eq "c_int" -and $_.ownershipAbi -eq 1
     }).Count -ne 1 ) {
    throw "Typed XLIR did not preserve the exact foreign c_int comparison domain"
}
if ( @($foreignLowered.instructions | Where-Object {
         $_.origin -eq $foreignCompare[0].id -and
         $_.kindName -eq "scalar-convert" -and
         $_.scalarConversionAbiName -eq "integer-to-integer" -and
         $_.type -eq $foreignCompare[0].operatorExecutionType
     }).Count -ne 1 ) {
    throw "Lowered XLIR did not materialize the exact language-integer to c_int conversion"
}

Invoke-Xl @("cgen", $foreignSource, "--backend", "xlir", "--emit", $foreignCPath) `
    "XLIR foreign operator Cgen failed"
$foreignCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c `"$foreignCPath`" -o `"$foreignObjectPath`""
& $env:ComSpec /d /v:on /c $foreignCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR foreign operator strict-C object gate failed"
}

Invoke-Xl @("ir", $foreignBoolSource, "--format", "json", "--emit", $foreignBoolTypedPath) `
    "XLIR foreign bool operator Typed IR failed"
Invoke-Xl @("ir", $foreignBoolSource, "--lowered", "--format", "json", "--emit", $foreignBoolLoweredPath) `
    "XLIR foreign bool operator lowering failed"
$foreignBoolTyped = Read-Json $foreignBoolTypedPath
$foreignBoolLowered = Read-Json $foreignBoolLoweredPath
$foreignBoolCompare = @($foreignBoolTyped.nodes | Where-Object {
    $_.line -eq 36 -and $_.sourceKindName -eq "binary" -and
    $_.opcodeName -eq "ne"
})
if ( $foreignBoolCompare.Count -ne 1 -or
     @($foreignBoolTyped.types | Where-Object {
         $_.id -eq $foreignBoolCompare[0].operatorExecutionType -and
         $_.name -eq "c_bool" -and $_.ownershipAbi -eq 1
     }).Count -ne 1 ) {
    throw "Typed XLIR did not preserve the exact foreign c_bool comparison domain"
}
if ( @($foreignBoolLowered.instructions | Where-Object {
         $_.origin -eq $foreignBoolCompare[0].id -and
         $_.kindName -eq "scalar-convert" -and
         $_.scalarConversionAbiName -eq "integer-to-bool" -and
         $_.type -eq $foreignBoolCompare[0].operatorExecutionType
     }).Count -ne 1 ) {
    throw "Lowered XLIR did not materialize the exact language-integer to c_bool conversion"
}

Invoke-Xl @("cgen", $foreignBoolSource, "--backend", "xlir", "--emit", $foreignBoolCPath) `
    "XLIR foreign bool operator Cgen failed"
$foreignBoolCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c `"$foreignBoolCPath`" -o `"$foreignBoolObjectPath`""
& $env:ComSpec /d /v:on /c $foreignBoolCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR foreign bool operator strict-C object gate failed"
}

Invoke-Xl @("ir", $languageBoolSource, "--format", "json", "--emit", $languageBoolTypedPath) `
    "XLIR language bool/integer operator Typed IR failed"
Invoke-Xl @("ir", $languageBoolSource, "--lowered", "--format", "json", "--emit", $languageBoolLoweredPath) `
    "XLIR language bool/integer operator lowering failed"
$languageBoolTyped = Read-Json $languageBoolTypedPath
$languageBoolLowered = Read-Json $languageBoolLoweredPath
$languageBoolCompare = @($languageBoolTyped.nodes | Where-Object {
    $_.line -eq 30 -and $_.sourceKindName -eq "binary" -and
    $_.opcodeName -eq "eq"
})
if ( $languageBoolCompare.Count -ne 1 -or
     $languageBoolCompare[0].operatorOperand0AbiName -ne "scalar-convert" -or
     $languageBoolCompare[0].operatorOperand1AbiName -ne "exact" -or
     @($languageBoolTyped.types | Where-Object {
         $_.id -eq $languageBoolCompare[0].operatorExecutionType -and
         $_.name -eq "int" -and $_.integerTargetName -eq "i64" -and
         $_.ownershipAbi -eq 1
     }).Count -ne 1 ) {
    throw "Typed XLIR did not fix language bool/integer equality to the integer execution domain"
}
if ( @($languageBoolLowered.instructions | Where-Object {
         $_.origin -eq $languageBoolCompare[0].id -and
         $_.kindName -eq "scalar-convert" -and
         $_.scalarConversionAbiName -eq "bool-to-integer" -and
         $_.type -eq $languageBoolCompare[0].operatorExecutionType
     }).Count -ne 1 ) {
    throw "Lowered XLIR did not materialize the exact language bool-to-integer comparison conversion"
}

Invoke-Xl @("cgen", $languageBoolSource, "--backend", "xlir", "--emit", $languageBoolCPath) `
    "XLIR language bool/integer operator Cgen failed"
$languageBoolCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c `"$languageBoolCPath`" -o `"$languageBoolObjectPath`""
& $env:ComSpec /d /v:on /c $languageBoolCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR language bool/integer operator strict-C object gate failed"
}

Invoke-Xl @("ir", $flowSource, "--format", "json", "--emit", $flowTypedPath) `
    "XLIR flow-refined operator Typed IR failed"
Invoke-Xl @("ir", $flowSource, "--lowered", "--format", "json", "--emit", $flowLoweredPath) `
    "XLIR flow-refined operator lowering failed"
$flowTyped = Read-Json $flowTypedPath
$flowLowered = Read-Json $flowLoweredPath
$flowAdd = @($flowTyped.nodes | Where-Object {
    $_.line -eq 28 -and $_.sourceKindName -eq "binary" -and
    $_.opcodeName -eq "add"
})
if ( $flowAdd.Count -ne 1 -or
     $flowAdd[0].operatorOperand0AbiName -ne "xvalue-project-exact" -or
     $flowAdd[0].operatorOperand1AbiName -ne "exact" -or
     @($flowTyped.types | Where-Object {
         $_.id -eq $flowAdd[0].operatorExecutionType -and
         $_.name -eq "int" -and $_.integerTargetName -eq "i64"
     }).Count -ne 1 ) {
    throw "Typed XLIR did not preserve the exact physical-var to logical-int operator path"
}
$flowNameId = @($flowTyped.nodeChildren)[$flowAdd[0].firstChild]
$flowName = @($flowTyped.nodes | Where-Object { $_.id -eq $flowNameId })
if ( $flowName.Count -ne 1 -or $flowName[0].type -eq $flowName[0].storageType -or
     @($flowLowered.instructions | Where-Object {
         $_.origin -eq $flowNameId -and $_.kindName -eq "runtime-call" -and
         $_.runtimeOpName -eq "xvalue-project-scalar-copy" -and
         $_.payloadType -eq $flowAdd[0].operatorExecutionType -and
         $_.type -eq $flowAdd[0].operatorExecutionType
     }).Count -ne 1 ) {
    throw "Lowered XLIR did not materialize the exact flow-refined xvalue scalar projection"
}

Invoke-Xl @("cgen", $flowSource, "--backend", "xlir", "--emit", $flowCPath) `
    "XLIR flow-refined operator Cgen failed"
Invoke-Xl @("cgen", $flowSource, "--backend", "xlir", "--emit", $flowAgainPath) `
    "XLIR flow-refined operator deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($flowCPath),
        [byte[]][System.IO.File]::ReadAllBytes($flowAgainPath)
    ) ) {
    throw "XLIR flow-refined operator C output is not deterministic"
}
$flowCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c `"$flowCPath`" -o `"$flowObjectPath`""
& $env:ComSpec /d /v:on /c $flowCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR flow-refined operator strict-C object gate failed"
}
Invoke-Xl @("run", $flowSource) "XLIR flow-refined operator runtime failed"

Write-Output "XLIR operator execution-type gate ok: u64, foreign and language bool/integer common domains, flow-refined physical/logical paths, explicit conversions, deterministic mechanical C, strict C, and runtime."
exit 0
