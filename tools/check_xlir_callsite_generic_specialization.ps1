param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_callsite_generic_specialization.xl"
$hostSource = Join-Path $Root "test\cases\xlir_callsite_generic_specialization_host.c"
$classProtocolSource = Join-Path $Root "test\cases\cgen_class_generic_handle.xl"
$classProtocolHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_callsite_generic_specialization.typed.xlir"
$loweredPath = Join-Path $outputDir "xlir_callsite_generic_specialization.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_callsite_generic_specialization.lowered.json"
$cPath = Join-Path $outputDir "xlir_callsite_generic_specialization_generated.c"
$headerPath = Join-Path $outputDir "xlir_callsite_generic_specialization_generated.h"
$exePath = Join-Path $outputDir "xlir_callsite_generic_specialization_host.exe"
$classProtocolTypedPath = Join-Path $outputDir "xlir_callsite_generic_class_protocol.typed.xlir"
$classProtocolLoweredPath = Join-Path $outputDir "xlir_callsite_generic_class_protocol.lowered.xlir"
$classProtocolCPath = Join-Path $outputDir "xlir_callsite_generic_class_protocol.c"
$classProtocolCRepeatPath = Join-Path $outputDir "xlir_callsite_generic_class_protocol.repeat.c"
$classProtocolExePath = Join-Path $outputDir "xlir_callsite_generic_class_protocol.exe"

foreach ( $path in @($xl, $source, $hostSource, $classProtocolSource, $classProtocolHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR generic CallSite specialization gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format text --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic CallSite typed IR failed" }
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($typed, '(?m)^  call-site ')).Count -ne 2 -or
     ([regex]::Matches($typed, '(?m)^  call-argument .* source=explicit ')).Count -ne 2 ) {
    throw "XLIR typed generic CallSite inputs are not canonical"
}

& $xl ir $source --lowered --format text --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic CallSite specialization lowering failed" }
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($lowered, '(?m)^  call-site ')).Count -ne 3 -or
     ([regex]::Matches($lowered, '(?m)^  call-argument .* source=explicit ')).Count -ne 3 -or
     ! $lowered.Contains('generic-instance source=') ) {
    throw "XLIR lowered specialization did not clone its owned CallSite slice"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic CallSite specialization JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$specializations = @($json.genericSpecializations)
$sites = @($json.callSites)
$targetGroups = @($sites | Group-Object targetSymbol | Sort-Object Count -Descending)
if ( $specializations.Count -ne 1 -or $json.callSiteCount -ne 3 -or
     $json.callArgumentCount -ne 3 -or @($sites.origin | Sort-Object -Unique).Count -ne 3 -or
     $targetGroups.Count -ne 2 -or $targetGroups[0].Count -ne 2 -or
     $targetGroups[1].Count -ne 1 -or
     @($sites | Where-Object { $_.targetSymbol -eq $specializations[0].symbol }).Count -ne 1 ) {
    throw "XLIR JSON generic specialization lost cloned or retargeted CallSite facts"
}

& $xl cgen $source --backend xlir --emit $cPath --emit-header $headerPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic CallSite specialization Cgen failed" }
$header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
if ( ! $header.Contains('int64_t xlir_callsite_generic_specialization_probe(void);') ) {
    throw "XLIR generic CallSite specialization export header is missing"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic CallSite specialization generated-C runtime failed" }

& $xl ir $classProtocolSource --format text --emit $classProtocolTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic class protocol typed IR failed" }
& $xl ir $classProtocolSource --lowered --format text --emit $classProtocolLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic class protocol lowering failed" }
$classTyped = [System.IO.File]::ReadAllText($classProtocolTypedPath, [System.Text.Encoding]::UTF8)
$classLowered = [System.IO.File]::ReadAllText($classProtocolLoweredPath, [System.Text.Encoding]::UTF8)
$index = [regex]::Match(
    $classTyped,
    '(?m)^  n(?<origin>[1-9][0-9]*) kind=index/28 .* call=protocol/9 .* target=(?<target>[1-9][0-9]*) .* @33:8$')
if ( !$index.Success ) { throw "generic __item__ syntax has no semantic target" }
$origin = $index.Groups['origin'].Value
$templateTarget = $index.Groups['target'].Value
$loweredIndex = [regex]::Match(
    $classLowered,
    '(?m)^  n' + $origin + ' kind=index/28 .* call=protocol/9 .* target=(?<target>[1-9][0-9]*) .* @33:8$')
if ( !$loweredIndex.Success -or
     $loweredIndex.Groups['target'].Value -eq $templateTarget -or
     ![regex]::IsMatch(
         $classLowered,
         '(?m)^  call-site .* origin=n' + $origin + ' kind=9 target=' +
         $loweredIndex.Groups['target'].Value + ' result=t[1-9][0-9]* receiver=n[1-9][0-9]* .* fixed=1 variadic=none dispatchKind=0 receiverAt=0$') -or
     ![regex]::IsMatch(
         $classLowered,
         '(?m)^  generic-instance source=n[1-9][0-9]* declaration=n[1-9][0-9]* symbol=' +
         $loweredIndex.Groups['target'].Value + ' args=\[t[1-9][0-9]*\]$') ) {
    throw "generic __item__ CallSite was not retargeted to one monomorphic class method"
}
if ( ![regex]::IsMatch(
         $classLowered,
         '(?m)^  n[1-9][0-9]* kind=binary/31 .* opcode=add operatorError=no-raise operatorExecution=t[1-9][0-9]* .* operatorOperands=exact,exact .* @15:27$') ) {
    throw "generic class operator body did not refresh its concrete execution ABI"
}

& $xl cgen $classProtocolSource --backend xlir --emit $classProtocolCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic class protocol Cgen failed" }
& $xl cgen $classProtocolSource --backend xlir --emit $classProtocolCRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "repeated XLIR generic class protocol Cgen failed" }
if ( [System.IO.File]::ReadAllText($classProtocolCPath, [System.Text.Encoding]::UTF8) -cne
     [System.IO.File]::ReadAllText($classProtocolCRepeatPath, [System.Text.Encoding]::UTF8) ) {
    throw "XLIR generic class protocol Cgen is not deterministic"
}
$classCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $classProtocolCPath $classProtocolHost !XL6_CGEN_LINKS! -o $classProtocolExePath && $classProtocolExePath"
& $env:ComSpec /d /v:on /c $classCommand
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic class protocol generated-C runtime failed" }

Write-Output "XLIR generic CallSite specialization gate ok: cloned arguments, retargeted site, ordinary and protocol syntax retargeting, concrete operator ABI refresh, JSON/text IR, deterministic strict C, and runtime."
exit 0
