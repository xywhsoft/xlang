param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$fixtures = @(
    "cgen_fstring.xl",
    "cgen_loop_fstring_needs.xl",
    "cgen_generator_fstring_needs.xl"
)
$conversionOps = @(
    "string-copy", "bool-to-string", "i64-to-string",
    "u64-to-string", "f64-to-string", "xvalue-to-string"
)

foreach ( $path in @($xl, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR interpolated-string gate input is missing: $path"
    }
}
foreach ( $fixture in $fixtures ) {
    $source = Join-Path $Root "test\cases\$fixture"
    if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
        throw "XLIR interpolated-string fixture is missing: $source"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

foreach ( $fixture in $fixtures ) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($fixture)
    $source = Join-Path $Root "test\cases\$fixture"
    $typedPath = Join-Path $outputDir "$name.interpolated-string.typed.json"
    $loweredPath = Join-Path $outputDir "$name.interpolated-string.lowered.json"
    $cPath = Join-Path $outputDir "$name.interpolated-string.c"
    $repeatCPath = Join-Path $outputDir "$name.interpolated-string.repeat.c"
    $exePath = Join-Path $outputDir "$name.interpolated-string.exe"

    & $xl ir $source --typed --format json --emit $typedPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR interpolated-string typed JSON failed: $fixture"
    }
    $typed = [System.IO.File]::ReadAllText(
        $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    if ( [int]$typed.nodeCount -ne @($typed.nodes).Count -or
         [int]$typed.nodeChildCount -ne @($typed.nodeChildren).Count ) {
        throw "XLIR typed node/child arena dump count mismatch: $fixture"
    }
    $interpolations = @($typed.nodes | Where-Object {
        $_.kindName -eq "interpolated-string"
    })
    if ( $interpolations.Count -eq 0 -or
         @($interpolations | Where-Object {
             $_.sourceKindName -ne "interp_string" -or
             $_.childCount -eq 0 -or
             $_.firstChild -lt 0 -or
             $_.firstChild + $_.childCount -gt $typed.nodeChildCount
         }).Count -ne 0 -or
         @($typed.nodes | Where-Object {
             $_.sourceKindName -eq "interp_string" -and
             $_.kindName -ne "interpolated-string"
         }).Count -ne 0 ) {
        throw "XLIR typed interpolation node identity or child range is ambiguous: $fixture"
    }

    & $xl ir $source --lowered --format json --emit $loweredPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR interpolated-string lowering failed: $fixture"
    }
    $lowered = [System.IO.File]::ReadAllText(
        $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    foreach ( $interpolation in $interpolations ) {
        $partIds = @()
        for ( $offset = 0; $offset -lt $interpolation.childCount; ++$offset ) {
            $partIds += [int]$typed.nodeChildren[
                [int]$interpolation.firstChild + $offset]
        }
        if ( @($partIds | Sort-Object -Unique).Count -ne $partIds.Count ) {
            throw "XLIR interpolation part order aliases a typed node: $fixture n$($interpolation.id)"
        }
        foreach ( $partId in $partIds ) {
            if ( $partId -le 0 -or $partId -gt $typed.nodeCount ) {
                throw "XLIR interpolation child id escapes the typed node arena: $fixture"
            }
            $part = $typed.nodes[$partId - 1]
            $runtime = @($lowered.instructions | Where-Object {
                $_.origin -eq $partId -and $_.kindName -eq "runtime-call"
            })
            if ( $part.sourceKindName -eq "interp_text" ) {
                if ( $runtime.Count -ne 1 -or
                     $runtime[0].runtimeOpName -ne "string-dup" -or
                     $runtime[0].hasImmediate -ne 1 ) {
                    throw "XLIR interpolation text is not one canonical STRING_DUP payload: $fixture n$partId"
                }
            } else {
                $conversions = @($runtime | Where-Object {
                    $_.runtimeOpName -in $conversionOps
                })
                if ( $conversions.Count -ne 1 ) {
                    throw "XLIR interpolation expression has no single explicit conversion ABI: $fixture n$partId"
                }
            }
        }
        $expectedConcat = [int]$interpolation.childCount - 1
        $parentInstructions = @($lowered.instructions | Where-Object {
            $_.origin -eq $interpolation.id
        })
        if ( @($parentInstructions | Where-Object {
                $_.kindName -eq "runtime-call" -and
                $_.runtimeOpName -eq "string-concat" -and
                $_.operandCount -eq 2
             }).Count -ne $expectedConcat -or
             @($parentInstructions | Where-Object {
                $_.kindName -eq "store"
             }).Count -ne 2 * $expectedConcat -or
             @($parentInstructions | Where-Object {
                $_.kindName -eq "load"
             }).Count -ne 2 * $expectedConcat -or
             @($parentInstructions | Where-Object {
                $_.kindName -eq "release"
             }).Count -ne 2 * $expectedConcat ) {
            throw "XLIR interpolation concat ownership transaction is not exact: $fixture n$($interpolation.id)"
        }
    }

    if ( $fixture -eq "cgen_fstring.xl" ) {
        foreach ( $required in @(
            "string-copy", "bool-to-string", "i64-to-string",
            "xvalue-to-string"
        ) ) {
            if ( @($lowered.instructions | Where-Object {
                    $_.runtimeOpName -eq $required
                 }).Count -eq 0 ) {
                throw "XLIR general interpolation fixture omits conversion ABI: $required"
            }
        }
    }
    if ( $fixture -eq "cgen_generator_fstring_needs.xl" -and
         (@($lowered.instructions | Where-Object {
             $_.kindName -eq "generator-item-get"
         }).Count -eq 0 -or
          @($lowered.instructions | Where-Object {
             $_.runtimeOpName -eq "i64-to-string"
          }).Count -ne 2) ) {
        throw "XLIR generator item projection does not feed explicit interpolation conversion"
    }

    & $xl cgen $source --backend xlir --emit $cPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR interpolated-string Cgen failed: $fixture"
    }
    & $xl cgen $source --backend xlir --emit $repeatCPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR interpolated-string repeat Cgen failed: $fixture"
    }
    $left = [System.IO.File]::ReadAllBytes($cPath)
    $right = [System.IO.File]::ReadAllBytes($repeatCPath)
    if ( -not [System.Linq.Enumerable]::SequenceEqual(
            [byte[]]$left, [byte[]]$right) ) {
        throw "XLIR interpolated-string Cgen is not byte-for-byte deterministic: $fixture"
    }
    $generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
    foreach ( $forbidden in @("xl6_xrt_fstr_join", "xl6CgenFString", "XL6_AST_INTERP_STRING")) {
        if ( $generated.Contains($forbidden) ) {
            throw "XLIR Cgen reconstructed source-level interpolation: $forbidden"
        }
    }

    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR interpolated-string strict generated-C runtime failed: $fixture"
    }
}

Write-Output "XLIR interpolated-string gate ok: first-class typed node and child arena, exact text/expression conversion ABI, left-to-right concat ownership transactions, generator and collection projections, raw/escaped/multiline runtime semantics, byte-for-byte deterministic mechanical C, and -Werror."
exit 0
