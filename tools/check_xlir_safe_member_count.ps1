param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_safe_member.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "cgen_safe_member.gate.typed.json"
$loweredPath = Join-Path $outputDir "cgen_safe_member.gate.lowered.json"
$cPath = Join-Path $outputDir "cgen_safe_member.gate.c"
$exePath = Join-Path $outputDir "cgen_safe_member.gate.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR safe-member count gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

Push-Location $Root
try {
    & $xl ir $source --format json --emit $typedPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR safe-member typed JSON failed" }
    $typed = [System.IO.File]::ReadAllText(
        $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $safeMembers = @($typed.nodes | Where-Object {
        $_.sourceKindName -eq "safe_member"
    })
    if ( $safeMembers.Count -ne 6 -or
         @($safeMembers | Where-Object {
             $_.memberAbiName -eq "safe-xvalue-count-tagged-nullable"
         }).Count -ne 3 -or
         @($safeMembers | Where-Object {
             $_.memberAbiName -eq "safe-xvalue-count-xvalue"
         }).Count -ne 3 -or
         @($safeMembers | Where-Object { $_.memberAbiName -eq "none" }).Count -ne 0 ) {
        throw "Typed XLIR safe length members do not have two exact null-result ABIs"
    }

    & $xl ir $source --lowered --format json --emit $loweredPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR safe-member lowering failed" }
    $lowered = [System.IO.File]::ReadAllText(
        $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $expectedOps = [ordered]@{
        "xvalue-count-tagged-nullable" = 3
        "xvalue-count-or-null" = 3
    }
    foreach ( $entry in $expectedOps.GetEnumerator() ) {
        $operations = @($lowered.instructions | Where-Object {
            $_.runtimeOpName -eq $entry.Key
        })
        if ( $operations.Count -ne $entry.Value -or
             @($operations | Where-Object {
                 $_.kindName -ne "runtime-call" -or $_.operandCount -ne 1 -or
                 $_.hasImmediate -ne 0 -or $_.targetSymbol -ne 0 -or
                 $_.targetValue -ne 0 -or $_.calleeType -ne 0 -or
                 $_.payloadType -ne 0 -or $_.opcodeName -ne "none"
             }).Count -ne 0 ) {
            throw "Lowered XLIR safe-member operation is not exact: $($entry.Key)"
        }
    }
    foreach ( $line in @(21, 22) ) {
        $member = @($lowered.nodes | Where-Object {
            $_.sourceKindName -eq "safe_member" -and $_.line -eq $line
        })
        $operation = @($lowered.instructions | Where-Object {
            $_.origin -eq $member[0].id -and
            $_.runtimeOpName -like "xvalue-count-*"
        })
        $release = @(if ( $operation.Count -eq 1 ) {
            $lowered.instructions | Where-Object {
                $_.index -eq ($operation[0].index + 1) -and
                $_.kindName -eq "release" -and $_.origin -eq $member[0].id
            }
        })
        if ( $member.Count -ne 1 -or $operation.Count -ne 1 -or
             $release.Count -ne 1 ) {
            throw "XLIR safe-member temporary receiver has no adjacent release at line $line"
        }
    }

    & $xl cgen $source --backend xlir --emit $cPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR safe-member Cgen failed" }
    $generated = [System.IO.File]::ReadAllText(
        $cPath, [System.Text.Encoding]::UTF8)
    foreach ( $required in @(
        "xrtValueCount(",
        "xrtValueType(",
        "xrtValueRetain(xrtValueNull())",
        ".hasValue = true"
    ) ) {
        if ( ! $generated.Contains($required) ) {
            throw "XLIR safe-member mechanical C is incomplete: $required"
        }
    }
    if ( $generated.Contains("safe_member") -or
         $generated.Contains("memberAbi") -or
         $generated.Contains("xl6_xrt_member_safe_length") ) {
        throw "XLIR safe-member Cgen retained source/legacy semantic recovery"
    }

    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR safe-member strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR safe-member count ABI gate ok: typed/dynamic null carriers, explicit RuntimeOps, temporary release, mechanical C, strict C, and runtime."
exit 0
