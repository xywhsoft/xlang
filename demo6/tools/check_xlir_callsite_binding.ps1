param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_callsite_binding.xl"
$legacyFixture = Join-Path $Root "test\cases\call_defaults.xl"
$hostSource = Join-Path $Root "test\cases\xlir_callsite_binding_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_callsite_binding.typed.xlir"
$loweredPath = Join-Path $outputDir "xlir_callsite_binding.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_callsite_binding.lowered.json"
$cPath = Join-Path $outputDir "xlir_callsite_binding_generated.c"
$headerPath = Join-Path $outputDir "xlir_callsite_binding_generated.h"
$legacyCPath = Join-Path $outputDir "call_defaults.callsite_generated.c"
$exePath = Join-Path $outputDir "xlir_callsite_binding_host.exe"

foreach ( $path in @($xl, $source, $legacyFixture, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR CallSite gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format text --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR CallSite typed IR failed" }
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($typed, '(?m)^  call-site ')).Count -lt 5 -or
     ([regex]::Matches($typed, '(?m)^  call-argument .* source=default ')).Count -ne 4 -or
     ([regex]::Matches($typed, '(?m)^  call-argument .* source=explicit ')).Count -lt 9 -or
     ([regex]::Matches($typed, 'arguments=[0-9]+\+3 fixed=3 variadic=none')).Count -lt 3 ) {
    throw "XLIR typed CallSite table has no canonical explicit/default binding slices"
}

& $xl ir $source --lowered --format text --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR CallSite lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches(
        $lowered, 'call opcode=none runtime=none v[0-9]+ type=[0-9]+ target=1 .*operands=\[v[0-9]+,v[0-9]+,v[0-9]+\]')).Count -ne 3 ) {
    throw "XLIR CallSite lowering did not emit three canonical positional CALL operands"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR CallSite JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
if ( $json.callSiteCount -lt 5 -or $json.callArgumentCount -lt 13 -or
     @($json.callSites).Count -ne $json.callSiteCount -or
     @($json.callArguments).Count -ne $json.callArgumentCount ) {
    throw "XLIR JSON CallSite arena counts are incomplete"
}
$expectedSources = @(
    "explicit,default,default",
    "explicit,default,explicit",
    "explicit,explicit,explicit",
    "explicit,explicit",
    "default,explicit"
)
for ( $i = 0; $i -lt 5; ++$i ) {
    $site = $json.callSites[$i]
    $slice = @($json.callArguments | Select-Object -Skip $site.firstArgument -First $site.argumentCount)
    $sources = ($slice | ForEach-Object { $_.sourceName }) -join ','
    $parameters = ($slice | ForEach-Object { $_.parameter }) -join ','
    $expectedParameters = if ( $i -lt 3 ) { "0,1,2" } else { "0,1" }
    $expectedFixed = if ( $i -lt 3 ) { 3 } else { 2 }
    if ( $site.variadicAbiName -ne "none" -or $site.fixedParameterCount -ne $expectedFixed -or
         $sources -ne $expectedSources[$i] -or $parameters -ne $expectedParameters ) {
        throw "XLIR JSON CallSite canonical binding mismatch at site $i"
    }
}

& $xl cgen $source --backend xlir --emit $cPath --emit-header $headerPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR CallSite Cgen failed" }
& $xl cgen $legacyFixture --backend xlir --emit $legacyCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR existing default-argument fixture no longer uses canonical CallSite binding" }
$header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
if ( ! $header.Contains("int64_t xlir_callsite_binding_probe(void);") ) {
    throw "XLIR CallSite export header is missing"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR CallSite generated-C runtime failed" }

Write-Output "XLIR CallSite binding gate ok: functions/constructors, explicit/default sources, canonical parameter order, text/JSON IR, strict C, and runtime."
exit 0
