param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_native_class_property.xl"
$moduleHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$support = Join-Path $Root "test\cases\xlir_resolved_property_call_abi_support.c"
$outputDir = Join-Path $Root "build\test"
$loweredPath = Join-Path $outputDir "xlir_resolved_property_call_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_resolved_property_call_abi.generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_resolved_property_call_abi.generated_again.c"
$xlirExe = Join-Path $outputDir "xlir_resolved_property_call_abi.xlir.exe"

foreach ( $path in @($xl, $source, $moduleHost, $support) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR resolved property CallTarget gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR resolved property CallTarget lowering failed"
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$specs = @(
    @{ Name = "native_property_get_value"; Return = "value"; Parameters = @("receiver"); Calls = 4; CallKind = "property-get" },
    @{ Name = "native_property_set_value"; Return = "void"; Parameters = @("receiver", "value"); Calls = 1; CallKind = "property-set" },
    @{ Name = "native_property_get_at"; Return = "value"; Parameters = @("receiver", "value"); Calls = 1; CallKind = "property-get" },
    @{ Name = "native_property_set_at"; Return = "void"; Parameters = @("receiver", "value", "value"); Calls = 1; CallKind = "property-set" },
    @{ Name = "native_property_get_cell"; Return = "value"; Parameters = @("receiver", "value", "value"); Calls = 1; CallKind = "property-get" },
    @{ Name = "native_property_set_cell"; Return = "void"; Parameters = @("receiver", "value", "value", "value"); Calls = 1; CallKind = "property-set" }
)
$propertyOrigins = @()
$propertySemanticOrigins = @()
$physicalProjectionInstructions = @()
$receiverType = 0
$valueType = 0
foreach ( $spec in $specs ) {
    $targets = @($lowered.callTargets | Where-Object {
        $_.kind -eq 2 -and $_.cName -eq $spec.Name
    })
    if ( $targets.Count -ne 1 ) {
        throw "Resolved property has no unique extern C CallTarget: $($spec.Name)"
    }
    $target = $targets[0]
    if ( $spec.Name -eq "native_property_get_value" ) {
        $receiverType = [int]$target.parameterTypes[0]
        $valueType = [int]$target.returnType
    }
    $parameterTypes = @($target.parameterTypes)
    $parameterAbis = @($target.parameterAbis)
    $parameterPhysicalAbis = @($target.parameterPhysicalAbis)
    $parameterPhysicalFields = @($target.parameterPhysicalFieldDeclarations)
    if ( $target.errorAbiName -ne "may-raise" -or
         $target.errorPhysicalAbiName -ne "foreign-scope" -or
         $target.parameterCount -ne $spec.Parameters.Count -or
         $parameterTypes.Count -ne $spec.Parameters.Count -or
         $parameterAbis.Count -ne $spec.Parameters.Count -or
         $parameterPhysicalAbis.Count -ne $spec.Parameters.Count -or
         $parameterPhysicalFields.Count -ne $spec.Parameters.Count -or
        ($spec.Return -eq "void" -and $target.returnType -ne 0) -or
        ($spec.Return -eq "value" -and $target.returnType -ne $valueType) ) {
        throw "Resolved property CallTarget signature is not exact: $($spec.Name)"
    }
    for ( $index = 0; $index -lt $spec.Parameters.Count; ++$index ) {
        $expectedAbi = if ( $spec.Parameters[$index] -eq "receiver" ) { 2 } else { 1 }
        if ( $parameterTypes[$index] -eq 0 -or $parameterAbis[$index] -ne $expectedAbi ) {
            throw "Resolved property CallTarget lost receiver/value ABI: $($spec.Name)[$index]"
        }
        $expectedType = if ( $index -eq 0 ) { $receiverType } else { $valueType }
        if ( $parameterTypes[$index] -ne $expectedType ) {
            throw "Resolved property parameter TypeId is not exact: $($spec.Name)[$index]"
        }
        $expectedPhysicalAbi = if ( $index -eq 0 ) {
            "native-class-handle-borrow"
        } else {
            "direct"
        }
        if ( $parameterPhysicalAbis[$index] -ne $expectedPhysicalAbi -or
             ($index -eq 0 -and $parameterPhysicalFields[$index] -eq 0) -or
             ($index -ne 0 -and $parameterPhysicalFields[$index] -ne 0) ) {
            throw "Resolved property parameter physical ABI is not exact: $($spec.Name)[$index]"
        }
    }
    $calls = @($lowered.instructions | Where-Object {
        $_.kindName -eq "call" -and $_.targetSymbol -eq $target.symbol
    })
    if ( $calls.Count -ne $spec.Calls -or
         @($calls | Where-Object {
             $_.operandCount -ne $target.parameterCount -or
             @($_.operands).Count -ne $target.parameterCount -or
             ($target.returnType -eq 0 -and ($_.result -eq 0 -or $_.type -ne 0)) -or
             ($target.returnType -ne 0 -and ($_.result -eq 0 -or $_.type -ne $target.returnType))
         }).Count -ne 0 ) {
        throw "Resolved property did not lower to exact direct CALL instructions: $($spec.Name)"
    }
    foreach ( $call in $calls ) {
        $outcome = $lowered.instructions[[int]$call.index + 1]
        if ($outcome.kindName -ne 'error-test' -or $outcome.operands[0] -ne $call.result -or
            $outcome.targetSymbol -ne $target.symbol -or $outcome.trueTarget -eq $outcome.falseTarget) {
            throw "Property outcome must precede receiver cleanup: $($spec.Name)"
        }
        $origins = @($lowered.nodes | Where-Object { $_.id -eq $call.origin })
        if ( $origins.Count -ne 1 ) {
            throw "Property CALL is not owned by its semantic property-get/property-set origin: $($spec.Name)"
        }
        # Indexed properties are semantically owned by the INDEX node itself;
        # its MEMBER child only preserves source shape and must not be chased.
        $semanticOrigins = $origins
        if ( $semanticOrigins[0].callKindName -ne $spec.CallKind -or
             $semanticOrigins[0].targetSymbol -ne $target.symbol ) {
            throw "Property CALL lost its resolved semantic target: $($spec.Name)"
        }
        $field = @($lowered.classFields | Where-Object {
            $_.declaration -eq $parameterPhysicalFields[0]
        })
        $projection = @($lowered.instructions | Where-Object {
            $_.result -eq [int]$call.operands[0]
        })
        if ( $field.Count -ne 1 -or $projection.Count -ne 1 -or
             $projection[0].kindName -ne "runtime-call" -or
             $projection[0].runtimeOpName -ne
                 "xvalue-object-get-literal-native-handle-borrow" -or
             $projection[0].origin -ne $semanticOrigins[0].id -or
             $projection[0].type -ne $field[0].type -or
             $projection[0].operandCount -ne 1 ) {
            throw "Property receiver did not cross its exact managed-field physical ABI: $($spec.Name)"
        }
        $physicalObject = @($lowered.instructions | Where-Object {
            $_.result -eq [int]$projection[0].operands[0]
        })
        if ( $physicalObject.Count -ne 1 -or
             $physicalObject[0].kindName -ne "load" -or
             $physicalObject[0].type -ne $receiverType -or
             $physicalObject[0].targetSymbol -eq 0 ) {
            throw "Property physical receiver projection lost its managed object operand: $($spec.Name)"
        }
        $receiverTemporary = [long]$physicalObject[0].targetSymbol
        $receiverStores = @($lowered.instructions | Where-Object {
            $_.kindName -eq "store" -and $_.targetSymbol -eq $receiverTemporary -and
            $_.type -eq $receiverType -and $_.operandCount -eq 1
        })
        $receiverRetains = @(if ( $receiverStores.Count -eq 1 ) {
            $retainedValue = [int]$receiverStores[0].operands[0]
            @($lowered.instructions | Where-Object {
                $_.result -eq $retainedValue -and $_.kindName -eq "runtime-call" -and
                $_.runtimeOpName -eq "xvalue-retain" -and
                $_.type -eq $receiverType -and $_.operandCount -eq 1
            })
        } else { @() })
        $receiverReleases = @($lowered.instructions | Where-Object {
            $_.kindName -eq "release" -and $_.targetSymbol -eq $receiverTemporary -and
            $_.type -eq $receiverType
        })
        if ( $receiverStores.Count -ne 1 -or $receiverRetains.Count -ne 1 -or
             $receiverReleases.Count -ne 2 -or
             @($receiverReleases | Where-Object errorScope -eq 0).Count -ne 0 -or
             @($receiverReleases | Where-Object errorScope -ne 0).Count -ne 2 -or
             $receiverRetains[0].id -ge $receiverStores[0].id -or
             $receiverStores[0].id -ge $physicalObject[0].id -or
             $physicalObject[0].id -ge $projection[0].id -or
             $projection[0].id -ge $call.id -or
             $call.id -ge $receiverReleases[0].id ) {
            throw "Property managed receiver has no exact retain/store/load/project/call/release lifetime transaction: $($spec.Name)"
        }
        $success = @($lowered.blocks | Where-Object id -eq $outcome.trueTarget)[0]
        $failure = @($lowered.blocks | Where-Object id -eq $outcome.falseTarget)[0]
        $successBegin = $lowered.instructions[[int]$success.firstInstruction]
        $successRelease = $lowered.instructions[[int]$success.firstInstruction + 1]
        $successEnd = $lowered.instructions[[int]$success.firstInstruction + 2]
        $cleanupOutcome = $lowered.instructions[[int]$success.firstInstruction + 3]
        $failureBegin = $lowered.instructions[[int]$failure.firstInstruction]
        $failureRelease = $lowered.instructions[[int]$failure.firstInstruction + 1]
        if ($successBegin.kindName -ne 'cleanup-begin' -or
            $successRelease.kindName -ne 'release' -or $successRelease.targetSymbol -ne $receiverTemporary -or
            $successRelease.errorScope -ne $successBegin.errorScope -or
            $successEnd.kindName -ne 'cleanup-end' -or $successEnd.errorScope -ne $successBegin.errorScope -or
            $cleanupOutcome.kindName -ne 'error-test' -or $cleanupOutcome.operands[0] -ne $successEnd.result -or
            $failureBegin.kindName -ne 'error-unwind-begin' -or
            $failureRelease.kindName -ne 'release' -or $failureRelease.targetSymbol -ne $receiverTemporary -or
            $failureRelease.errorScope -ne $failureBegin.errorScope) {
            throw "Property receiver needs exclusive success and protected failure cleanup: $($spec.Name)"
        }
        $propertyOrigins += $call.origin
        $propertySemanticOrigins += $semanticOrigins[0].id
        $physicalProjectionInstructions += @($receiverRetains[0].id, $projection[0].id)
    }
}

if ( @($lowered.instructions | Where-Object {
        $_.origin -in ($propertyOrigins + $propertySemanticOrigins) -and
        $_.runtimeOpName -ne "none" -and
        $_.id -notin $physicalProjectionInstructions
     }).Count -ne 0 ) {
    throw "Resolved native property semantics emitted a RuntimeOp outside its declared physical receiver projection"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR resolved property CallTarget Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR resolved property deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR resolved property generated C is not deterministic"
}
$generated = [System.IO.File]::ReadAllText(
    $generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    @{ Pattern = '(?m)^\s+v\d+ = native_property_get_value\(v\d+\);$'; Count = 4; Name = "get value" },
    @{ Pattern = '(?m)^\s+native_property_set_value\(v\d+, v\d+\);$'; Count = 1; Name = "set value" },
    @{ Pattern = '(?m)^\s+v\d+ = native_property_get_at\(v\d+, v\d+\);$'; Count = 1; Name = "get indexed value" },
    @{ Pattern = '(?m)^\s+native_property_set_at\(v\d+, v\d+, v\d+\);$'; Count = 1; Name = "set indexed value" },
    @{ Pattern = '(?m)^\s+v\d+ = native_property_get_cell\(v\d+, v\d+, v\d+\);$'; Count = 1; Name = "get multi-index value" },
    @{ Pattern = '(?m)^\s+native_property_set_cell\(v\d+, v\d+, v\d+, v\d+\);$'; Count = 1; Name = "set multi-index value" }
) ) {
    if ( [regex]::Matches($generated, $required.Pattern).Count -ne $required.Count ) {
        throw "XLIR property Cgen is missing exact direct extern calls: $($required.Name)"
    }
}
if ( [regex]::Matches(
        $generated,
        '(?m)^\s+v\d+ = xlir_xvalue_object_get_literal_native_handle\(v\d+, xrtStrView\("handle"\)\);$'
     ).Count -ne 9 ) {
    throw "XLIR property Cgen did not materialize one exact native receiver projection per property call"
}
foreach ( $forbidden in @(
    "xrtObjectGet", "xrtObjectSet", "xlir_dynamic_member", "xlir_dynamic_index"
) ) {
    if ( $generated.Contains($forbidden) ) {
        throw "XLIR property Cgen guessed a dynamic member/index ABI: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $generatedPath -include $support $moduleHost !XL6_CGEN_LINKS! -o $xlirExe && $xlirExe"
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR resolved property strict generated-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR resolved property CallTarget ABI gate ok: exact extern C getter/setter targets, explicit managed-field native receiver projection plus ordered scalar index/value TypeIds, direct member/single-index/multi-index CALLs, and deterministic strict-C runtime semantics."
exit 0
