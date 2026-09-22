param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\nullable_string_lifecycle.xl"
$programHost = Join-Path $Root "test\cases\xlir_class_static_storage_host.c"
$support = Join-Path $Root "test\cases\cgen_class_host.c"
$outputDir = Join-Path $Root "build\tmp"
$loweredJson = Join-Path $outputDir "nullable_string_lifecycle.lowered.json"
$generatedC = Join-Path $outputDir "nullable_string_lifecycle.xlir.c"
$generatedCSecond = Join-Path $outputDir "nullable_string_lifecycle.xlir.second.c"

foreach ( $path in @($xl, $source, $programHost, $support) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR nullable-string lifecycle ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

function Get-NodeById($document, $id) {
    return @($document.nodes | Where-Object { $_.id -eq $id })[0]
}

function Get-DefinitionByValue($document, $value) {
    return @($document.instructions | Where-Object { $_.result -eq $value })[0]
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR nullable-string lifecycle lowering failed"
}
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$canonicalNullableStrings = @($lowered.types | Where-Object {
    $candidate = $_
    $payload = if ( @($candidate.params).Count -eq 1 ) {
        Get-TypeById $lowered $candidate.params[0]
    } else { $null }
    $candidate.name -eq "nullable" -and
    $candidate.cAbi -eq 25 -and $candidate.ownershipAbi -eq 2 -and
    $candidate.nullAbi -eq 1 -and $null -ne $payload -and
    $payload.name -eq "string" -and $payload.cAbi -eq 25
})
if ( $canonicalNullableStrings.Count -ne 1 ) {
    throw "XLIR nullable-string TypePool is not structurally canonical: $($canonicalNullableStrings.Count) TypeIds"
}

$nullableMembers = @($lowered.nodes | Where-Object {
    $_.memberAbiName -eq "class-field-nullable-string-copy"
})
if ( $nullableMembers.Count -ne 3 ) {
    throw "XLIR nullable class-field MemberAbi coverage is incomplete: $($nullableMembers.Count)"
}
foreach ( $member in $nullableMembers ) {
    $type = Get-TypeById $lowered $member.storageType
    if ( $null -eq $type -or $type.name -ne "nullable" -or
         $type.cAbi -ne 25 -or $type.ownershipAbi -ne 2 -or
         $type.nullAbi -ne 1 -or @($type.params).Count -ne 1 ) {
        throw "XLIR nullable class field has no exact pointer-sentinel managed-string TypeId"
    }
}

$fieldProjections = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-nullable-string-copy" -and
    (Get-NodeById $lowered $_.origin).memberAbiName -eq
        "class-field-nullable-string-copy"
})
if ( $fieldProjections.Count -ne 1 ) {
    throw "XLIR nullable class-field read has no unique owned projection sequence"
}
$fieldProjection = $fieldProjections[0]
$fieldLookup = Get-DefinitionByValue $lowered $fieldProjection.operands[0]
$fieldReceiver = if ( $null -ne $fieldLookup ) {
    Get-DefinitionByValue $lowered $fieldLookup.operands[0]
} else { $null }
$fieldReceiverType = if ( $null -ne $fieldReceiver ) {
    Get-TypeById $lowered $fieldReceiver.type
} else { $null }
if ( $null -eq $fieldLookup -or
     $fieldLookup.runtimeOpName -ne "xvalue-object-get-literal" -or
     $fieldLookup.origin -ne $fieldProjection.origin -or
     $fieldLookup.hasImmediate -ne 1 -or
     $null -eq $fieldReceiverType -or $fieldReceiverType.kind -ne 2 -or
     $fieldReceiverType.cAbi -ne 26 ) {
    throw "XLIR nullable class field did not lower to exact object lookup plus nullable-string copy"
}

$nullableCopies = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "string-nullable-copy"
})
if ( $nullableCopies.Count -lt 9 ) {
    throw "XLIR nullable-string owning boundaries lost explicit nullable copies: $($nullableCopies.Count)"
}
foreach ( $copy in $nullableCopies ) {
    $type = Get-TypeById $lowered $copy.type
    $operand = Get-DefinitionByValue $lowered $copy.operands[0]
    if ( $copy.operandCount -ne 1 -or $null -eq $type -or
         $type.name -ne "nullable" -or $type.cAbi -ne 25 -or
         $type.ownershipAbi -ne 2 -or $null -eq $operand -or
         $operand.type -ne $copy.type ) {
        throw "XLIR nullable-string copy has no exact same-TypeId owned boundary"
    }
}
foreach ( $copy in @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "string-copy"
}) ) {
    if ( (Get-TypeById $lowered $copy.type).name -eq "nullable" ) {
        throw "XLIR nullable string regressed to the non-null string-copy operation"
    }
}

$nullCount = @($lowered.instructions | Where-Object { $_.kindName -eq "null" }).Count
$wrapCount = @($lowered.instructions | Where-Object { $_.kindName -eq "nullable-wrap" }).Count
$unwrapCount = @($lowered.instructions | Where-Object { $_.kindName -eq "nullable-unwrap" }).Count
$testCount = @($lowered.instructions | Where-Object { $_.kindName -eq "null-test" }).Count
$nullableNulls = @($lowered.instructions | Where-Object {
    $_.kindName -eq "null" -and $_.type -eq $canonicalNullableStrings[0].id
})
$nullableFieldDefaults = @($lowered.classFields | Where-Object {
    $_.type -eq $canonicalNullableStrings[0].id -and
    $_.initializerAbiName -eq "null-sentinel" -and
    $_.initializerErrorAbiName -eq "no-raise" -and $_.initializer -ne 0
})
if ( $nullCount -ne 3 -or $nullableNulls.Count -ne 3 -or
     $nullableFieldDefaults.Count -ne 1 -or $wrapCount -lt 6 -or
     $unwrapCount -lt 6 -or $testCount -lt 11 ) {
    throw "XLIR nullable string executable-null/field-default/wrap/unwrap/coalesce ABI is incomplete: null=$nullCount fieldDefault=$($nullableFieldDefaults.Count) wrap=$wrapCount unwrap=$unwrapCount test=$testCount"
}

$captureReturn = @($lowered.nodes | Where-Object {
    $_.kindName -eq "return" -and $_.line -eq 57
})[0]
$captureReturnInstructions = @($lowered.instructions | Where-Object {
    $null -ne $captureReturn -and $_.origin -eq $captureReturn.id
})
if ( $null -eq $captureReturn -or
     @($captureReturnInstructions | Where-Object {
         $_.runtimeOpName -eq "string-nullable-copy"
     }).Count -ne 1 -or
     @($captureReturnInstructions | Where-Object {
         $_.kindName -eq "move"
     }).Count -ne 0 ) {
    throw "XLIR closure capture return is not an explicit copy-only ownership boundary"
}

$constructor = @($lowered.nodes | Where-Object {
    $_.kindName -eq "call" -and $_.line -eq 43 -and
    (Get-TypeById $lowered $_.storageType).name -eq "NullableTextBox"
})[0]
$constructorCall = @($lowered.instructions | Where-Object {
    $null -ne $constructor -and $_.origin -eq $constructor.id -and
    $_.kindName -eq "call"
})[0]
$constructorArgument = if ( $null -ne $constructorCall -and
                            $constructorCall.operandCount -eq 2 ) {
    Get-DefinitionByValue $lowered $constructorCall.operands[1]
} else { $null }
$constructorArgumentStorage = if ( $null -ne $constructorArgument ) {
    @($lowered.storages | Where-Object {
        $_.symbol -eq $constructorArgument.targetSymbol
    })[0]
} else { $null }
if ( $null -eq $constructorArgument -or
     $constructorArgument.kindName -ne "load" -or
     $null -eq $constructorArgumentStorage -or
     $constructorArgumentStorage.kindName -ne "global" ) {
    throw "XLIR constructor borrowed nullable-string lvalue was wrapped in an owning temporary"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR nullable-string lifecycle C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR nullable-string lifecycle repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR nullable-string lifecycle generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_object_get_literal_or_null",
    "xlir_xvalue_project_nullable_string_copy",
    "!= NULL ? xrtStrDup"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR nullable-string lifecycle generated C lost mechanical ownership emission: $required"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror build\tmp\nullable_string_lifecycle.xlir.c test\cases\xlir_class_static_storage_host.c test\cases\cgen_class_host.c !XL6_CGEN_LINKS! -o build\test\nullable_string_lifecycle.xlir.exe && build\test\nullable_string_lifecycle.xlir.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR nullable-string lifecycle strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR nullable-string lifecycle ABI gate passed: one structurally canonical TypeId, exact null/wrap/coalesce CFG, nullable parameter/capture ownership, class-field projection, constructor borrow, deterministic C, and strict runtime."
