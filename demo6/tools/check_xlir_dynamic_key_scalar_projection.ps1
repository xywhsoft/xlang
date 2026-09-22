param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_typed_container_index_assign.xl"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_dynamic_key_scalar_projection.typed.json"
$loweredJson = Join-Path $outputDir "xlir_dynamic_key_scalar_projection.lowered.json"
$generatedC = Join-Path $outputDir "xlir_dynamic_key_scalar_projection.c"
$generatedCSecond = Join-Path $outputDir "xlir_dynamic_key_scalar_projection.second.c"

foreach ( $path in @($xl, $source, $runtimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR dynamic-key scalar projection gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR dynamic-key scalar projection Typed IR construction failed"
}
& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR dynamic-key scalar projection lowering failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$index = @($typed.nodes | Where-Object {
    $_.kindName -eq "index" -and $_.line -eq 22 -and
    $_.storageType -ne 0 -and $_.indexWriteAbiName -eq "none"
})
if ( $index.Count -ne 1 ) {
    throw "Typed XLIR dynamic-key scalar read has no unique static result TypeId"
}
$resultType = @($typed.types | Where-Object { $_.id -eq $index[0].storageType })
if ( $resultType.Count -ne 1 -or $resultType[0].cAbi -ne 3 -or
     $resultType[0].ownershipAbi -ne 1 -or $resultType[0].nullAbi -ne 0 -or
     $resultType[0].typeValueId -eq "0x0000000000000000" ) {
    throw "Typed XLIR dynamic-key scalar result has no exact trivial i64 ABI"
}

$lookup = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-index-get-xvalue-key" -and
    $_.sourceLine -eq 22
})
$projection = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-scalar-copy" -and
    $_.sourceLine -eq 22
})
if ( $lookup.Count -ne 1 -or $projection.Count -ne 1 ) {
    throw "Lowered XLIR dynamic-key scalar read is not one lookup plus one projection"
}
$carrierType = @($lowered.types | Where-Object { $_.id -eq $lookup[0].type })
if ( $lookup[0].operandCount -ne 2 -or $lookup[0].result -eq 0 -or
     $carrierType.Count -ne 1 -or $carrierType[0].kind -ne 1 -or
     $carrierType[0].name -ne "var" -or $carrierType[0].cAbi -ne 26 -or
     $carrierType[0].ownershipAbi -ne 2 -or $carrierType[0].nullAbi -ne 2 ) {
    throw "Lowered XLIR dynamic-key lookup did not preserve the canonical borrowed carrier ABI"
}
if ( $projection[0].id -ne $lookup[0].id + 1 -or
     $projection[0].operandCount -ne 1 -or
     $projection[0].operands[0] -ne $lookup[0].result -or
     $projection[0].type -ne $resultType[0].id -or
     $projection[0].payloadType -ne $resultType[0].id -or
     $projection[0].runtimeIdentityType -ne 0 -or
     $projection[0].calleeType -ne 0 ) {
    throw "Lowered XLIR scalar projection has no exact source, payload TypeId and trivial result ABI"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR dynamic-key scalar projection C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR dynamic-key scalar projection repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR dynamic-key scalar projection generated C is not deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_index_get_xvalue_key_or_null(",
    "extern bool xrtValueGetInt(const xvalue*, int64_t*);",
    " == NULL || !xrtValueGetInt(v",
    "abort();"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR dynamic-key scalar projection C is missing mechanical IR emission: $required"
    }
}
foreach ( $forbidden in @(
    "indexWriteAbi", "collectionAbi", "lvalueKind", "sourceKind",
    "xl6_dynamic_key_int", "xl6_infer_dynamic_key_result",
    "cgen_typed_container_index_assign.xl"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR Cgen reconstructed dynamic-key scalar source semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_dynamic_key_scalar_projection.c test\cases\xlir_default_class_layout_host.c !XL6_CGEN_LINKS! -o build\test\xlir_dynamic_key_scalar_projection.exe && build\test\xlir_dynamic_key_scalar_projection.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR dynamic-key scalar projection strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR dynamic-key scalar projection gate passed: canonical borrowed lookup, exact payload TypeId, invariant-checked trivial scalar copy, deterministic strict C, and runtime assertions."
