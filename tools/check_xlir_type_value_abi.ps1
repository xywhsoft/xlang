param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

function Read-Utf8([string]$path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_type_value_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_type_value_abi_host.c"
$historicalHost = Join-Path $Root "test\cases\xlir_type_value_host.c"
$dynamicSource = Join-Path $Root "test\cases\xlir_type_value_dynamic.xl"
$historicalCases = @(
    @{ Name = "type_values"; Source = "test\cases\type_values.xl"; Define = "XLIR_TYPE_VALUES_CASE" },
    @{ Name = "type_aliases"; Source = "test\cases\type_aliases.xl"; Define = "XLIR_TYPE_ALIASES_CASE" },
    @{ Name = "cgen_generic_class_type_values"; Source = "test\cases\cgen_generic_class_type_values.xl"; Define = "XLIR_GENERIC_CLASS_TYPE_VALUES_CASE" }
)
$outputDir = Join-Path $Root "build\test"
$typedTextPath = Join-Path $outputDir "xlir_type_value_abi.typed.xlir"
$typedJsonPath = Join-Path $outputDir "xlir_type_value_abi.typed.json"
$loweredTextPath = Join-Path $outputDir "xlir_type_value_abi.lowered.xlir"
$loweredJsonPath = Join-Path $outputDir "xlir_type_value_abi.lowered.json"
$cPath = Join-Path $outputDir "xlir_type_value_abi_generated.c"
$headerPath = Join-Path $outputDir "xlir_type_value_abi_generated.h"
$exePath = Join-Path $outputDir "xlir_type_value_abi_host.exe"
$dynamicTypedTextPath = Join-Path $outputDir "xlir_type_value_dynamic.typed.xlir"
$dynamicTypedJsonPath = Join-Path $outputDir "xlir_type_value_dynamic.typed.json"
$dynamicLoweredTextPath = Join-Path $outputDir "xlir_type_value_dynamic.lowered.xlir"
$dynamicLoweredJsonPath = Join-Path $outputDir "xlir_type_value_dynamic.lowered.json"
$dynamicCPath = Join-Path $outputDir "xlir_type_value_dynamic.c"
$dynamicExePath = Join-Path $outputDir "xlir_type_value_dynamic.exe"

foreach ( $path in @($xl, $source, $hostSource, $historicalHost, $dynamicSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR TypeId ABI gate input is missing: $path"
    }
}
foreach ( $case in $historicalCases ) {
    $path = Join-Path $Root $case.Source
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR TypeId historical fixture is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format text --emit $typedTextPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR TypeId typed text IR failed" }
$typedText = Read-Utf8 $typedTextPath
foreach ( $required in @(
    "cabi=35 callable=invalid ownership=1 null=0 nominal=s0 typeValue=0xEFF6483752293D0B name=type display=type integer=none params=[] error-result=xlir_result_type_id",
    "kind=type-of admission=verified result=t3 resultAbi=value error=no-raise",
    "query=static-id queryTarget=t4",
    "typeValueTarget=4 symbol=0 target=0"
) ) {
    if ( ! $typedText.Contains($required) ) {
        throw "XLIR TypeId typed identity is incomplete: $required"
    }
}

& $xl ir $dynamicSource --format text --emit $dynamicTypedTextPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic TypeId typed text IR failed" }
$dynamicTypedText = Read-Utf8 $dynamicTypedTextPath
foreach ( $required in @(
    "runtime-type-map version=1 fallback=0x3594F07E9DD4DB58",
    "null=0x730AFBEFBAD8CCE6 bool=0xB7A37F8A6C8D2047 i64=0xE99D3F7F03464F1C",
    "query=dynamic-xvalue queryTarget=",
    "kind=type-of admission=verified"
) ) {
    if ( ! $dynamicTypedText.Contains($required) ) {
        throw "XLIR dynamic TypeId typed identity is incomplete: $required"
    }
}

& $xl ir $dynamicSource --format json --emit $dynamicTypedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic TypeId typed JSON IR failed" }
$dynamicTypedJson = Read-Utf8 $dynamicTypedJsonPath | ConvertFrom-Json
$dynamicQueries = @($dynamicTypedJson.intrinsicCalls | Where-Object {
    $_.kindName -eq "type-of" -and $_.typeQueryAbiName -eq "dynamic-xvalue"
})
if ( $dynamicTypedJson.runtimeTypeMap.abiVersion -ne 1 -or
     $dynamicTypedJson.runtimeTypeMap.fallback -ne "0x3594F07E9DD4DB58" -or
     $dynamicTypedJson.runtimeTypeMap.i64 -ne "0xE99D3F7F03464F1C" -or
     $dynamicTypedJson.runtimeTypeMap.string -ne "0x4955F0967ED4EF16" -or
     $dynamicTypedJson.runtimeTypeMap.array -ne "0x275BD96A834D5C08" -or
     $dynamicQueries.Count -ne 6 -or
     @($dynamicQueries | Where-Object {
         $_.admissionName -ne "verified" -or $_.resultAbiName -ne "value" -or
         $_.errorAbiName -ne "no-raise" -or $_.typeQueryTarget -le 0
     }).Count -ne 0 ) {
    throw "XLIR dynamic TypeId typed JSON has no exact runtime query/map ABI"
}

& $xl ir $dynamicSource --lowered --format text --emit $dynamicLoweredTextPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic TypeId lowered text IR failed" }
$dynamicLoweredText = Read-Utf8 $dynamicLoweredTextPath
foreach ( $required in @(
    "runtime=xvalue-type-bind",
    "identity=t",
    "runtime=xvalue-type-id"
) ) {
    if ( ! $dynamicLoweredText.Contains($required) ) {
        throw "XLIR dynamic TypeId lowered identity is incomplete: $required"
    }
}

& $xl ir $dynamicSource --lowered --format json --emit $dynamicLoweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic TypeId lowered JSON IR failed" }
$dynamicLoweredJson = Read-Utf8 $dynamicLoweredJsonPath | ConvertFrom-Json
$dynamicBinds = @($dynamicLoweredJson.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-type-bind"
})
$dynamicReads = @($dynamicLoweredJson.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-type-id"
})
if ( $dynamicBinds.Count -lt 1 -or $dynamicReads.Count -ne 6 -or
     @($dynamicBinds | Where-Object { $_.runtimeIdentityType -le 0 }).Count -ne 0 ) {
    throw "XLIR dynamic TypeId lowered JSON has no explicit bind/query operations"
}

& $xl cgen $dynamicSource --backend xlir --emit $dynamicCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic TypeId Cgen failed" }
$dynamicGenerated = Read-Utf8 $dynamicCPath
foreach ( $required in @(
    "extern uint64_t xrtValueTypeId(const xvalue*);",
    "extern bool xrtValueTypeIdBind(xvalue*, uint64_t);",
    "static xvalue* xlir_xvalue_type_bind(xvalue* value, uint64_t typeId)",
    "static uint64_t xlir_xvalue_type_id(const xvalue* value)",
    "case 2: return 0xE99D3F7F03464F1CULL;"
) ) {
    if ( ! $dynamicGenerated.Contains($required) ) {
        throw "XLIR dynamic TypeId mechanical C emission is incomplete: $required"
    }
}

& $xl ir $source --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR TypeId typed JSON IR failed" }
$typedJson = Read-Utf8 $typedJsonPath | ConvertFrom-Json
$typeIdTypes = @($typedJson.types | Where-Object { $_.cAbi -eq 35 })
$typeQueries = @($typedJson.intrinsicCalls | Where-Object { $_.kindName -eq "type-of" })
if ( $typeIdTypes.Count -ne 1 -or
     $typeIdTypes[0].ownershipAbi -ne 1 -or
     $typeIdTypes[0].typeValueId -ne "0xEFF6483752293D0B" -or
     $typeIdTypes[0].errorResultCName -ne "xlir_result_type_id" -or
     $typeQueries.Count -ne 1 -or
     $typeQueries[0].admissionName -ne "verified" -or
     $typeQueries[0].resultAbiName -ne "value" -or
     $typeQueries[0].errorAbiName -ne "no-raise" -or
     $typeQueries[0].typeQueryAbiName -ne "static-id" -or
     $typeQueries[0].typeQueryTarget -le 0 ) {
    throw "XLIR TypeId typed JSON has no exact static query/value identity ABI"
}

& $xl ir $source --lowered --format text --emit $loweredTextPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR TypeId lowered text IR failed" }
$loweredText = Read-Utf8 $loweredTextPath
foreach ( $required in @(
    "global-export g0",
    "cname=xlir_type_int value=0xE99D3F7F03464F1CULL",
    "extern-global x0",
    "cname=xlir_type_external",
    "async-result=copy-type-id",
    "async=copy-type-id",
    "cell-new",
    "cell-load",
    "cell-store",
    "immediate=0x4955F0967ED4EF16ULL",
    "discard opcode=none"
) ) {
    if ( ! $loweredText.Contains($required) ) {
        throw "XLIR TypeId lowered ABI is incomplete: $required"
    }
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR TypeId lowered JSON IR failed" }
$loweredJson = Read-Utf8 $loweredJsonPath | ConvertFrom-Json
if ( $loweredJson.globalExportCount -ne 3 -or
     $loweredJson.externGlobalCount -ne 1 -or
     @($loweredJson.globalExports | Where-Object {
         $_.initializer -notmatch '^0x[0-9A-F]{16}ULL$'
     }).Count -ne 0 -or
     @($loweredJson.externGlobals | Where-Object {
         $_.accessAbi -ne "read-write"
     }).Count -ne 0 ) {
    throw "XLIR TypeId lowered JSON has no exact global/extern storage ABI"
}

& $xl cgen $source --backend xlir --emit $cPath --emit-header $headerPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR TypeId Cgen failed" }
$generated = Read-Utf8 $cPath
$header = Read-Utf8 $headerPath
foreach ( $required in @(
    "typedef struct xlir_result_type_id { bool ok; uint64_t value; } xlir_result_type_id;",
    "static uint64_t xlir_future_await_type_id(xfuture* future)",
    "static xlir_cell* xlir_cell_new_type_id(void)",
    "static uint64_t xlir_cell_load_type_id(const xlir_cell* cell)",
    "static void xlir_cell_store_type_id(xlir_cell* cell, uint64_t value)",
    "const uint64_t xlir_type_int = 0xE99D3F7F03464F1CULL;",
    "extern uint64_t xlir_type_external;",
    "xlir_result_type_id result = xlir_fn_"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR TypeId C emission is incomplete: $required"
    }
}
foreach ( $required in @(
    "extern const uint64_t xlir_type_int;",
    "extern uint64_t xlir_type_async;",
    "extern uint64_t xlir_type_closure;",
    "uint64_t xlir_type_identity(uint64_t value);",
    "uint64_t xlir_type_of_int(int64_t value);",
    "uint64_t xlir_type_read_external(void);",
    "uint64_t xlir_type_fail(void);"
) ) {
    if ( ! $header.Contains($required) ) {
        throw "XLIR TypeId public header is incomplete: $required"
    }
}

foreach ( $case in $historicalCases ) {
    $caseSource = Join-Path $Root $case.Source
    $caseC = Join-Path $outputDir ($case.Name + ".xlir_type.c")
    & $xl cgen $caseSource --backend xlir --emit $caseC
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR historical TypeId Cgen failed: $($case.Name)"
    }
}

$compileCommands = @(
    'call test\suites\common.bat',
    ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror "{0}" !XL6_CGEN_LINKS! -o "{1}"' -f $hostSource, $exePath),
    ('"{0}"' -f $exePath),
    ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include "{0}" -include test\cases\cgen_class_host.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o "{1}"' -f $dynamicCPath, $dynamicExePath),
    ('"{0}"' -f $dynamicExePath)
)
foreach ( $case in $historicalCases ) {
    $caseExe = Join-Path $outputDir ($case.Name + ".xlir_type.exe")
    $compileCommands += ('!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -D{0} "{1}" !XL6_CGEN_LINKS! -o "{2}"' -f $case.Define, $historicalHost, $caseExe)
    $compileCommands += ('"{0}"' -f $caseExe)
}
Push-Location $Root
try {
    & $env:ComSpec /d /v:on /c ($compileCommands -join ' && ')
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR TypeId strict-C/runtime compatibility gate failed" }
} finally {
    Pop-Location
}

Write-Output "XLIR TypeId ABI gate ok: static and dynamic identity, runtime tag map/binding, globals/externs, result carrier, async/capture, exact historical hashes, strict C, and runtime contracts."
exit 0
