param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\extlib_plist.xl"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_plist_return_type_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_plist_return_type_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_plist_return_type_abi.c"
$generatedAgainPath = Join-Path $outputDir "xlir_plist_return_type_abi.again.c"

foreach ( $path in @($xl, $source) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR plist return/type ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Invoke-Xl @("ir", $source, "--typed", "--format", "json", "--emit", $typedPath) `
    "XLIR plist return/type Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$typeTests = @($typed.nodes | Where-Object {
    $_.sourceKindName -eq "type_is" -and $_.line -eq 220
})
if ( $typeTests.Count -ne 1 ) {
    throw "Typed plist UID guard does not contain one canonical TYPE_IS"
}
$typeTest = $typeTests[0]
$sourceNodeId = [int]$typed.nodeChildren[[int]$typeTest.firstChild]
$targetNodeId = [int]$typed.nodeChildren[[int]$typeTest.firstChild + 1]
$sourceNode = @($typed.nodes | Where-Object { $_.id -eq $sourceNodeId })[0]
$targetNode = @($typed.nodes | Where-Object { $_.id -eq $targetNodeId })[0]
$negation = @($typed.nodes | Where-Object { $_.id -eq $typeTest.parent })[0]
if ( $null -eq $sourceNode -or $sourceNode.kindName -ne "name" -or
     $sourceNode.storageType -eq 0 -or $null -eq $targetNode -or
     $targetNode.storageType -eq 0 -or $sourceNode.storageType -eq $targetNode.storageType -or
     $null -eq $negation -or $negation.kindName -ne "unary" -or
     $negation.opcodeName -ne "not" ) {
    throw "Typed plist UID guard lost var TYPE_IS UID followed by explicit boolean negation"
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR plist return/type lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$nominalTests = @($instructions | Where-Object {
    $_.sourceLine -eq 220 -and $_.runtimeOpName -eq "xvalue-nominal-test"
})
if ( $nominalTests.Count -ne 1 -or $nominalTests[0].operandCount -ne 1 -or
     $nominalTests[0].payloadType -ne $targetNode.storageType ) {
    throw "Lowered plist UID guard lost its exact dynamic source and nominal target TypeId"
}
$testIndex = [int]$nominalTests[0].index
if ( $testIndex + 2 -ge $instructions.Count -or
     $instructions[$testIndex + 1].kindName -ne "unary" -or
     $instructions[$testIndex + 1].opcodeName -ne "not" -or
     $instructions[$testIndex + 2].kindName -ne "branch" ) {
    throw "Lowered plist UID guard lost TYPE_IS -> NOT -> BRANCH ordering"
}

$transferReturnFound = $false
foreach ( $return in @($instructions | Where-Object {
    $_.sourceLine -eq 112 -and $_.kindName -eq "return" -and $_.operandCount -eq 1
}) ) {
    $returnedValue = [int]@($return.operands)[0]
    $rebind = @($instructions | Where-Object { $_.result -eq $returnedValue })
    if ( $rebind.Count -ne 1 -or $rebind[0].kindName -ne "value" -or
         $rebind[0].operandCount -ne 1 ) { continue }
    $loadedValue = [int]@($rebind[0].operands)[0]
    $load = @($instructions | Where-Object { $_.result -eq $loadedValue })
    if ( $load.Count -ne 1 -or $load[0].kindName -ne "load" -or
         $load[0].targetSymbol -eq 0 -or $load[0].type -eq $rebind[0].type ) { continue }
    $release = @($instructions | Where-Object {
        $_.index -gt $rebind[0].index -and $_.index -lt $return.index -and
        $_.kindName -eq "release" -and
        $_.targetSymbol -eq $load[0].targetSymbol -and $_.type -eq $load[0].type
    })
    $move = @($instructions | Where-Object {
        $_.index -gt $rebind[0].index -and $_.index -lt $return.index -and
        $_.kindName -eq "move" -and
        $_.targetSymbol -eq $load[0].targetSymbol -and
        $_.type -eq $load[0].type -and $_.operandCount -eq 1 -and
        [int]@($_.operands)[0] -eq $loadedValue
    })
    $retain = @($instructions | Where-Object {
        $_.index -gt $load[0].index -and $_.index -lt $return.index -and
        $_.runtimeOpName -eq "xvalue-retain" -and
        $_.operandCount -eq 1 -and [int]@($_.operands)[0] -eq $loadedValue
    })
    if ( $release.Count -eq 0 -and $move.Count -eq 1 -and $retain.Count -eq 0 ) {
        $transferReturnFound = $true
        break
    }
}
if ( !$transferReturnFound ) {
    throw "Lowered Buffer -> var return lost LOAD/rebind/source-MOVE ownership transfer"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR plist return/type Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR plist return/type deterministic Cgen failed"
if ( (Get-FileHash -Algorithm SHA256 -LiteralPath $generatedPath).Hash -ne
     (Get-FileHash -Algorithm SHA256 -LiteralPath $generatedAgainPath).Hash ) {
    throw "XLIR plist return/type Cgen is not deterministic"
}

Invoke-Xl @("run", $source) "Default XLIR plist source runtime failed"

Write-Output "XLIR plist ABI gate ok: UID TYPE_IS is parenthesized and nominal, owned Buffer -> var return uses one ownership-preserving rebind plus exact source-storage MOVE, deterministic strict Cgen passes, and default XLIR runtime passes."
