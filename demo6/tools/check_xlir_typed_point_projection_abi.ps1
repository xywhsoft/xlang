param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_typed_point.xl"
$runtimeHost = Join-Path $Root "test\cases\xlir_program_entry_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_typed_point_projection.typed.json"
$loweredJson = Join-Path $outputDir "xlir_typed_point_projection.lowered.json"
$generatedC = Join-Path $outputDir "xlir_typed_point_projection.c"
$generatedCSecond = Join-Path $outputDir "xlir_typed_point_projection.second.c"
$legacyC = Join-Path $outputDir "xlir_typed_point_projection.legacy.c"

foreach ( $path in @($xl, $source, $runtimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR typed point projection gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed point projection Typed IR construction failed"
}
& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed point projection lowering failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$write = @($typed.nodes | Where-Object {
    $_.kindName -eq "index" -and $_.line -eq 14 -and
    $_.indexWriteAbiName -eq "xvalue-array-i64-set-new"
})
$read = @($typed.nodes | Where-Object {
    $_.kindName -eq "index" -and $_.line -eq 16 -and
    $_.indexWriteAbiName -eq "none"
})
if ( $write.Count -ne 1 -or $read.Count -ne 1 -or
     $write[0].storageType -ne $read[0].storageType ) {
    throw "Typed XLIR point container has no exact write/read element TypeId"
}
$pointType = @($typed.types | Where-Object { $_.id -eq $read[0].storageType })
if ( $pointType.Count -ne 1 -or $pointType[0].cAbi -ne 28 -or
     $pointType[0].ownershipAbi -ne 1 -or $pointType[0].nullAbi -ne 1 -or
     $pointType[0].typeValueId -eq "0x0000000000000000" ) {
    throw "Typed XLIR point element has no exact trivial pointer-sentinel ABI"
}

$box = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-raw-pointer-borrow" -and
    $_.sourceLine -eq 14
})
$set = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-array-set-i64-new" -and
    $_.sourceLine -eq 14
})
$lookup = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-array-at-i64" -and
    $_.sourceLine -eq 16
})
$projection = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-raw-pointer-borrow" -and
    $_.sourceLine -eq 16
})
if ( $box.Count -ne 1 -or $set.Count -ne 1 -or
     $lookup.Count -ne 1 -or $projection.Count -ne 1 ) {
    throw "Lowered XLIR point container has no exact box/set/borrow/project operation chain"
}
if ( $set[0].operandCount -ne 3 -or
     $set[0].operands[2] -ne $box[0].result -or
     $projection[0].id -ne $lookup[0].id + 1 -or
     $projection[0].operandCount -ne 1 -or
     $projection[0].operands[0] -ne $lookup[0].result -or
     $projection[0].type -ne $pointType[0].id -or
     $projection[0].payloadType -ne $pointType[0].id -or
     $projection[0].runtimeIdentityType -ne 0 -or
     $projection[0].calleeType -ne 0 ) {
    throw "Lowered XLIR point projection has no exact borrowed carrier, static payload TypeId and pointer-sentinel result ABI"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed point projection C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed point projection repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR typed point projection generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtValuePointer(",
    "xrtValueArraySetNew(",
    "xrtValueArrayAt(",
    "extern bool xrtValueGetPointer(const xvalue*, void**);",
    " == NULL || !xrtValueGetPointer(v",
    "abort();"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR typed point projection C is missing mechanical IR emission: $required"
    }
}
foreach ( $forbidden in @(
    "point<ImageTag>", "cgen_typed_point.xl", "indexWriteAbi",
    "collectionAbi", "sourceKind", "xl6_project_point"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR Cgen reconstructed typed point source semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_typed_point_projection.c test\cases\xlir_program_entry_host.c !XL6_CGEN_LINKS! -o build\test\xlir_typed_point_projection.exe && build\test\xlir_typed_point_projection.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed point projection strict-C/runtime gate failed"
    }

    & $xl cgen $source --backend legacy --emit $legacyC
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed point projection legacy C generation failed"
    }
    $legacyCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror build\tmp\xlir_typed_point_projection.legacy.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_typed_point_projection.legacy.exe && build\test\xlir_typed_point_projection.legacy.exe'
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed point projection legacy differential gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR typed point projection gate passed: exact pointer-sentinel TypeId, explicit raw-pointer box/project operations, canonical borrowed carrier ABI, deterministic strict C, runtime assertions, and legacy differential semantics."
