param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\export_vars.xl"
$hostSource = Join-Path $Root "test\cases\xlir_global_export_boundary_host.c"
$outputDir = Join-Path $Root "build\test"
$textPath = Join-Path $outputDir "xlir_global_export_boundary.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_global_export_boundary.lowered.json"
$cPath = Join-Path $outputDir "xlir_global_export_boundary_generated.c"
$headerPath = Join-Path $outputDir "xlir_global_export_boundary_generated.h"
$exePath = Join-Path $outputDir "xlir_global_export_boundary_host.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR global-export gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format text --emit $textPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR global export lowered text IR failed" }
$text = [System.IO.File]::ReadAllText($textPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "globalExports=2",
    "storage=s3 declaration=n2 initializer=n5 type=t3 mutability=mutable cname=xl6_counter value=1LL",
    "storage=s4 declaration=n6 initializer=n9 type=t3 mutability=const cname=xl6_version value=7LL"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "XLIR GlobalExport text ABI is incomplete: $required"
    }
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR global export lowered JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$counter = $json.globalExports | Where-Object { $_.cName -eq "xl6_counter" }
$version = $json.globalExports | Where-Object { $_.cName -eq "xl6_version" }
if ( $json.globalExportCount -ne 2 -or $counter.storageSymbol -ne 3 -or
     $counter.mutability -ne "mutable" -or $counter.initializer -ne "1LL" -or
     $version.storageSymbol -ne 4 -or $version.mutability -ne "const" -or
     $version.initializer -ne "7LL" ) {
    throw "XLIR GlobalExport JSON has no exact static storage ABI"
}

& $xl cgen $source --backend xlir --emit $cPath --emit-header $headerPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR global export Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
$header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "int64_t xl6_counter = 1LL;",
    "const int64_t xl6_version = 7LL;"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR C emitter ignored verified GlobalExport storage: $required"
    }
}
if ( $generated -notmatch 'v[0-9]+ = xl6_counter;' -or
     $generated -notmatch '\{ &\(xl6_counter\), &\(xlir_committed\), sizeof\(xl6_counter\) \}' -or
     !$generated.Contains('(xl6SlotRuntimeV1Get()->Apply)') ) {
    throw "XLIR LOAD/transactional STORE emission did not use the verified public storage name"
}
foreach ( $required in @(
    "extern int64_t xl6_counter;",
    "extern const int64_t xl6_version;"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR header ignored verified GlobalExport ABI: $required"
    }
}
if ( $generated.Contains("static int64_t s3") -or $generated.Contains("static int64_t s4") ) {
    throw "XLIR C emitter duplicated public global storage behind a private symbol"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR global export generated-C runtime failed" }

Write-Output "XLIR global export boundary gate ok: static scalar initialization, mutable/const storage, transactional writes, C/header linkage, and direct host access."
exit 0
