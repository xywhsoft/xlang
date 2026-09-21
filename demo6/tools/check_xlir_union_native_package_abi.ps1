param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\import_package_type_union_use.xl"
$nativeSource = Join-Path $Root "test\fixtures\union_api\union_api_native.c"
$outputDir = Join-Path $Root "build\test"
$irPath = Join-Path $outputDir "xlir_union_native_package_abi.xlir.json"
$cPath = Join-Path $outputDir "xlir_union_native_package_abi.generated.c"
$nativeObject = Join-Path $outputDir "xlir_union_native_package_abi.native.o"
$typeName = "XL_Union_____xl6__import__union__api__Payload"
$guardName = "XL6_ABI_DEFINED_$typeName"

foreach ( $path in @($xl, $source, $nativeSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR native union package ABI input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$oldPackagePath = $env:XLANG_PACKAGE_PATH
try {
    $env:XLANG_PACKAGE_PATH =
        (Join-Path $Root "release\extlibs") + ";" +
        (Join-Path $Root "test\fixtures")

    & $xl ir $source --format json --emit $irPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native union package IR failed" }
    $ir = [System.IO.File]::ReadAllText(
        $irPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $unionTypes = @($ir.types | Where-Object { $_.kind -eq 3 })
    if ( $unionTypes.Count -ne 1 -or $unionTypes[0].cName -ne $typeName ) {
        throw "XLIR does not own the exact native union package C ABI name"
    }
    $fieldCNames = @($ir.unionFields | ForEach-Object { $_.cName })
    if ( $fieldCNames.Count -ne 2 -or $fieldCNames[0] -ne "i" -or
         $fieldCNames[1] -ne "d" ) {
        throw "XLIR does not own the exact native union field C ABI names"
    }

    & $xl cgen $source --backend xlir --emit $cPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR native union package Cgen failed" }
    $generated = [System.IO.File]::ReadAllText(
        $cPath, [System.Text.Encoding]::UTF8)
    foreach ( $required in @(
        "#ifndef $guardName",
        "#define $guardName 1",
        "typedef union $typeName",
        "    int64_t i;",
        "    double d;",
        "extern $typeName xl6_union_api_default;"
    ) ) {
        if ( ! $generated.Contains($required) ) {
            throw "XLIR native union package C ABI is incomplete: $required"
        }
    }
    if ( $generated -match 'xlir_union_t[0-9]+' ) {
        throw "XLIR leaked a pool-order union name across the native package ABI"
    }

    & $xl run $source --backend xlir
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR native union package compile/runtime contract failed"
    }
    & $xl run $source --backend legacy
    if ( $LASTEXITCODE -ne 0 ) {
        throw "legacy native union package compatibility contract failed"
    }

    $command = "call test\suites\common.bat && `"!XL6_CGEN_CC!`" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c `"$nativeSource`" -o `"$nativeObject`""
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "native union package source is not independently compilable"
    }
}
finally {
    $env:XLANG_PACKAGE_PATH = $oldPackagePath
}

Write-Output "XLIR native union package ABI gate ok: stable nominal C name, shared definition guard, standalone native source, XLIR and legacy runtime."
exit 0
