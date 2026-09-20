param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$compiler = Join-Path $Root "release\xl.exe"
$fixture = Join-Path $Root "test\cases\c_abi_types.xl"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$foreignHost = Join-Path $Root "test\cases\c_abi_types_host.c"
$loweredIr = Join-Path $Root "build\tmp\xlir_c_scalar_abi.lowered.json"
$generatedC = Join-Path $Root "build\tmp\xlir_c_scalar_abi.c"

foreach ( $path in @($compiler, $fixture, $runtimeHost, $foreignHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR C scalar ABI gate input is missing: $path"
    }
}

Remove-Item -LiteralPath $loweredIr -Force -ErrorAction SilentlyContinue
& $compiler ir $fixture --lowered --format json --emit $loweredIr
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $loweredIr -PathType Leaf) ) {
    throw "XLIR C scalar ABI lowered-IR dump failed"
}
$ir = [System.IO.File]::ReadAllText(
    $loweredIr, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
foreach ( $typeName in @(
    "c_char", "c_schar", "c_uchar", "c_short", "c_ushort", "c_int", "c_uint",
    "c_long", "c_ulong", "c_longlong", "c_ulonglong", "c_size", "c_ssize",
    "c_ptrdiff", "c_intptr", "c_uintptr", "c_bool", "c_float", "c_double", "c_wchar"
) ) {
    $matches = @($ir.types | Where-Object { $_.name -eq $typeName })
    if ( $matches.Count -ne 1 ) {
        throw "XLIR C scalar type is not unique in the type pool: $typeName"
    }
    $type = $matches[0]
    if ( $type.kind -ne 1 -or
         $type.collectionAbiName -ne "none" -or
         $type.runtimeResourceAbiName -ne "none" -or
         $type.persistenceValueAbiName -ne "none" -or
         $type.cAbi -le 0 -or
         $type.callableErasureAbiName -ne "invalid" -or
         $type.ownershipAbi -ne 1 -or
         $type.nullAbi -ne 0 -or
         $type.nominalSymbol -ne 0 -or
         $type.integerTargetName -ne "none" -or
         @($type.params).Count -ne 0 -or
         $type.typeValueId -notmatch '^0x[0-9A-F]{16}$' ) {
        throw "XLIR C scalar type has no exact trivial ABI: $typeName"
    }
}
$scalarConversions = @($ir.instructions | Where-Object {
    $_.kindName -eq "scalar-convert" -and
    $_.opcodeName -eq "none" -and
    $_.runtimeOpName -eq "none"
})
if ( $scalarConversions.Count -lt 35 ) {
    throw "XLIR C scalar calls do not expose explicit inbound/outbound scalar conversions"
}
foreach ( $abi in @(
    "bool-to-bool", "integer-to-integer", "float-to-float"
) ) {
    if ( @($scalarConversions | Where-Object {
        $_.scalarConversionAbiName -eq $abi
    }).Count -eq 0 ) {
        throw "XLIR C scalar calls lost a canonical scalar conversion family: $abi"
    }
}

Remove-Item -LiteralPath $generatedC -Force -ErrorAction SilentlyContinue
& $compiler cgen $fixture --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 -or ! (Test-Path -LiteralPath $generatedC -PathType Leaf) ) {
    throw "XLIR C scalar ABI C generation failed"
}
$cText = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $prototype in @(
    "extern char xl6_c_echo_char(char);",
    "extern signed char xl6_c_echo_schar(signed char);",
    "extern unsigned char xl6_c_echo_uchar(unsigned char);",
    "extern short xl6_c_echo_short(short);",
    "extern unsigned short xl6_c_echo_ushort(unsigned short);",
    "extern int xl6_c_echo_int(int);",
    "extern unsigned int xl6_c_echo_uint(unsigned int);",
    "extern long xl6_c_echo_long(long);",
    "extern unsigned long xl6_c_echo_ulong(unsigned long);",
    "extern long long xl6_c_echo_longlong(long long);",
    "extern unsigned long long xl6_c_echo_ulonglong(unsigned long long);",
    "extern size_t xl6_c_echo_size(size_t);",
    "extern intptr_t xl6_c_echo_ssize(intptr_t);",
    "extern ptrdiff_t xl6_c_echo_ptrdiff(ptrdiff_t);",
    "extern intptr_t xl6_c_echo_intptr(intptr_t);",
    "extern uintptr_t xl6_c_echo_uintptr(uintptr_t);",
    "extern bool xl6_c_not_bool(bool);",
    "extern float xl6_c_echo_float(float);",
    "extern double xl6_c_echo_double(double);",
    "extern wchar_t xl6_c_echo_wchar(wchar_t);"
) ) {
    if ( ! $cText.Contains($prototype) ) {
        throw "XLIR C scalar generated prototype is missing: $prototype"
    }
}
foreach ( $cast in @(
    "(unsigned int)", "(unsigned long long)", "(size_t)", "(intptr_t)",
    "(uintptr_t)", "(bool)", "(float)", "(double)", "(wchar_t)"
) ) {
    if ( ! $cText.Contains($cast) ) {
        throw "XLIR C scalar generated conversion is missing: $cast"
    }
}

$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include build\tmp\xlir_c_scalar_abi.c test\cases\xlir_default_class_layout_host.c test\cases\c_abi_types_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_c_scalar_abi.exe && build\tmp\xlir_c_scalar_abi.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR exact C scalar generated-C runtime gate failed"
}

Write-Output "XLIR exact C scalar types, conversions, prototypes and runtime ABI ok."
