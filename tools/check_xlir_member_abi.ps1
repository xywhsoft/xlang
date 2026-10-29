param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_member_abi_string_length.xl"
$hostSource = Join-Path $Root "test\cases\xlir_member_abi_string_length_host.c"
$outputDir = Join-Path $Root "build\test"
$typedJsonPath = Join-Path $outputDir "xlir_member_abi_string_length.typed.json"
$loweredJsonPath = Join-Path $outputDir "xlir_member_abi_string_length.lowered.json"
$cPath = Join-Path $outputDir "xlir_member_abi_string_length_generated.c"
$exePath = Join-Path $outputDir "xlir_member_abi_string_length_host.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR member ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR member ABI typed JSON failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$members = @($typed.nodes | Where-Object {
    $_.memberAbiName -eq "string-utf8-count"
})
if ( $members.Count -ne 4 -or @($members | Where-Object {
        $_.memberAbi -ne 1 -or $_.kind -ne 27 -or $_.opcodeName -ne "none"
    }).Count -ne 0 -or
    @($typed.nodes | Where-Object {
        $_.memberAbiName -ne "none" -and
        $_.memberAbiName -ne "string-utf8-count"
    }).Count -ne 0 ) {
    throw "Typed XLIR has no exact canonical string UTF-8 member ABI"
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR member ABI lowered JSON failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$ops = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "string-utf8-count"
})
if ( $ops.Count -ne 4 -or @($ops | Where-Object {
        $_.kindName -ne "runtime-call" -or $_.operandCount -ne 1 -or
        $_.hasImmediate -ne 0 -or $_.targetSymbol -ne 0 -or
        $_.targetValue -ne 0 -or $_.calleeType -ne 0 -or
        $_.payloadType -ne 0 -or $_.opcodeName -ne "none"
    }).Count -ne 0 ) {
    throw "Lowered XLIR has no exact one-borrow/i64 string UTF-8 count ABI"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR member ABI Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
if ( ! $generated.Contains("extern size_t xrtUtf8Count(xlir_string_view);") -or
     ([regex]::Matches($generated,
        "\(int64_t\)xrtUtf8Count\(xrtStrView\(v[0-9]+\)\)")).Count -ne 4 -or
     $generated.Contains(".length") ) {
    throw "XLIR string length C rendering is not mechanical from STRING_UTF8_COUNT"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR member ABI strict generated-C runtime failed"
}

Write-Output "XLIR member ABI gate ok: string.length is a verified string-utf8-count Typed ABI, a one-borrow/i64 runtime op, mechanical C, strict C, and Unicode runtime semantics."
exit 0
