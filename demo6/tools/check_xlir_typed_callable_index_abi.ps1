param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_typed_callable_index_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$outputDir = Join-Path $Root "build\tmp"
$loweredJson = Join-Path $outputDir "xlir_typed_callable_index_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_typed_callable_index_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_typed_callable_index_abi.second.c"
$legacyC = Join-Path $outputDir "xlir_typed_callable_index_abi.legacy.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR typed callable index ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR typed callable index lowering failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$projections = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-closure-copy"
})
if ( $projections.Count -ne 2 ) {
    throw "XLIR typed callable index projection count is incomplete: $($projections.Count)"
}
foreach ( $projection in $projections ) {
    $resultType = Get-TypeById $lowered $projection.type
    $sourceDefinition = @($lowered.instructions | Where-Object {
        $_.result -eq $projection.operands[0]
    })[0]
    $sourceType = if ( $null -ne $sourceDefinition ) {
        Get-TypeById $lowered $sourceDefinition.type
    } else { $null }
    if ( $projection.operandCount -ne 1 -or
         $projection.calleeType -ne $projection.type -or
         $projection.payloadType -ne 0 -or
         $null -eq $resultType -or $resultType.name -ne "function" -or
         $resultType.cAbi -ne 31 -or $resultType.ownershipAbi -ne 2 -or
         $null -eq $sourceDefinition -or
         $sourceDefinition.runtimeOpName -ne "xvalue-array-at-i64" -or
         $null -eq $sourceType -or $sourceType.name -ne "var" ) {
        throw "XLIR typed callable index has no exact borrowed xvalue source, calleeType, and owned closure result ABI"
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR typed callable index C generation failed" }
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR typed callable index repeat C generation failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR typed callable index generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static xlir_closure* xlir_xvalue_project_closure_try_copy(const xvalue* source",
    "xrtValueType(source) == 0", "xrtValueIsCallable(source)",
    "retained = xrtValueRetain(source)",
    "xlir_closure_new(entry, NULL, retained, xlir_xvalue_closure_drop, NULL, NULL, 0)",
    " = xlir_xvalue_project_closure_try_copy(v",
    "xlir_xvalue_closure_entry_t"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR typed callable index generated C is missing mechanical callable projection: $required"
    }
}
if ( ([regex]::Matches($c,
        'static xlir_closure\* xlir_xvalue_project_closure_try_copy\(')).Count -ne 1 ) {
    throw "XLIR typed callable index helper emission is not use-directed and unique"
}
if ( ([regex]::Matches($c,
        ' = xlir_xvalue_project_closure_try_copy\(v')).Count -ne 2 ) {
    throw "XLIR typed callable index Cgen did not emit exactly two verified projection instructions"
}
foreach ( $forbidden in @(
    "xlir_xvalue_object_set_literal_closure",
    "xlir_xvalue_object_get_literal_closure",
    "xlir_xvalue_project_nullable_closure_copy",
    "xl6_index_function", "xl6_array_function_get"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR typed callable index Cgen emitted an unrelated or source-shape helper: $forbidden"
    }
}

Push-Location $Root
try {
    # The fixture obtains its callable array through task.all, whose explicit
    # RuntimeOp targets the exported xl.dll Future aggregate bridge.
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_typed_callable_index_abi.c test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_typed_callable_index_abi.exe && build\test\xlir_typed_callable_index_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed callable index strict-C/runtime gate failed"
    }

    & $xl cgen $source --backend legacy --emit $legacyC
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed callable index legacy C generation failed"
    }
    $legacyCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror build\tmp\xlir_typed_callable_index_abi.legacy.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_typed_callable_index_abi.legacy.exe && build\test\xlir_typed_callable_index_abi.legacy.exe'
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed callable index legacy differential runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR typed callable index ABI gate passed: exact borrowed lookup plus owned signature-carrying closure projection, deterministic use-directed C, strict runtime, and legacy differential semantics."
