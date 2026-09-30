param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_class_field_inherited_narrowed_abi.xl"
$support = Join-Path $Root "test\cases\cgen_class_host.c"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_class_field_inherited_narrowed_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_class_field_inherited_narrowed_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_class_field_inherited_narrowed_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_class_field_inherited_narrowed_abi.second.c"

foreach ( $path in @($xl, $source, $support, $genericHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR inherited/narrowed class-field ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-NodeById($document, $id) {
    return @($document.nodes | Where-Object { $_.id -eq $id })[0]
}

function Get-DefinitionByValue($document, $value) {
    return @($document.instructions | Where-Object { $_.result -eq $value })[0]
}

& $xl ir $source --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR inherited/narrowed class-field typed IR failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$childLayout = @($typed.classLayouts | Where-Object { $_.baseType -ne 0 })[0]
if ( $null -eq $childLayout -or $childLayout.baseDeclaration -eq 0 ) {
    throw "Typed XLIR class layout has no explicit derived-to-base identity"
}
$members = @($typed.nodes | Where-Object {
    $_.memberAbiName -eq "class-field-i64" -or
    $_.memberAbiName -eq "class-field-string-copy"
})
if ( @($members | Where-Object {
        $_.memberAbiName -eq "class-field-i64"
    }).Count -ne 2 -or
     @($members | Where-Object {
        $_.memberAbiName -eq "class-field-string-copy"
    }).Count -ne 2 ) {
    throw "Typed XLIR inherited/narrowed class-field MemberAbi matrix is incomplete"
}
$narrowedMembers = 0
foreach ( $member in $members ) {
    $receiverId = $typed.nodeChildren[$member.firstChild]
    $receiver = Get-NodeById $typed $receiverId
    if ( $null -eq $receiver ) {
        throw "Typed XLIR class-field member has no receiver node"
    }
    if ( $receiver.type -ne $receiver.storageType ) {
        ++$narrowedMembers
        if ( $receiver.type -ne $childLayout.type ) {
            throw "Typed XLIR narrowed class-field receiver does not carry the exact semantic child TypeId"
        }
    }
}
if ( $narrowedMembers -ne 2 ) {
    throw "Typed XLIR flow-refined nullable class-field coverage is incomplete: $narrowedMembers"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR inherited/narrowed class-field lowering failed"
}
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
foreach ( $operationName in @(
    "xvalue-object-get-literal-i64",
    "xvalue-object-get-literal-string-copy"
) ) {
    $operations = @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq $operationName
    })
    if ( $operations.Count -ne 2 ) {
        throw "Lowered XLIR inherited/narrowed class-field operation count is incomplete for $operationName"
    }
    $sourceKinds = @{}
    foreach ( $operation in $operations ) {
        $origin = Get-NodeById $lowered $operation.origin
        $definition = Get-DefinitionByValue $lowered $operation.operands[0]
        if ( $operation.operandCount -ne 1 -or $operation.hasImmediate -ne 1 -or
             $operation.targetSymbol -ne 0 -or $operation.targetValue -ne 0 -or
             $operation.calleeType -ne 0 -or $operation.payloadType -ne 0 -or
             $null -eq $origin -or
             $origin.memberAbiName -notin @("class-field-i64", "class-field-string-copy") -or
             $null -eq $definition ) {
            throw "Lowered XLIR $operationName has no exact class MemberAbi, receiver, key or result contract"
        }
        $sourceKinds[$definition.kindName] = $true
    }
    foreach ( $sourceKind in @("load", "nullable-unwrap") ) {
        if ( ! $sourceKinds.ContainsKey($sourceKind) ) {
            throw "Lowered XLIR $operationName is missing $sourceKind receiver coverage"
        }
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR inherited/narrowed class-field C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR inherited/narrowed class-field repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR inherited/narrowed class-field generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_object_get_literal_i64(v",
    "xlir_xvalue_object_get_literal_string_copy(v"
) ) {
    if ( ([regex]::Matches($c, [regex]::Escape($required))).Count -ne 2 ) {
        throw "XLIR inherited/narrowed class-field Cgen did not emit the exact mechanical projection count: $required"
    }
}
foreach ( $forbidden in @(
    "xl6_lookup_inherited_field", "xl6_narrow_nullable_member",
    "xl6_infer_class_field_abi"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR inherited/narrowed class-field Cgen reconstructed typed semantics: $forbidden"
    }
}

Push-Location $Root
try {
    # Include the generated module only in the generic host translation unit.
    # Compiler-wide -include would duplicate every exported definition in the
    # separately compiled support source.
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"build/tmp/xlir_class_field_inherited_narrowed_abi.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c !XL6_CGEN_LINKS! -o build\test\xlir_class_field_inherited_narrowed_abi.exe && build\test\xlir_class_field_inherited_narrowed_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR inherited/narrowed class-field strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR inherited/narrowed class-field ABI gate passed: explicit base-layout lookup, semantic nullable flow type, exact MemberAbi, deterministic mechanical C, and strict runtime."
