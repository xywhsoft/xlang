param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_dict_iterator_projection.xl"
$keyOnlySource = Join-Path $Root "test\cases\xlir_dict_key_only_iterator.xl"
$hostSourcePath = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
foreach ( $path in @($xl, $source, $keyOnlySource, $hostSourcePath) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR dict iterator projection gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-XlirDictFixture {
    param([string]$SourcePath, [string]$Name)
    $typedPath = Join-Path $outputDir "$Name.typed.json"
    $loweredPath = Join-Path $outputDir "$Name.lowered.json"
    $generatedPath = Join-Path $outputDir "$Name.c"
    & $xl ir $SourcePath --typed --format json --emit $typedPath
    if ( $LASTEXITCODE -ne 0 ) { throw "$Name typed JSON failed" }
    & $xl ir $SourcePath --lowered --format json --emit $loweredPath
    if ( $LASTEXITCODE -ne 0 ) { throw "$Name lowering failed" }
    & $xl cgen $SourcePath --backend xlir --emit $generatedPath
    if ( $LASTEXITCODE -ne 0 ) { throw "$Name Cgen failed" }
    return @{
        Typed = ([System.IO.File]::ReadAllText(
            $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json)
        Lowered = ([System.IO.File]::ReadAllText(
            $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json)
        GeneratedPath = $generatedPath
        Generated = [System.IO.File]::ReadAllText(
            $generatedPath, [System.Text.Encoding]::UTF8)
    }
}

$main = Invoke-XlirDictFixture $source "xlir_dict_iterator_projection"
$loops = @($main.Typed.nodes | Where-Object {
    $_.loopAbiName -like "xvalue-iterator-*"
})
if ( $loops.Count -ne 12 -or @($loops | Where-Object {
        $_.loopAbiName -ne "xvalue-iterator-key-value" -or
        $_.loopKeyAbiName -ne "string-copy" -or
        $_.loopKeyLifetimeAbiName -ne "owned-iteration-release" -or
        $_.loopErrorAbiName -ne "no-raise" -or
        $_.loopResourceAbiName -ne "xvalue-iterator-destroy"
    }).Count -ne 0 ) {
    throw "Typed XLIR dict key+item layout/lifetime ABI is not exact"
}
foreach ( $projection in @(
    @{ Name = "i64"; Count = 6 },
    @{ Name = "u64"; Count = 1 },
    @{ Name = "bool"; Count = 1 },
    @{ Name = "f64"; Count = 1 },
    @{ Name = "string-copy"; Count = 3 }
) ) {
    if ( @($loops | Where-Object {
            $_.loopItemAbiName -eq $projection.Name
         }).Count -ne $projection.Count ) {
        throw "Typed XLIR dict item projection count changed: $($projection.Name)"
    }
}
if ( @($loops | Where-Object {
        $_.loopItemAbiName -eq "string-copy" -and
        $_.loopItemLifetimeAbiName -eq "owned-iteration-release"
    }).Count -ne 3 -or
     @($loops | Where-Object {
        $_.loopItemAbiName -ne "string-copy" -and
        $_.loopItemLifetimeAbiName -eq "trivial-replace"
    }).Count -ne 9 ) {
    throw "Typed XLIR dict item representation and lifetime are conflated"
}

$stringTypes = @($main.Typed.types | Where-Object {
    $_.cAbi -eq 25 -and $_.ownershipAbi -eq 2
})
$dictTypes = @($main.Typed.types | Where-Object {
    $_.kind -eq 6 -and $_.collectionAbiName -eq "dict" -and
    $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and
    @($_.params).Count -eq 1
})
$iteratorTypes = @($main.Typed.types | Where-Object {
    $_.kind -eq 14 -and $_.cAbi -eq 37 -and $_.ownershipAbi -eq 2 -and
    $_.nullAbi -eq 1 -and @($_.params).Count -eq 1
})
if ( $stringTypes.Count -ne 1 -or $dictTypes.Count -ne 5 -or
     $iteratorTypes.Count -ne 5 ) {
    throw "Typed XLIR dict/cursor TypeId matrix is incomplete"
}
foreach ( $loop in $loops ) {
    $cursorType = @($iteratorTypes | Where-Object {
        $_.id -eq $loop.loopResourceType
    })
    if ( $cursorType.Count -ne 1 -or @($dictTypes | Where-Object {
            $_.id -eq $cursorType[0].params[0]
        }).Count -ne 1 ) {
        throw "Typed XLIR dict loop has no source-bound cursor TypeId: n$($loop.id)"
    }
}

$loopIds = @($loops | ForEach-Object { $_.id })
$loopInstructions = @($main.Lowered.instructions | Where-Object {
    $_.origin -in $loopIds
})
$keySymbols = @()
$managedItemSymbols = @()
$cursorSymbols = @()
$keyReleaseTotal = 0
$itemReleaseTotal = 0
$cursorReleaseTotal = 0
foreach ( $loop in $loops ) {
    $instructions = @($loopInstructions | Where-Object {
        $_.origin -eq $loop.id
    })
    $stringStores = @($instructions | Where-Object {
        $_.kindName -eq "store" -and $_.type -eq $stringTypes[0].id
    })
    $expectedStringStores = 1
    if ( $loop.loopItemAbiName -eq "string-copy" ) {
        $expectedStringStores = 2
    }
    $cursorStores = @($instructions | Where-Object {
        $_.kindName -eq "store" -and $_.type -eq $loop.loopResourceType -and
        $_.targetSymbol -ge 0xF0000000
    })
    if ( @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-create"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-advance"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-key-string-copy"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-item-$($loop.loopItemAbiName)"
         }).Count -ne 1 -or
         $stringStores.Count -ne $expectedStringStores -or
         $cursorStores.Count -ne 1 ) {
        throw "Lowered XLIR dict projection operations are not canonical for n$($loop.id)"
    }
    $keySymbol = [long]$stringStores[0].targetSymbol
    $cursorSymbol = [long]$cursorStores[0].targetSymbol
    $keySymbols += $keySymbol
    $cursorSymbols += $cursorSymbol
    $keyReleaseCount = @($instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $keySymbol
    }).Count
    $cursorReleaseCount = @($instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $cursorSymbol
    }).Count
    if ( $keyReleaseCount -lt 1 -or $cursorReleaseCount -lt 1 ) {
        throw "Lowered XLIR dict loop has an escaping path without key/cursor cleanup: n$($loop.id)"
    }
    if ( $loop.loopItemAbiName -eq "string-copy" ) {
        $itemSymbol = [long]$stringStores[1].targetSymbol
        $managedItemSymbols += $itemSymbol
        $itemReleaseCount = @($instructions | Where-Object {
            $_.kindName -eq "release" -and $_.targetSymbol -eq $itemSymbol
        }).Count
        if ( $itemReleaseCount -lt 1 ) {
            throw "Lowered XLIR dict<string> loop has no owned item cleanup: n$($loop.id)"
        }
        $itemReleaseTotal += $itemReleaseCount
    }
    $keyReleaseTotal += $keyReleaseCount
    $cursorReleaseTotal += $cursorReleaseCount
}
if ( @($keySymbols | Select-Object -Unique).Count -ne 12 -or
     @($managedItemSymbols | Select-Object -Unique).Count -ne 3 -or
     @($cursorSymbols | Select-Object -Unique).Count -ne 12 -or
     # Returning a key first creates the independent string-copy below; the
     # iteration key therefore has one owner release, not a duplicated return
     # cleanup.  Cursor cleanup remains split only where CFG exits require it.
     $keyReleaseTotal -ne 17 -or $itemReleaseTotal -ne 6 -or
     $cursorReleaseTotal -ne 16 -or
     @($main.Lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "string-copy" -and
        $_.origin -in @($main.Typed.nodes | Where-Object {
            $_.kind -eq 12
        } | ForEach-Object { $_.id })
     }).Count -ne 2 ) {
    throw "Lowered XLIR dict resource/return ownership distribution changed"
}

$keyOnly = Invoke-XlirDictFixture $keyOnlySource "xlir_dict_key_only_iterator"
$keyLoops = @($keyOnly.Typed.nodes | Where-Object {
    $_.loopAbiName -like "xvalue-iterator-*"
})
if ( $keyLoops.Count -ne 2 -or @($keyLoops | Where-Object {
        $_.loopAbiName -ne "xvalue-iterator-key" -or
        $_.loopKeyAbiName -ne "string-copy" -or
        $_.loopItemAbiName -ne "none" -or
        $_.loopKeyLifetimeAbiName -ne "owned-iteration-release" -or
        $_.loopItemLifetimeAbiName -ne "none"
    }).Count -ne 0 ) {
    throw "Typed XLIR dict key-only loop is not semantically distinct from item iteration"
}
$keyLoopIds = @($keyLoops | ForEach-Object { $_.id })
$keyLoopInstructions = @($keyOnly.Lowered.instructions | Where-Object {
    $_.origin -in $keyLoopIds
})
if ( @($keyLoopInstructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-key-string-copy"
     }).Count -ne 2 -or
     @($keyLoopInstructions | Where-Object {
        $_.runtimeOpName -like "xvalue-iterator-item-*"
     }).Count -ne 0 -or
     @($keyOnly.Lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "string-copy"
     }).Count -ne 1 ) {
    throw "Lowered XLIR dict key-only projection or return ownership is ambiguous"
}

foreach ( $generatedCase in @($main, $keyOnly) ) {
    if ( ([regex]::Matches($generatedCase.Generated,
            "static char\* xlir_xvalue_iterator_key_string_copy\(")).Count -ne 1 -or
         ([regex]::Matches($generatedCase.Generated, "\bfor\s*\(")).Count -ne 0 -or
         $generatedCase.Generated.Contains("loopKeyAbi") -or
         $generatedCase.Generated.Contains("loopKeyLifetimeAbi") ) {
        throw "XLIR dict C rendering is not mechanical from verified CFG"
    }
}
if ( ([regex]::Matches($main.Generated,
        "=\s*xlir_xvalue_iterator_key_string_copy\(v\d+\);")).Count -ne 12 -or
     ([regex]::Matches($keyOnly.Generated,
        "=\s*xlir_xvalue_iterator_key_string_copy\(v\d+\);")).Count -ne 2 -or
     ([regex]::Matches($keyOnly.Generated,
        "=\s*xlir_xvalue_iterator_item_[a-z0-9_]+\(v\d+\);")).Count -ne 0 ) {
    throw "Generated C does not preserve dict key-only/key+item projection counts"
}
foreach ( $keySymbol in $keySymbols ) {
    $loweredReleaseCount = @($loopInstructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $keySymbol
    }).Count
    $renderedReleaseCount = ([regex]::Matches(
        $main.Generated, "xrtFree\(s$keySymbol\);\s*s$keySymbol = NULL;"
    )).Count
    if ( $renderedReleaseCount -ne $loweredReleaseCount ) {
        throw "Generated C does not preserve every dict key release: s$keySymbol"
    }
}
foreach ( $itemSymbol in $managedItemSymbols ) {
    $loweredReleaseCount = @($loopInstructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $itemSymbol
    }).Count
    $renderedReleaseCount = ([regex]::Matches(
        $main.Generated, "xrtFree\(s$itemSymbol\);\s*s$itemSymbol = NULL;"
    )).Count
    if ( $renderedReleaseCount -ne $loweredReleaseCount ) {
        throw "Generated C does not preserve every dict managed item release: s$itemSymbol"
    }
}

foreach ( $run in @(
    @{ Name = "xlir_dict_iterator_projection"; Generated = $main.GeneratedPath },
    @{ Name = "xlir_dict_key_only_iterator"; Generated = $keyOnly.GeneratedPath }
) ) {
    $exePath = Join-Path $outputDir "$($run.Name)_host.exe"
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$($run.Generated)`" `"$hostSourcePath`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "$($run.Name) strict generated-C runtime failed"
    }
}

Write-Output "XLIR dict iterator projection gate ok: explicit key-only versus key+item LoopAbi, string-key copy/lifetime, i64/u64/bool/f64/string item projections, double-managed reverse cleanup on all exits/finally, independent return ownership, mechanical C, strict compile and runtime semantics."
exit 0
