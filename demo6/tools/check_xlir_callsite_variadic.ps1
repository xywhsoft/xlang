param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_callsite_variadic.xl"
$legacyFixture = Join-Path $Root "test\cases\cgen_extern_xvalue_vararg.xl"
$hostSource = Join-Path $Root "test\cases\xlir_callsite_variadic_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_callsite_variadic.typed.xlir"
$loweredPath = Join-Path $outputDir "xlir_callsite_variadic.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_callsite_variadic.lowered.json"
$cPath = Join-Path $outputDir "xlir_callsite_variadic_generated.c"
$headerPath = Join-Path $outputDir "xlir_callsite_variadic_generated.h"
$legacyCPath = Join-Path $outputDir "cgen_extern_xvalue_vararg.xlir.c"
$exePath = Join-Path $outputDir "xlir_callsite_variadic_host.exe"

foreach ( $path in @($xl, $source, $legacyFixture, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR variadic CallSite gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format text --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR variadic CallSite typed IR failed" }
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($typed, '(?m)^  call-site ')).Count -ne 4 -or
     ([regex]::Matches($typed, 'fixed=1 variadic=array')).Count -ne 2 -or
     ([regex]::Matches($typed, 'fixed=1 variadic=dict')).Count -ne 2 -or
     ([regex]::Matches($typed, 'source=array-vararg-item')).Count -ne 3 -or
     ([regex]::Matches($typed, 'source=dict-vararg-item')).Count -ne 2 ) {
    throw "XLIR typed variadic CallSite table has no canonical ARRAY/DICT tail slices"
}

& $xl ir $source --lowered --format text --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR variadic CallSite lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($lowered, 'runtime=xvalue-array-new')).Count -ne 2 -or
     ([regex]::Matches($lowered, 'runtime=xvalue-array-append-new')).Count -ne 3 -or
     ([regex]::Matches($lowered, 'runtime=xvalue-object-new')).Count -ne 2 -or
     ([regex]::Matches($lowered, 'runtime=xvalue-object-set-literal-new')).Count -ne 2 -or
     ([regex]::Matches($lowered, '(?m)^      i[0-9]+ call .*operands=\[v[0-9]+,v[0-9]+\]')).Count -ne 4 ) {
    throw "XLIR variadic CallSite lowering did not materialize exact erased-container CALL operands"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR variadic CallSite JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
if ( $json.callSiteCount -ne 4 -or $json.callArgumentCount -ne 9 ) {
    throw "XLIR JSON variadic CallSite arena counts are incomplete"
}
$expectedAbis = @("array", "array", "dict", "dict")
$expectedSources = @(
    "explicit,array-vararg-item,array-vararg-item,array-vararg-item",
    "explicit",
    "explicit,dict-vararg-item,dict-vararg-item",
    "explicit"
)
$expectedParameters = @("0,1,1,1", "0", "0,1,1", "0")
for ( $i = 0; $i -lt 4; ++$i ) {
    $site = $json.callSites[$i]
    $slice = @($json.callArguments | Select-Object -Skip $site.firstArgument -First $site.argumentCount)
    if ( $site.fixedParameterCount -ne 1 -or
         $site.variadicAbiName -ne $expectedAbis[$i] -or
         (($slice | ForEach-Object { $_.sourceName }) -join ',') -ne $expectedSources[$i] -or
         (($slice | ForEach-Object { $_.parameter }) -join ',') -ne $expectedParameters[$i] ) {
        throw "XLIR JSON variadic CallSite mismatch at site $i"
    }
}

& $xl cgen $source --backend xlir --emit $cPath --emit-header $headerPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR variadic CallSite Cgen failed" }
& $xl cgen $legacyFixture --backend xlir --emit $legacyCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR existing extern xvalue vararg fixture no longer lowers through CallSite" }
$header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
if ( ! $header.Contains("int64_t xlir_callsite_variadic_probe(void);") ) {
    throw "XLIR variadic CallSite export header is missing"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR variadic CallSite generated-C runtime failed" }

Write-Output "XLIR variadic CallSite gate ok: ARRAY/DICT tails, empty aggregates, text/JSON IR, strict C, existing externs, and runtime."
exit 0
