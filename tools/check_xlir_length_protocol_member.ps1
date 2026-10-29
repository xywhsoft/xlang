param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_length_protocol_member.xl"
$hostSource = Join-Path $Root "test\cases\xlir_length_protocol_member_host.c"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$bufferSource = Join-Path $Root "test\cases\stdlib_buffer.xl"
$outputDir = Join-Path $Root "build\test"
$typedJsonPath = Join-Path $outputDir "xlir_length_protocol_member.typed.json"
$bufferTypedJsonPath = Join-Path $outputDir "xlir_length_protocol_buffer.typed.json"
$loweredJsonPath = Join-Path $outputDir "xlir_length_protocol_member.lowered.json"
$cPath = Join-Path $outputDir "xlir_length_protocol_member_generated.c"
$exePath = Join-Path $outputDir "xlir_length_protocol_member_host.exe"

foreach ( $path in @($xl, $source, $hostSource, $genericHost, $bufferSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR length protocol member gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR length protocol member typed JSON failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$members = @($typed.nodes | Where-Object {
    $_.memberAbiName -eq "length-protocol-call"
})
if ( $members.Count -ne 2 -or @($members | Where-Object {
        $_.memberAbi -ne 2 -or $_.kind -ne 27 -or
        $_.targetSymbol -eq 0 -or $_.type -ne $_.storageType
    }).Count -ne 0 -or @($members.targetSymbol | Select-Object -Unique).Count -ne 2 ) {
    throw "Typed XLIR has no exact source-class/native-class length protocol ABI"
}

# The standard-library Buffer is deliberately a separate typed probe: its
# broader fixture still contains a non-range iterator loop, but both `.length`
# sites must already resolve to the same canonical `__len__` method here.
& $xl ir $bufferSource --format json --emit $bufferTypedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR Buffer length protocol typed JSON failed" }
$bufferTyped = [System.IO.File]::ReadAllText(
    $bufferTypedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$bufferMembers = @($bufferTyped.nodes | Where-Object {
    $_.memberAbiName -eq "length-protocol-call"
})
if ( $bufferMembers.Count -ne 2 -or @($bufferMembers | Where-Object {
        $_.targetSymbol -eq 0 -or $_.type -ne $_.storageType
    }).Count -ne 0 -or @($bufferMembers.targetSymbol | Select-Object -Unique).Count -ne 1 ) {
    throw "Typed XLIR Buffer.length sites do not share one exact protocol target"
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR length protocol member lowered JSON failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
foreach ( $member in $members ) {
    $calls = @($lowered.instructions | Where-Object {
        $_.origin -eq $member.id -and $_.kindName -eq "call"
    })
    if ( $calls.Count -ne 1 -or $calls[0].targetSymbol -ne $member.targetSymbol -or
         $calls[0].type -ne $member.storageType -or
         $calls[0].operandCount -ne 1 -or $calls[0].runtimeOpName -ne "none" ) {
        throw "Lowered XLIR length protocol member is not one exact direct CALL"
    }
    $targets = @($lowered.callTargets | Where-Object {
        $_.symbol -eq $member.targetSymbol
    })
    if ( $targets.Count -ne 1 -or $targets[0].kind -ne 1 -or
         $targets[0].returnType -ne $member.storageType -or
         $targets[0].parameterCount -ne 1 -or
         @($targets[0].parameterTypes).Count -ne 1 -or
         @($targets[0].parameterAbis).Count -ne 1 -or
         $targets[0].parameterAbis[0] -ne 2 ) {
        throw "Lowered XLIR length protocol target has no one-borrow/result ABI"
    }
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR length protocol member Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $targetSymbol in @($members.targetSymbol) ) {
    if ( ([regex]::Matches($generated,
            "= xlir_fn_$targetSymbol\(v[0-9]+\);")).Count -ne 1 ) {
        throw "XLIR length protocol C is not mechanical from its direct target"
    }
}
if ( $generated.Contains(".length") -or
     $generated.Contains("xlir_xvalue_count(" ) ) {
    throw "XLIR length protocol C recovered source member/dynamic-field semantics"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include `"$cPath`" -include `"$hostSource`" `"$genericHost`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR length protocol member strict generated-C runtime failed"
}

Write-Output "XLIR length protocol member gate ok: class and native-class length resolve to exact __len__ targets, lower as one-borrow direct calls, and execute through mechanical C."
exit 0
