param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$compiler = Join-Path $Root "release\xl.exe"
$fixture = Join-Path $Root "test\cases\export_primitive_widths.xl"
$hostSource = Join-Path $Root "test\cases\export_primitive_widths_host.c"
$outputDir = Join-Path $Root "build\tmp\xlir_export_primitive_widths"
$legacyDir = Join-Path $Root "build\tmp\legacy_export_primitive_widths"
$loweredJson = Join-Path $outputDir "export_primitive_widths.lowered.json"
$xlirC = Join-Path $outputDir "export_primitive_widths.c"
$xlirHeader = Join-Path $outputDir "export_primitive_widths.h"
$legacyC = Join-Path $legacyDir "export_primitive_widths.c"
$legacyHeader = Join-Path $legacyDir "export_primitive_widths.h"
$xlirExe = Join-Path $outputDir "export_primitive_widths.exe"
$legacyExe = Join-Path $legacyDir "export_primitive_widths.exe"

foreach ( $path in @($compiler, $fixture, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR export primitive-width gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force $outputDir, $legacyDir | Out-Null

& $compiler ir $fixture --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR export primitive-width JSON dump failed" }
$json = [System.IO.File]::ReadAllText($loweredJson, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$float32Types = @($json.types | Where-Object { $_.name -eq "float32" })
if ( $float32Types.Count -ne 1 -or
     $float32Types[0].kind -ne 1 -or
     $float32Types[0].collectionAbiName -ne "none" -or
     $float32Types[0].cAbi -le 0 -or
     $float32Types[0].ownershipAbi -ne 1 -or
     $float32Types[0].nullAbi -ne 0 -or
     $float32Types[0].errorResultCName -ne "xlir_result_f32" ) {
    throw "Language float32 has no first-class trivial F32/error-result ABI"
}
$export = @($json.exports)[0]
$expectedNames = @("i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "f32", "f64", "ok", "handle")
$expectedTypes = @("int8", "uint8", "int16", "uint16", "int32", "uint32", "int", "uint64", "float32", "float", "bool", "point")
$expectedValues = @("i8", "u8", "i16", "u16", "i32", "u32", "type", "type", "type", "type", "type", "type")
$parameterTypeNames = @($export.parameterTypes | ForEach-Object {
    $typeId = $_
    @($json.types | Where-Object { $_.id -eq $typeId })[0].name
})
$returnTypes = @($json.types | Where-Object { $_.id -eq $export.returnType })
if ( @($json.exports).Count -ne 1 -or
     $export.returnAbiName -ne "value" -or
     $export.returnValueAbi -ne "type" -or
     $returnTypes.Count -ne 1 -or $returnTypes[0].name -ne "uint64" -or
     (@($export.parameterNames) -join ",") -ne ($expectedNames -join ",") -or
     ($parameterTypeNames -join ",") -ne ($expectedTypes -join ",") -or
     @($export.parameterAbis | Where-Object { $_ -ne "value" }).Count -ne 0 -or
     (@($export.parameterValueAbis) -join ",") -ne ($expectedValues -join ",") ) {
    throw "XLIR export JSON lost exact public parameter names or value ABIs"
}

& $compiler cgen $fixture --backend xlir --emit $xlirC --emit-header $xlirHeader
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR export primitive-width C generation failed" }
& $compiler cgen $fixture --backend legacy --emit $legacyC --emit-header $legacyHeader
if ( $LASTEXITCODE -ne 0 ) { throw "Legacy export primitive-width C generation failed" }
$prototype = "uint64_t xl6_pick_u64(int8_t i8, uint8_t u8, int16_t i16, uint16_t u16, int32_t i32, uint32_t u32, int64_t i64, uint64_t u64, float f32, double f64, bool ok, void* handle)"
$xlirText = [System.IO.File]::ReadAllText($xlirC, [System.Text.Encoding]::UTF8)
$xlirHeaderText = [System.IO.File]::ReadAllText($xlirHeader, [System.Text.Encoding]::UTF8)
$legacyHeaderText = [System.IO.File]::ReadAllText($legacyHeader, [System.Text.Encoding]::UTF8)
$privateParameters = @('int8_t', 'uint8_t', 'int16_t', 'uint16_t', 'int32_t', 'uint32_t',
    'int64_t', 'uint64_t', 'float', 'double', 'bool', 'void*')
for ($p = 0; $p -lt $privateParameters.Count; ++$p) { $privateParameters[$p] += " xlir_input_$p" }
$privatePrototype = 'uint64_t xl6_pick_u64(' + ($privateParameters -join ', ') + ')'
if ( ! $xlirText.Contains($privatePrototype) -or
     ! $xlirHeaderText.Contains("$prototype;") -or
     ! $legacyHeaderText.Contains("$prototype;") ) {
    throw "XLIR/legacy export primitive-width public prototypes differ"
}
foreach ( $adapter in @(
    "(int64_t)xlir_input_0", "(int64_t)xlir_input_1", "(int64_t)xlir_input_2",
    "(int64_t)xlir_input_3", "(int64_t)xlir_input_4", "(int64_t)xlir_input_5"
) ) {
    if ( ! $xlirText.Contains($adapter) ) {
        throw "XLIR export wrapper omitted a verified public-to-internal adapter: $adapter"
    }
}

$common = Join-Path $Root "test\suites\common.bat"
$command = "call `"$common`" && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -I`"$outputDir`" `"$xlirC`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$xlirExe`" && `"$xlirExe`" && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXL6_LEGACY_RUNTIME -I`"$legacyDir`" `"$legacyC`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$legacyExe`" && `"$legacyExe`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR export primitive-width strict C/legacy differential runtime failed"
}

Write-Output "XLIR export primitive-width ABI gate ok: first-class F32, explicit public names and fixed integer widths, mechanical i64 adapters, identical legacy header, strict C, and differential runtime."
