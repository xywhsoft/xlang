param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_managed_native_extern_receiver_abi.xl"
$support = Join-Path $Root "test\cases\xlir_managed_native_extern_receiver_host.c"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_managed_native_extern_receiver_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_managed_native_extern_receiver_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_managed_native_extern_receiver_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_managed_native_extern_receiver_abi.second.c"
$legacyC = Join-Path $outputDir "xlir_managed_native_extern_receiver_abi.legacy.c"

foreach ( $path in @($xl, $source, $support, $genericHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR managed native extern receiver ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Read-Json($path) {
    return [System.IO.File]::ReadAllText(
        $path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed native extern receiver Typed IR failed" }
$typed = Read-Json $typedJson
$probe = @($typed.types | Where-Object { $_.name -eq "BorrowedProbe" })[0]
$field = @($typed.classFields | Where-Object { $_.ownerType -eq $probe.id })[0]
$binds = @($typed.nativeHandleBinds | Where-Object {
    $_.targetType -eq $probe.id -and $_.abiName -eq "class-field-borrow"
})
if ( $null -eq $probe -or $probe.cAbi -ne 26 -or
     $probe.ownershipAbi -ne 2 -or $probe.nullAbi -ne 2 -or
     $probe.nominalSymbol -eq 0 -or
     $probe.typeValueId -eq "0x0000000000000000" -or
     $null -eq $field -or $field.declaration -eq 0 -or
     $field.nativeDestroySymbol -ne 0 -or $binds.Count -ne 3 -or
     @($binds | Where-Object {
         $_.fieldDeclaration -ne $field.declaration
     }).Count -ne 0 ) {
    throw "Typed XLIR native extern receiver is not a managed nominal object with exact borrowed field binds"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed native extern receiver lowering failed" }
$lowered = Read-Json $loweredJson
$target = @($lowered.callTargets | Where-Object {
    $_.cName -eq "borrowed_probe_value"
})
if ( $target.Count -ne 1 -or $target[0].parameterCount -ne 1 -or
     @($target[0].parameterTypes)[0] -ne $probe.id -or
     @($target[0].parameterAbis)[0] -ne 2 -or
     @($target[0].parameterPhysicalAbis)[0] -ne
        "native-class-handle-borrow" -or
     @($target[0].parameterPhysicalFieldDeclarations)[0] -ne
        $field.declaration ) {
    throw "Lowered XLIR native extern target has no exact semantic receiver and physical field-borrow ABI"
}
$calls = @($lowered.instructions | Where-Object {
    $_.kindName -eq "call" -and $_.targetSymbol -eq $target[0].symbol
})
$projections = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-object-get-literal-raw-pointer"
})
if ( $calls.Count -ne 3 -or $projections.Count -ne 3 ) {
    throw "Lowered XLIR native extern calls do not have three explicit managed-field receiver projections"
}
foreach ( $call in $calls ) {
    $projection = @($lowered.instructions | Where-Object {
        $_.result -eq $call.operands[0] -and
        $_.runtimeOpName -eq "xvalue-object-get-literal-raw-pointer"
    })
    if ( $projection.Count -ne 1 -or $projection[0].type -ne $field.type -or
         $projection[0].operandCount -ne 1 ) {
        throw "Lowered XLIR native extern CALL does not consume its exact raw field projection"
    }
}
if ( @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-project-native-handle-borrow" -or
        $_.kindName -eq "native-handle-project"
    }).Count -ne 0 ) {
    throw "XLIR managed native extern receiver regressed to the direct nominal-handle projector"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed native extern receiver C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed native extern receiver repeat C generation failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR managed native extern receiver generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "extern int64_t borrowed_probe_value(void*);",
    "xlir_xvalue_object_get_literal_raw_pointer(",
    "borrowed_probe_value(v"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR managed native extern receiver generated C is missing: $required"
    }
}
foreach ( $forbidden in @(
    "xlir_xvalue_project_native_handle_borrow",
    "xrtValueTypeId(source) != typeId"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR managed native extern receiver generated C contains obsolete direct-handle logic: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_managed_native_extern_receiver_abi.c -include test\cases\xlir_managed_native_extern_receiver_host.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\xlir_managed_native_extern_receiver_abi.exe && build\test\xlir_managed_native_extern_receiver_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native extern receiver strict-C/runtime gate failed"
    }

    & $xl cgen $source --backend legacy --emit $legacyC
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR managed native extern receiver legacy C generation failed" }
    $legacyCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror build\tmp\xlir_managed_native_extern_receiver_abi.legacy.c test\cases\xlir_managed_native_extern_receiver_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_managed_native_extern_receiver_abi.legacy.exe && build\test\xlir_managed_native_extern_receiver_abi.legacy.exe'
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR managed native extern receiver legacy differential runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR managed native extern receiver ABI gate passed: managed reference identity, exact CallTarget physical field, explicit lowering projection, mechanical C, strict runtime, and legacy differential semantics."
