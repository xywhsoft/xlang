param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_string_membership.xl"
$hostSource = Join-Path $Root "test\cases\xlir_string_membership_host.c"
$compatSource = Join-Path $Root "test\cases\cgen_in_containers.xl"
$outputDir = Join-Path $Root "build\test"
$textPath = Join-Path $outputDir "xlir_string_membership.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_string_membership.lowered.json"
$cPath = Join-Path $outputDir "xlir_string_membership_generated.c"
$compatCPath = Join-Path $outputDir "cgen_in_containers_string_membership.c"
$exePath = Join-Path $outputDir "xlir_string_membership_host.exe"

foreach ( $path in @($xl, $source, $hostSource, $compatSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR string membership gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format text --emit $textPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string membership text IR failed" }
$text = [System.IO.File]::ReadAllText($textPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($text, "runtime=string-contains")).Count -ne 5 -or
     ([regex]::Matches($text, "kind=temporary")).Count -ne 8 ) {
    throw "XLIR string membership has no exact operation/temporary/release ABI"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string membership JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$ops = @($json.instructions | Where-Object { $_.runtimeOpName -eq "string-contains" })
$temporarySymbols = @{}
foreach ( $storage in @($json.storages | Where-Object { $_.kindName -eq "temporary" }) ) {
    $temporarySymbols[[string]$storage.symbol] = $storage
}
$allReleases = @($json.instructions | Where-Object { $_.kindName -eq "release" })
$releases = @($allReleases | Where-Object { $temporarySymbols.ContainsKey([string]$_.targetSymbol) })
if ( $ops.Count -ne 5 -or @($ops | Where-Object {
        $_.kindName -ne "runtime-call" -or $_.operandCount -ne 2 -or
        $_.hasImmediate -ne 0 -or $_.targetSymbol -ne 0 -or
        $_.targetValue -ne 0 -or $_.calleeType -ne 0 -or
        $_.payloadType -ne 0 -or $_.opcodeName -ne "none"
    }).Count -ne 0 -or
    $temporarySymbols.Count -ne 8 -or $releases.Count -ne 8 -or
    @($releases | Where-Object {
        $_.operandCount -ne 0 -or ! $temporarySymbols.ContainsKey([string]$_.targetSymbol)
    }).Count -ne 0 ) {
    throw "XLIR string membership JSON has no exact two-borrow bool ownership ABI"
}
foreach ($storage in $temporarySymbols.Values) {
    $exact = @($releases | Where-Object { $_.targetSymbol -eq $storage.symbol })
    if ($exact.Count -ne 1 -or $exact[0].type -ne $storage.type) {
        throw "XLIR string membership must close every exact temporary owner once"
    }
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string membership Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
if ( ! $generated.Contains("extern bool xrtStrContains(xlir_string_view, xlir_string_view);") -or
     ([regex]::Matches($generated,
        "xrtStrContains\(xrtStrView\(v[0-9]+\), xrtStrView\(v[0-9]+\)\)")).Count -ne 5 -or
     # Module finalization also releases the persistent source string. Match
     # the complete IR release program, not the historical temporary-only sum.
     ([regex]::Matches($generated, "xrtFree\(s[0-9]+\)")).Count -ne $allReleases.Count ) {
    throw "XLIR string membership C rendering is not mechanical from STRING_CONTAINS/RELEASE"
}

# This established mixed-container program was previously blocked at its first
# static string `in`; accepting it proves that the dedicated operation composes
# with the separate xvalue membership ABI in one module.
& $xl cgen $compatSource --backend xlir --emit $compatCPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR mixed string/xvalue container membership compatibility Cgen failed"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string membership strict generated-C runtime failed" }

Write-Output "XLIR string membership gate ok: verified borrowed haystack/needle ordering, owned-rvalue TEMPORARY/RELEASE, direct xrtStrContains rendering, mixed static-string/xvalue container composition, strict C and runtime."
exit 0
