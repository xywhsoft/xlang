param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_typed_container_safe_index_objects_functions.xl"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$support = Join-Path $Root "test\cases\cgen_class_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_typed_safe_index_objects_functions.typed.json"
$loweredJson = Join-Path $outputDir "xlir_typed_safe_index_objects_functions.lowered.json"
$generatedC = Join-Path $outputDir "xlir_typed_safe_index_objects_functions.c"
$generatedCSecond = Join-Path $outputDir "xlir_typed_safe_index_objects_functions.second.c"

foreach ( $path in @($xl, $source, $genericHost, $support) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR typed safe-index nominal ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed safe-index nominal Typed IR construction failed"
}
& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed safe-index nominal lowering failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$native = @($typed.types | Where-Object {
    $_.kind -eq 2 -and $_.name -eq "Native" -and $_.cAbi -eq 26 -and
    $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 2 -and
    $_.nominalSymbol -ne 0 -and $_.typeValueId -ne "0x0000000000000000"
})
if ( $native.Count -ne 1 ) {
    throw "XLIR native-backed class element has no unique nominal managed-object ABI"
}
$nullableNative = @($typed.types | Where-Object {
    $_.kind -eq 8 -and $_.params.Count -eq 1 -and
    $_.params[0] -eq $native[0].id
})
if ( $nullableNative.Count -ne 1 -or
     $nullableNative[0].cAbi -ne 26 -or
     $nullableNative[0].ownershipAbi -ne 2 -or
     $nullableNative[0].nullAbi -ne 2 ) {
    throw "XLIR nullable native class did not preserve presence with the managed xvalue ABI"
}

$lookupCounts = @{
    "xvalue-array-at-i64-or-null" = 8
    "xvalue-int-map-at-i64-or-null" = 8
    "xvalue-object-get-literal" = 6
    "xvalue-index-get-xvalue-key" = 2
}
$safeIndexOrigins = @($lowered.nodes | Where-Object {
    $_.kindName -eq 'index' -and $_.sourceKindName -eq 'safe_index'
} | ForEach-Object { $_.id })
$lookups = @()
foreach ( $entry in $lookupCounts.GetEnumerator() ) {
    $matches = @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq $entry.Key -and
        $safeIndexOrigins -contains $_.origin
    })
    if ( $matches.Count -ne $entry.Value ) {
        throw "XLIR safe-index lookup carrier count is incomplete for $($entry.Key): $($matches.Count)"
    }
    $lookups += $matches
}
foreach ( $lookup in $lookups ) {
    $carrier = Get-TypeById $lowered $lookup.type
    if ( $lookup.operandCount -lt 1 -or $null -eq $carrier -or
         $carrier.kind -ne 1 -or $carrier.name -ne "var" -or
         $carrier.cAbi -ne 26 -or $carrier.ownershipAbi -ne 2 -or
         $carrier.nullAbi -ne 2 ) {
        throw "XLIR safe-index runtime lookup did not return one canonical dynamic carrier"
    }
}

$retains = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-nullable-xvalue-retain"
})
$unions = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-nullable-union-copy"
})
$closures = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-nullable-closure-copy"
})
if ( $retains.Count -ne 12 -or $unions.Count -ne 6 -or $closures.Count -ne 6 ) {
    throw "XLIR safe-index exact nullable projection coverage is incomplete: retained=$($retains.Count), union=$($unions.Count), closure=$($closures.Count)"
}
$nativeRetains = @($retains | Where-Object {
    $_.payloadType -eq $native[0].id
})
if ( $nativeRetains.Count -ne 6 ) {
    throw "XLIR safe-index native presence projection count is incomplete: $($nativeRetains.Count)"
}
foreach ( $projection in $retains ) {
    $result = Get-TypeById $lowered $projection.type
    $payload = Get-TypeById $lowered $projection.payloadType
    if ( $projection.operandCount -ne 1 -or $projection.payloadType -eq 0 -or
         $projection.runtimeIdentityType -ne $projection.payloadType -or
         $projection.calleeType -ne 0 -or
         $null -eq $result -or $null -eq $payload -or
         $result.kind -ne 8 -or $result.params.Count -ne 1 -or
         $result.params[0] -ne $projection.payloadType -or
         $result.cAbi -ne 26 -or $result.ownershipAbi -ne 2 -or
         $result.nullAbi -ne 2 -or
         $payload.typeValueId -eq "0x0000000000000000" ) {
        throw "XLIR safe-index retained nominal projection has no exact payload TypeId and ownership ABI"
    }
}
foreach ( $projection in $unions ) {
    $result = Get-TypeById $lowered $projection.type
    $payload = Get-TypeById $lowered $projection.payloadType
    if ( $projection.operandCount -ne 1 -or $null -eq $result -or
         $null -eq $payload -or $result.kind -ne 8 -or
         $result.params[0] -ne $projection.payloadType -or
         $result.cAbi -ne 36 -or $result.ownershipAbi -ne 1 -or
         $result.nullAbi -ne 3 -or $payload.kind -ne 3 -or
         $payload.cAbi -ne 38 -or
         $payload.typeValueId -eq "0x0000000000000000" ) {
        throw "XLIR safe-index union projection has no exact tagged layout and TypeId ABI"
    }
}
foreach ( $projection in $closures ) {
    $result = Get-TypeById $lowered $projection.type
    $signature = Get-TypeById $lowered $projection.calleeType
    if ( $projection.operandCount -ne 1 -or $projection.calleeType -eq 0 -or
         $null -eq $result -or $null -eq $signature -or
         $result.kind -ne 8 -or $result.params[0] -ne $projection.calleeType -or
         $result.cAbi -ne 31 -or $result.ownershipAbi -ne 2 -or
         $result.nullAbi -ne 1 -or $signature.kind -ne 4 -or
         $signature.cAbi -ne 31 -or $signature.ownershipAbi -ne 2 ) {
        throw "XLIR safe-index callable projection has no exact nullable signature ABI"
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed safe-index nominal C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed safe-index nominal repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR typed safe-index nominal generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static xvalue* xlir_xvalue_project_nullable_retain(",
    "xlir_xvalue_type_id(source) != exactTypeId",
    " = xlir_xvalue_project_nullable_retain(v",
    "xlir_xvalue_union_unbox(",
    "xlir_xvalue_project_closure_try_copy_into("
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR typed safe-index nominal generated C is missing mechanical IR emission: $required"
    }
}
foreach ( $forbidden in @(
    "xl6_safe_index_pair", "xl6_safe_index_payload",
    "xl6_safe_index_native", "xl6_safe_index_function",
    "xl6_infer_safe_index_result", "xl6_source_index_kind"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR typed safe-index nominal Cgen reconstructed source semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_typed_safe_index_objects_functions.c test\cases\xlir_generated_module_diagnostic_host.c test\cases\cgen_class_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_typed_safe_index_objects_functions.exe && build\test\xlir_typed_safe_index_objects_functions.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed safe-index nominal strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR typed safe-index nominal ABI gate passed: canonical dynamic lookup carriers, exact Pair/native/union/closure nullable projections, null-handle presence, deterministic strict C, and runtime assertions."
