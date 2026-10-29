param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

function Read-Utf8([string]$path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Assert-Deterministic([string]$first, [string]$second)
{
    if ( ! [System.Linq.Enumerable]::SequenceEqual(
            [byte[]][System.IO.File]::ReadAllBytes($first),
            [byte[]][System.IO.File]::ReadAllBytes($second)
        ) ) {
        throw "XLIR string iterator output is not deterministic: $first"
    }
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_string_iterator_abi.xl"
$historicalSource = Join-Path $Root "test\cases\stdlib_string.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_string_iterator_abi.typed.json"
$typedRepeatPath = Join-Path $outputDir "xlir_string_iterator_abi.typed.repeat.json"
$loweredPath = Join-Path $outputDir "xlir_string_iterator_abi.lowered.json"
$loweredRepeatPath = Join-Path $outputDir "xlir_string_iterator_abi.lowered.repeat.json"
$cPath = Join-Path $outputDir "xlir_string_iterator_abi.c"
$cRepeatPath = Join-Path $outputDir "xlir_string_iterator_abi.repeat.c"
$historicalTypedPath = Join-Path $outputDir "stdlib_string.string_iterator.typed.json"
$historicalCPath = Join-Path $outputDir "stdlib_string.string_iterator.xlir.c"
$historicalCRepeatPath = Join-Path $outputDir "stdlib_string.string_iterator.xlir.repeat.c"
$xlirExe = Join-Path $outputDir "xlir_string_iterator_abi.xlir.exe"
$historicalObject = Join-Path $outputDir "stdlib_string.string_iterator.xlir.o"

foreach ( $path in @(
    $xl, $source, $historicalSource, $hostSource
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR string iterator gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string iterator typed JSON failed" }
& $xl ir $source --typed --format json --emit $typedRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "Repeated XLIR string iterator typed JSON failed" }
& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string iterator lowering failed" }
& $xl ir $source --lowered --format json --emit $loweredRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "Repeated XLIR string iterator lowering failed" }
& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR string iterator Cgen failed" }
& $xl cgen $source --backend xlir --emit $cRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "Repeated XLIR string iterator Cgen failed" }
& $xl ir $historicalSource --typed --format json --emit $historicalTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "Established stdlib string typed XLIR failed" }
& $xl cgen $historicalSource --backend xlir --emit $historicalCPath
if ( $LASTEXITCODE -ne 0 ) { throw "Established stdlib string XLIR Cgen failed" }
& $xl cgen $historicalSource --backend xlir --emit $historicalCRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "Repeated established stdlib string XLIR Cgen failed" }

Assert-Deterministic $typedPath $typedRepeatPath
Assert-Deterministic $loweredPath $loweredRepeatPath
Assert-Deterministic $cPath $cRepeatPath
Assert-Deterministic $historicalCPath $historicalCRepeatPath

$typed = Read-Utf8 $typedPath | ConvertFrom-Json
$lowered = Read-Utf8 $loweredPath | ConvertFrom-Json
$historicalTyped = Read-Utf8 $historicalTypedPath | ConvertFrom-Json
$instructions = @($lowered.instructions)
$compilerSymbolFloor = [uint32]::Parse(
    "F0000000", [Globalization.NumberStyles]::HexNumber)
$stringTypes = @($typed.types | Where-Object {
    $_.kind -eq 1 -and $_.name -eq "string" -and
    $_.cAbi -eq 25 -and $_.ownershipAbi -eq 2 -and
    $_.typeValueId -eq $typed.runtimeTypeMap.string
})
$integerTypes = @($typed.types | Where-Object {
    $_.kind -eq 1 -and $_.name -eq "int" -and
    $_.cAbi -eq 3 -and $_.typeValueId -eq $typed.runtimeTypeMap.i64
})
$charTypes = @($typed.types | Where-Object {
    $_.kind -eq 1 -and $_.name -eq "char" -and
    $_.cAbi -eq 43 -and $_.ownershipAbi -eq 1
})
if ( $stringTypes.Count -ne 1 -or $integerTypes.Count -ne 1 -or
     $charTypes.Count -ne 1 ) {
    throw "Typed XLIR string iterator has no unique canonical string/int/char TypeId"
}
$stringType = $stringTypes[0]
$integerType = $integerTypes[0]
$charType = $charTypes[0]

$loops = @($typed.nodes | Where-Object {
    $_.loopAbiName -like "string-utf8-*"
})
$valueLoops = @($loops | Where-Object {
    $_.loopAbiName -eq "string-utf8-value" -and
    $_.loopKeyAbiName -eq "none" -and
    $_.loopKeyLifetimeAbiName -eq "none"
})
$indexedLoops = @($loops | Where-Object {
    $_.loopAbiName -eq "string-utf8-index-value" -and
    $_.loopKeyAbiName -eq "ordinal-i64" -and
    $_.loopKeyLifetimeAbiName -eq "trivial-replace"
})
if ( $loops.Count -ne 7 -or $valueLoops.Count -ne 6 -or
     $indexedLoops.Count -ne 1 -or
     @($loops | Where-Object {
        $_.loopItemAbiName -ne "char" -or
        $_.loopItemLifetimeAbiName -ne "trivial-replace" -or
        $_.loopErrorAbiName -ne "may-raise" -or
        $_.loopResourceAbiName -ne "string-snapshot-release" -or
        $_.loopResourceType -ne $stringType.id
     }).Count -ne 0 ) {
    throw "Typed XLIR string loop layout, projection, effect or resource ABI is not exact"
}

$loopIds = @($loops | ForEach-Object { $_.id })
$loopInstructions = @($instructions | Where-Object { $_.origin -in $loopIds })
foreach ( $entry in @{
    "string-utf8-count" = 7
    "string-copy" = 3
}.GetEnumerator() ) {
    if ( @($loopInstructions | Where-Object {
            $_.runtimeOpName -eq $entry.Key
         }).Count -ne $entry.Value ) {
        throw "Lowered XLIR string iterator operation count is not exact: $($entry.Key)"
    }
}
$scalarItems = @($instructions | Where-Object {
    $_.runtimeOpName -eq "string-scalar-at"
})
if ( $scalarItems.Count -ne 7 ) {
    throw "Lowered XLIR string iterator has no exact char scalar projection count"
}

$snapshotSymbols = @()
$itemSymbols = @()
$copiedSources = 0
$transferredSources = 0
$discardedItems = 0
foreach ( $loop in $loops ) {
    $perLoop = @($loopInstructions | Where-Object { $_.origin -eq $loop.id })
    $counts = @($perLoop | Where-Object {
        $_.runtimeOpName -eq "string-utf8-count"
    })
    $items = @($scalarItems | Where-Object {
        $item = $_
        @($instructions | Where-Object {
            $_.index -eq ([int]$item.index + 1) -and
            $_.kindName -eq "error-test" -and
            $_.origin -eq $loop.id -and
            @($_.operands).Count -eq 1 -and
            $_.operands[0] -eq $item.result
        }).Count -eq 1
    })
    if ( $counts.Count -ne 1 -or $items.Count -ne 1 -or
         $counts[0].type -ne $integerType.id -or
         $items[0].type -ne $charType.id -or
         @($counts[0].operands).Count -ne 1 -or
         @($items[0].operands).Count -ne 2 ) {
        throw "Lowered XLIR string loop has no unique count/checked-char projection: n$($loop.id)"
    }
    $snapshotLoads = @($instructions | Where-Object {
        $_.result -eq $counts[0].operands[0] -and
        $_.kindName -eq "load" -and $_.type -eq $stringType.id
    })
    $itemSnapshotLoads = @($instructions | Where-Object {
        $_.result -eq $items[0].operands[0] -and
        $_.kindName -eq "load" -and $_.type -eq $stringType.id
    })
    $positionLoads = @($instructions | Where-Object {
        $_.result -eq $items[0].operands[1] -and
        $_.kindName -eq "load" -and $_.type -eq $integerType.id
    })
    if ( $snapshotLoads.Count -ne 1 -or $itemSnapshotLoads.Count -ne 1 -or
         $positionLoads.Count -ne 1 -or
         $snapshotLoads[0].targetSymbol -ne $itemSnapshotLoads[0].targetSymbol -or
         [long]$snapshotLoads[0].targetSymbol -lt $compilerSymbolFloor ) {
        throw "String loop count/projection does not share one compiler-owned snapshot and ordinal: n$($loop.id)"
    }
    $snapshotSymbol = [long]$snapshotLoads[0].targetSymbol
    $snapshotSymbols += $snapshotSymbol
    $snapshotStores = @($perLoop | Where-Object {
        $_.kindName -eq "store" -and $_.type -eq $stringType.id -and
        [long]$_.targetSymbol -eq $snapshotSymbol
    })
    $snapshotReleases = @($perLoop | Where-Object {
        $_.kindName -eq "release" -and $_.type -eq $stringType.id -and
        [long]$_.targetSymbol -eq $snapshotSymbol
    })
    if ( $snapshotStores.Count -ne 1 -or $snapshotReleases.Count -lt 1 -or
         @($snapshotStores[0].operands).Count -ne 1 ) {
        throw "String loop snapshot acquisition/release is not explicit on every route: n$($loop.id)"
    }
    $acquisitions = @($instructions | Where-Object {
        $_.result -eq $snapshotStores[0].operands[0]
    })
    if ( $acquisitions.Count -ne 1 ) {
        throw "String loop snapshot has no unique value definition: n$($loop.id)"
    }
    if ( $acquisitions[0].runtimeOpName -eq "string-copy" ) {
        ++$copiedSources
    } else {
        ++$transferredSources
    }

    $itemStores = @($perLoop | Where-Object {
        $_.kindName -eq "store" -and $_.type -eq $charType.id -and
        @($_.operands).Count -eq 1 -and $_.operands[0] -eq $items[0].result -and
        [long]$_.targetSymbol -lt $compilerSymbolFloor
    })
    $itemDiscards = @($perLoop | Where-Object {
        $_.kindName -eq "discard" -and $_.type -eq $charType.id -and
        @($_.operands).Count -eq 1 -and $_.operands[0] -eq $items[0].result
    })
    if ( $itemStores.Count + $itemDiscards.Count -ne 1 ) {
        throw "String loop scalar result has no unique char target/discard contract: n$($loop.id)"
    }
    if ( $itemStores.Count -eq 1 ) {
        $itemSymbol = [long]$itemStores[0].targetSymbol
        $itemSymbols += $itemSymbol
        if ( @($perLoop | Where-Object {
                $_.kindName -eq "release" -and
                [long]$_.targetSymbol -eq $itemSymbol
             }).Count -ne 0 ) {
            throw "String loop char target incorrectly acquired an owned release edge: n$($loop.id)"
        }
    } else {
        ++$discardedItems
    }
}
if ( @($snapshotSymbols | Select-Object -Unique).Count -ne $loops.Count -or
     @($itemSymbols | Select-Object -Unique).Count -ne ($loops.Count - 1) -or
     $discardedItems -ne 1 -or
     $copiedSources -ne 3 -or $transferredSources -ne 4 ) {
    throw "String loops lost unique snapshot/item storage or copy-vs-transfer acquisition semantics"
}

$historicalLoops = @($historicalTyped.nodes | Where-Object {
    $_.loopAbiName -like "string-utf8-*"
})
if ( $historicalLoops.Count -ne 2 -or
     @($historicalLoops | Where-Object {
         $_.loopResourceAbiName -ne "string-snapshot-release" -or
         $_.loopItemAbiName -ne "char" -or
         $_.loopItemLifetimeAbiName -ne "trivial-replace" -or
         $_.loopErrorAbiName -ne "may-raise"
     }).Count -ne 0 ) {
    throw "Established stdlib string loops did not migrate to canonical UTF-8 iterator ABI"
}

$generated = Read-Utf8 $cPath
if ( ([regex]::Matches($generated,
        "=\s*\(int64_t\)xrtUtf8Count\(v\d+\);")).Count -ne 7 -or
     ([regex]::Matches($generated,
        "bool\s+projection_ok_\d+\s*=\s*xlir_string_scalar_at\(v\d+,\s*v\d+,\s*&v\d+\);")).Count -ne 7 -or
     ([regex]::Matches($generated,
        "if \(projection_ok_\d+\) goto b\d+; else goto b\d+;")).Count -ne 7 -or
     ! $generated.Contains(
        "static bool xlir_string_scalar_at(xlir_string text, int64_t index, uint32_t* scalar)") -or
     $generated.Contains("xrtUtf8Substr") ) {
    throw "Generated C does not mechanically render every checked UTF-8 char projection"
}
foreach ( $forbidden in @(
    "loopAbi", "loopKeyAbi", "loopItemAbi", "sourceKind",
    "XL6_HIR", "xl6Hir", "utf8_char_size"
) ) {
    if ( $generated.Contains($forbidden) ) {
        throw "Generated string iterator C leaked semantic metadata or byte-scanning fallback: $forbidden"
    }
}
foreach ( $symbol in $snapshotSymbols ) {
    $loweredReleaseCount = @($loopInstructions | Where-Object {
        $_.kindName -eq "release" -and [long]$_.targetSymbol -eq $symbol
    }).Count
    $renderedReleaseCount = ([regex]::Matches(
        $generated,
        "\{\s*&\(xlir_released\),\s*&\(s$symbol\),\s*sizeof\(xlir_released\)\s*\}"
    )).Count
    if ( $renderedReleaseCount -ne $loweredReleaseCount ) {
        throw "Generated C does not preserve every string loop cleanup edge: s$symbol"
    }
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$xlirExe`" && `"$xlirExe`" && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$historicalCPath`" -c `"$hostSource`" -o `"$historicalObject`""
Push-Location $Root
try {
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "String iterator strict compile/runtime or stdlib compile failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR string iterator ABI gate ok: canonical UTF-8 scalar ordinals, trivial char projection with checked status edges, named-source snapshot copy, owned-rvalue transfer, empty/continue/break/return/finally/throw cleanup, emoji runtime semantics, deterministic mechanical C, strict runtime, and established stdlib string migration."
exit 0
