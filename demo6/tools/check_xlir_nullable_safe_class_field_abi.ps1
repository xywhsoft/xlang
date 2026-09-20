param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\nullable_safe_navigation.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "nullable_safe_navigation.gate.typed.json"
$typedRepeatPath = Join-Path $outputDir "nullable_safe_navigation.gate.repeat.typed.json"
$loweredPath = Join-Path $outputDir "nullable_safe_navigation.gate.lowered.json"
$loweredRepeatPath = Join-Path $outputDir "nullable_safe_navigation.gate.repeat.lowered.json"
$cPath = Join-Path $outputDir "nullable_safe_navigation.gate.c"
$cRepeatPath = Join-Path $outputDir "nullable_safe_navigation.gate.repeat.c"
$legacyPath = Join-Path $outputDir "nullable_safe_navigation.gate.legacy.c"
$exePath = Join-Path $outputDir "nullable_safe_navigation.gate.exe"
$legacyExePath = Join-Path $outputDir "nullable_safe_navigation.gate.legacy.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR nullable safe-class-field gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Assert-SameBytes([string]$leftPath, [string]$rightPath, [string]$label) {
    $left = [System.IO.File]::ReadAllBytes($leftPath)
    $right = [System.IO.File]::ReadAllBytes($rightPath)
    if ( ! [System.Linq.Enumerable]::SequenceEqual([byte[]]$left, [byte[]]$right) ) {
        throw "XLIR nullable safe-class-field $label is not byte deterministic"
    }
}

Push-Location $Root
try {
    & $xl ir $source --format json --emit $typedPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable safe-class-field typed JSON failed" }
    & $xl ir $source --format json --emit $typedRepeatPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable safe-class-field repeated typed JSON failed" }
    Assert-SameBytes $typedPath $typedRepeatPath "Typed IR"

    $typed = [System.IO.File]::ReadAllText(
        $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $safeFields = @($typed.nodes | Where-Object {
        $_.sourceKindName -eq "safe_member" -and
        $_.memberAbiName -eq "safe-class-field-nullable"
    })
    if ( $safeFields.Count -ne 2 -or
         @($safeFields | Where-Object { $_.line -eq 23 }).Count -ne 1 -or
         @($safeFields | Where-Object { $_.line -eq 27 }).Count -ne 1 -or
         @($typed.nodes | Where-Object {
             $_.sourceKindName -eq "safe_member" -and
             $_.memberAbiName -eq "none"
         }).Count -ne 0 ) {
        throw "Typed XLIR did not freeze both declared safe fields to one exact member ABI"
    }
    foreach ( $member in $safeFields ) {
        $resultType = @($typed.types | Where-Object { $_.id -eq $member.storageType })
        if ( $resultType.Count -ne 1 -or $resultType[0].kind -ne 8 -or
             @($resultType[0].params).Count -ne 1 ) {
            throw "Typed XLIR safe field result is not one exact nullable TypeId at line $($member.line)"
        }
    }

    & $xl ir $source --lowered --format json --emit $loweredPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable safe-class-field lowering failed" }
    & $xl ir $source --lowered --format json --emit $loweredRepeatPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable safe-class-field repeated lowering failed" }
    Assert-SameBytes $loweredPath $loweredRepeatPath "Lowered IR"

    $lowered = [System.IO.File]::ReadAllText(
        $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $expectedProjection = @{
        23 = "xvalue-project-tagged-nullable-copy"
        27 = "xvalue-project-nullable-string-copy"
    }
    $classIdentity = 0
    foreach ( $member in $safeFields ) {
        $lookup = @($lowered.instructions | Where-Object {
            $_.origin -eq $member.id -and
            $_.runtimeOpName -eq "xvalue-safe-class-field-get-literal"
        })
        $projection = @($lowered.instructions | Where-Object {
            $_.origin -eq $member.id -and
            $_.runtimeOpName -eq $expectedProjection[$member.line]
        })
        $resultType = @($typed.types | Where-Object { $_.id -eq $member.storageType })[0]
        $release = @(if ( $projection.Count -eq 1 ) {
            $lowered.instructions | Where-Object {
                $_.index -eq ($projection[0].index + 1) -and
                $_.origin -eq $member.id -and $_.kindName -eq "release"
            }
        })
        if ( $lookup.Count -ne 1 -or $projection.Count -ne 1 -or
             $lookup[0].kindName -ne "runtime-call" -or
             $lookup[0].operandCount -ne 1 -or $lookup[0].hasImmediate -ne 1 -or
             $lookup[0].targetSymbol -ne 0 -or $lookup[0].targetValue -ne 0 -or
             $lookup[0].calleeType -ne 0 -or $lookup[0].opcodeName -ne "none" -or
             $lookup[0].payloadType -ne $resultType.params[0] -or
             $lookup[0].runtimeIdentityType -eq 0 -or
             $projection[0].index -ne ($lookup[0].index + 1) -or
             $release.Count -ne 1 ) {
            throw "Lowered XLIR safe field has no exact lookup/project/release sequence at line $($member.line)"
        }
        if ( $classIdentity -eq 0 ) { $classIdentity = $lookup[0].runtimeIdentityType }
        if ( $lookup[0].runtimeIdentityType -ne $classIdentity ) {
            throw "Lowered XLIR safe fields do not share the frozen declared receiver class TypeId"
        }
    }
    if ( @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-safe-class-field-get-literal"
    }).Count -ne 2 ) {
        throw "Lowered XLIR emitted an unexpected safe declared-class lookup count"
    }

    & $xl cgen $source --backend xlir --emit $cPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable safe-class-field Cgen failed" }
    & $xl cgen $source --backend xlir --emit $cRepeatPath
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable safe-class-field repeated Cgen failed" }
    Assert-SameBytes $cPath $cRepeatPath "generated C"
    $generated = [System.IO.File]::ReadAllText(
        $cPath, [System.Text.Encoding]::UTF8)
    foreach ( $required in @(
        "xlir_xvalue_safe_class_field_get_or_null",
        "if (source == NULL || xrtValueType(source) == 0) return xrtValueNull();",
        'xrtStrView("value")', 'xrtStrView("name")',
        "xlir_xvalue_project_", "xlir_xvalue_project_nullable_string_copy"
    ) ) {
        if ( ! $generated.Contains($required) ) {
            throw "XLIR nullable safe-class-field mechanical C is incomplete: $required"
        }
    }
    foreach ( $forbidden in @("memberAbi", "sourceKind", "XL6_AST_SAFE_MEMBER") ) {
        if ( $generated.Contains($forbidden) ) {
            throw "XLIR nullable safe-class-field Cgen retained semantic recovery: $forbidden"
        }
    }

    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR nullable safe-class-field strict-C/runtime gate failed"
    }

    & $xl cgen $source --backend legacy --emit $legacyPath
    if ( $LASTEXITCODE -ne 0 ) { throw "Legacy nullable safe-class-field Cgen failed" }
    $legacyCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror `"$legacyPath`" !XL6_CGEN_XLANG_LINKS! -o `"$legacyExePath`" && `"$legacyExePath`""
    & $env:ComSpec /d /v:on /c $legacyCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR nullable safe-class-field legacy differential runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR nullable safe declared-class field ABI gate ok: deterministic Typed/Lowered/C, exact class and payload TypeIds, lookup/project/release ownership, mechanical C, strict runtime, and legacy differential runtime."
exit 0
