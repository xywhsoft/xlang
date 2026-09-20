param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_borrowed_call_provenance.xl"
$moduleHost = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\test"
foreach ( $path in @($xl, $source, $moduleHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR borrowed-call provenance gate input is missing: $path"
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

function Get-ValueDefinition($Ir, [uint64]$Value)
{
    $definitions = @($Ir.instructions | Where-Object {
        [uint64]$_.result -eq $Value -and $Value -ne 0
    })
    if ( $definitions.Count -ne 1 ) {
        throw "XLIR value v$Value has no unique instruction definition"
    }
    return $definitions[0]
}

$loweredPath = Join-Path $outputDir "xlir_borrowed_call_provenance.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_borrowed_call_provenance.xlir.c"
$repeatPath = Join-Path $outputDir "xlir_borrowed_call_provenance.xlir.repeat.c"
$legacyPath = Join-Path $outputDir "xlir_borrowed_call_provenance.legacy.c"

Push-Location $Root
try {
    Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
        "XLIR borrowed-call provenance lowering failed"
    $ir = [System.IO.File]::ReadAllText(
        $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

    $invokeDeclaration = @($ir.nodes | Where-Object {
        $_.line -eq 13 -and $_.kindName -eq "function-declaration" -and
        $_.sourceKindName -eq "func"
    })
    $invokeTargets = @()
    if ( $invokeDeclaration.Count -eq 1 ) {
        $invokeTargets = @($ir.callTargets | Where-Object {
            $_.declaration -eq $invokeDeclaration[0].id
        })
    }
    if ( $invokeTargets.Count -ne 1 ) {
        throw "Nullable callback consumer has no unique CallTarget"
    }
    $invokeTarget = $invokeTargets[0]
    if ( $invokeTarget.parameterCount -ne 2 -or
         @($invokeTarget.parameterAbis).Count -ne 2 -or
         $invokeTarget.parameterAbis[0] -ne 2 ) {
        throw "Nullable callback consumer lost its exact borrowed CallTarget parameter ABI"
    }

    foreach ( $line in @(28, 29) ) {
        $callNode = @($ir.nodes | Where-Object {
            $_.line -eq $line -and $_.kindName -eq "call" -and
            $_.targetSymbol -eq $invokeTarget.symbol
        })
        $calls = @()
        if ( $callNode.Count -eq 1 ) {
            $calls = @($ir.instructions | Where-Object {
                $_.origin -eq $callNode[0].id -and $_.kindName -eq "call" -and
                $_.targetSymbol -eq $invokeTarget.symbol
            })
        }
        if ( $calls.Count -ne 1 -or @($calls[0].operands).Count -ne 2 ) {
            throw "Nullable callback call has no unique canonical CALL at line $line"
        }
        $wrap = Get-ValueDefinition $ir ([uint64]$calls[0].operands[0])
        $sourceLoad = $null
        if ( $wrap.kindName -eq "nullable-wrap" -and
             @($wrap.operands).Count -eq 1 ) {
            $sourceLoad = Get-ValueDefinition $ir ([uint64]$wrap.operands[0])
        }
        if ( $null -eq $sourceLoad -or $sourceLoad.kindName -ne "load" -or
             $wrap.type -ne $invokeTarget.parameterTypes[0] ) {
            throw "Borrowed nullable callback provenance is not transparent LOAD -> NULLABLE_WRAP -> CALL at line $line"
        }
    }

    $nullableCallNode = @($ir.nodes | Where-Object {
        $_.line -eq 17 -and $_.kindName -eq "call" -and
        $_.callKindName -eq "function-value"
    })
    $nullableClosureCalls = @()
    if ( $nullableCallNode.Count -eq 1 ) {
        $nullableClosureCalls = @($ir.instructions | Where-Object {
            $_.origin -eq $nullableCallNode[0].id -and
            $_.kindName -eq "closure-call"
        })
    }
    if ( $nullableClosureCalls.Count -ne 1 -or
         @($nullableClosureCalls[0].operands).Count -lt 1 -or
         (Get-ValueDefinition $ir ([uint64]$nullableClosureCalls[0].operands[0])).kindName -ne
             "nullable-unwrap" ) {
        throw "Flow-refined nullable callback invocation lost its explicit NULLABLE_UNWRAP -> CLOSURE_CALL ABI"
    }

    $compound = @($ir.nodes | Where-Object {
        $_.line -eq 30 -and $_.kindName -eq "assign" -and
        $_.opcodeName -eq "add"
    })
    $members = @()
    if ( $compound.Count -eq 1 ) {
        $members = @($ir.nodes | Where-Object {
            $_.parent -eq $compound[0].id -and $_.kindName -eq "member" -and
            $_.memberAbiName -eq "class-field-i64"
        })
    }
    if ( $members.Count -ne 1 ) {
        throw "Rvalue receiver compound assignment lost its exact class-field member ABI"
    }
    $member = $members[0]
    $instructions = @($ir.instructions)
    $get = @($instructions | Where-Object {
        $_.origin -eq $member.id -and
        $_.runtimeOpName -eq "xvalue-object-get-literal-i64"
    })
    $binary = @($instructions | Where-Object {
        $_.origin -eq $compound[0].id -and $_.kindName -eq "binary" -and
        $_.opcodeName -eq "add"
    })
    $set = @($instructions | Where-Object {
        $_.origin -eq $compound[0].id -and
        $_.runtimeOpName -eq "xvalue-object-set-literal-new"
    })
    if ( $get.Count -ne 1 -or $binary.Count -ne 1 -or $set.Count -ne 1 -or
         @($get[0].operands).Count -ne 1 ) {
        throw "Class-field compound assignment is not one explicit receiver-anchor/GET/BINARY/SET transaction"
    }
    $receiverLoad = Get-ValueDefinition $ir ([uint64]$get[0].operands[0])
    $receiverStores = @($instructions | Where-Object {
        $_.origin -eq $compound[0].id -and $_.kindName -eq "store" -and
        [uint64]$_.targetSymbol -eq [uint64]$receiverLoad.targetSymbol
    })
    if ( $receiverLoad.kindName -ne "load" -or
         $receiverStores.Count -ne 1 ) {
        throw "Class-field compound assignment receiver has no unique temporary STORE/LOAD anchor"
    }
    $receiverStore = $receiverStores[0]
    $receiverReleases = @($instructions | Where-Object {
        $_.kindName -eq "release" -and
        [uint64]$_.targetSymbol -eq [uint64]$receiverStore.targetSymbol
    })
    if ( $receiverReleases.Count -ne 1 -or
         @($set[0].operands).Count -ne 2 -or
         $set[0].operands[0] -ne $receiverLoad.result -or
         @($binary[0].operands) -notcontains $get[0].result ) {
        throw "Class-field read and write do not share one anchored receiver SSA value"
    }
    $storeIndex = [Array]::IndexOf($instructions, $receiverStore)
    $loadIndex = [Array]::IndexOf($instructions, $receiverLoad)
    $getIndex = [Array]::IndexOf($instructions, $get[0])
    $binaryIndex = [Array]::IndexOf($instructions, $binary[0])
    $setIndex = [Array]::IndexOf($instructions, $set[0])
    $releaseIndex = [Array]::IndexOf($instructions, $receiverReleases[0])
    if ( !($storeIndex -lt $loadIndex -and $loadIndex -lt $getIndex -and
           $getIndex -lt $binaryIndex -and $binaryIndex -lt $setIndex -and
           $setIndex -lt $releaseIndex) ) {
        throw "Class-field receiver lifetime does not enclose the complete read-modify-write transaction"
    }

    Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
        "XLIR borrowed-call provenance Cgen failed"
    Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $repeatPath) `
        "XLIR borrowed-call provenance repeat Cgen failed"
    if ( ! [System.Linq.Enumerable]::SequenceEqual(
            [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
            [byte[]][System.IO.File]::ReadAllBytes($repeatPath)
        ) ) {
        throw "XLIR borrowed-call provenance C is not byte-for-byte deterministic"
    }
    Invoke-Xl @("cgen", $source, "--backend", "legacy", "--emit", $legacyPath) `
        "Legacy borrowed-call provenance differential Cgen failed"

    Invoke-CheckedCmd `
        "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\test\xlir_borrowed_call_provenance.xlir.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\test\xlir_borrowed_call_provenance.xlir.exe && build\test\xlir_borrowed_call_provenance.xlir.exe" `
        "XLIR borrowed-call provenance strict-C/runtime gate failed"
    Invoke-CheckedCmd `
        "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror build\test\xlir_borrowed_call_provenance.legacy.c !XL6_CGEN_LINKS! -o build\test\xlir_borrowed_call_provenance.legacy.exe && build\test\xlir_borrowed_call_provenance.legacy.exe" `
        "Legacy borrowed-call provenance differential runtime failed"

    foreach ( $integration in @(
        "extlib_hash_progress", "extlib_hash_progress_cancel"
    ) ) {
        $integrationSource = Join-Path $Root "test\cases\$integration.xl"
        $integrationC = Join-Path $outputDir "$integration.borrowed-provenance.xlir.c"
        Invoke-Xl @("cgen", $integrationSource, "--backend", "xlir", "--emit", $integrationC) `
            "XLIR nullable callback integration Cgen failed: $integration"
        Invoke-CheckedCmd `
            "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c build\test\$integration.borrowed-provenance.xlir.c -o build\test\$integration.borrowed-provenance.xlir.o" `
            "XLIR nullable callback integration strict-C compile failed: $integration"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR borrowed-call provenance gate passed: nullable LOAD/WRAP/CALL and UNWRAP/CLOSURE_CALL chains are verifier-visible, rvalue class receivers own one STORE/LOAD/GET/BINARY/SET/RELEASE transaction, deterministic strict XLIR/legacy runtimes agree, and hash progress integrations compile warning-free."
