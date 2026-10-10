param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'xlir_iterator_gate_contract.ps1')

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_erased_collection_iterator_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_erased_collection_iterator_abi.typed.json"
$typedRepeatPath = Join-Path $outputDir "xlir_erased_collection_iterator_abi.typed.repeat.json"
$loweredPath = Join-Path $outputDir "xlir_erased_collection_iterator_abi.lowered.json"
$loweredRepeatPath = Join-Path $outputDir "xlir_erased_collection_iterator_abi.lowered.repeat.json"
$cPath = Join-Path $outputDir "xlir_erased_collection_iterator_abi.c"
$cRepeatPath = Join-Path $outputDir "xlir_erased_collection_iterator_abi.repeat.c"
$xlirExe = Join-Path $outputDir "xlir_erased_collection_iterator_abi.xlir.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR erased collection iterator gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR erased collection iterator typed JSON failed" }
& $xl ir $source --typed --format json --emit $typedRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR erased collection iterator repeated typed JSON failed" }
& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR erased collection iterator lowering failed" }
& $xl ir $source --lowered --format json --emit $loweredRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR erased collection iterator repeated lowering failed" }
& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR erased collection iterator Cgen failed" }
& $xl cgen $source --backend xlir --emit $cRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR erased collection iterator repeated Cgen failed" }

foreach ( $pair in @(
    @($typedPath, $typedRepeatPath),
    @($loweredPath, $loweredRepeatPath),
    @($cPath, $cRepeatPath)
) ) {
    if ( ![System.Linq.Enumerable]::SequenceEqual(
            [byte[]][System.IO.File]::ReadAllBytes($pair[0]),
            [byte[]][System.IO.File]::ReadAllBytes($pair[1])) ) {
        throw "XLIR erased collection iterator output is not deterministic: $($pair[0])"
    }
}

$typed = [System.IO.File]::ReadAllText(
    $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$shapeRuntimeIds = @{
    array = $typed.runtimeTypeMap.array
    list = $typed.runtimeTypeMap.intMap
    set = $typed.runtimeTypeMap.set
    dict = $typed.runtimeTypeMap.object
}
$collections = @($typed.types | Where-Object {
    $_.kind -eq 1 -and $_.collectionAbiName -in @("array", "list", "set", "dict")
})
if ( $collections.Count -ne 4 ) {
    throw "Typed XLIR does not expose exactly four statically shaped erased collection types"
}
foreach ( $shape in @("array", "list", "set", "dict") ) {
    $matches = @($collections | Where-Object { $_.collectionAbiName -eq $shape })
    if ( $matches.Count -ne 1 -or $matches[0].name -ne $shape -or
         $matches[0].cAbi -ne 26 -or $matches[0].ownershipAbi -ne 2 -or
         $matches[0].nullAbi -ne 2 -or @($matches[0].params).Count -ne 0 -or
         $matches[0].typeValueId -ne $shapeRuntimeIds[$shape] ) {
        throw "Typed XLIR erased $shape type has no exact shape, ownership or runtime identity ABI"
    }
}
$dynamicTypes = @($typed.types | Where-Object {
    $_.kind -eq 1 -and $_.collectionAbiName -eq "none" -and
    $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and
    @($_.params).Count -eq 0 -and
    $_.typeValueId -eq $typed.runtimeTypeMap.fallback
})
if ( $dynamicTypes.Count -lt 1 ) {
    throw "Typed XLIR erased collection elements have no canonical dynamic carrier TypeValueId"
}

$cursorTypes = @($typed.types | Where-Object {
    $_.kind -eq 14 -and $_.collectionAbiName -eq "none" -and
    $_.cAbi -eq 37 -and $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 1 -and
    @($_.params).Count -eq 1
})
if ( $cursorTypes.Count -ne 4 ) {
    throw "Typed XLIR erased collections do not own one cursor TypeId per source shape"
}
foreach ( $collection in $collections ) {
    if ( @($cursorTypes | Where-Object { $_.params[0] -eq $collection.id }).Count -ne 1 ) {
        throw "Typed XLIR erased $($collection.collectionAbiName) has no source-bound cursor TypeId"
    }
}

$loops = @($typed.nodes | Where-Object {
    $_.loopAbiName -like "xvalue-iterator-*"
})
if ( $loops.Count -ne 10 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-value" -and
        $_.loopKeyAbiName -eq "none" -and
        $_.loopItemAbiName -eq "xvalue-retain"
     }).Count -ne 4 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-key-value" -and
        $_.loopKeyAbiName -eq "ordinal-i64" -and
        $_.loopItemAbiName -eq "xvalue-retain"
     }).Count -ne 1 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-key-value" -and
        $_.loopKeyAbiName -eq "integer-i64" -and
        $_.loopItemAbiName -eq "xvalue-retain"
     }).Count -ne 2 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-key-value" -and
        $_.loopKeyAbiName -eq "string-copy" -and
        $_.loopItemAbiName -eq "xvalue-retain"
     }).Count -ne 2 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-key" -and
        $_.loopKeyAbiName -eq "string-copy" -and
        $_.loopItemAbiName -eq "none"
     }).Count -ne 1 -or
     @($loops | Where-Object {
        $_.loopErrorAbiName -ne "may-raise" -or
        $_.loopResourceAbiName -ne "xvalue-iterator-destroy"
     }).Count -ne 0 ) {
    throw "Typed XLIR erased collection loop target/key/item/resource contract is not exact"
}
foreach ( $loop in $loops ) {
    $loopResourceType = $loop.loopResourceType
    if ( @($cursorTypes | Where-Object {
            $_.id -eq $loopResourceType
         }).Count -ne 1 ) {
        throw "Typed XLIR erased collection loop has no exact cursor resource TypeId: n$($loop.id)"
    }
}

$loopIds = @($loops | ForEach-Object { Get-XlirIteratorOrigins $typed $_ })
$loopInstructions = @($lowered.instructions | Where-Object {
    $_.origin -in $loopIds
})
if ( @($loopInstructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-create"
     }).Count -ne 10 -or
     @($loopInstructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-advance"
     }).Count -ne 10 -or
     @($loopInstructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-item-xvalue-retain"
     }).Count -ne 9 -or
     @($loopInstructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-key-ordinal-i64"
     }).Count -ne 1 -or
     @($loopInstructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-key-i64"
     }).Count -ne 2 -or
     @($loopInstructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-iterator-key-string-copy"
     }).Count -ne 3 ) {
    throw "Lowered XLIR erased collection cursor projections are not canonical"
}
$cursorSymbols = @()
foreach ( $loop in $loops ) {
    Assert-XlirCheckedIteratorAdvance $lowered $loop
    $origins = @(Get-XlirIteratorOrigins $typed $loop)
    $instructions = @($loopInstructions | Where-Object { $_.origin -in $origins })
    $cursorStores = @(Get-XlirIteratorCursorStore $lowered $loop)
    if ( $cursorStores.Count -ne 1 ) {
        throw "Lowered XLIR erased collection loop has no unique cursor storage: n$($loop.id)"
    }
    $cursorSymbol = [long]$cursorStores[0].targetSymbol
    $cursorSymbols += $cursorSymbol
    if ( @($instructions | Where-Object {
            $_.kindName -eq "release" -and
            $_.type -eq $loop.loopResourceType -and
            $_.targetSymbol -eq $cursorSymbol
         }).Count -lt 1 ) {
        throw "Lowered XLIR erased collection loop has an escaping path without cursor destroy: n$($loop.id)"
    }
}
if ( @($cursorSymbols | Select-Object -Unique).Count -ne 10 ) {
    throw "Lowered XLIR erased collection loops do not own unique cursor storage"
}
$itemProjectionTypes = @($loopInstructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-iterator-item-xvalue-retain"
} | ForEach-Object { $_.type } | Select-Object -Unique)
foreach ( $typeId in $itemProjectionTypes ) {
    $type = @($typed.types | Where-Object { $_.id -eq $typeId })
    if ( $type.Count -ne 1 -or $type[0].collectionAbiName -ne "none" -or
         $type[0].typeValueId -ne $typed.runtimeTypeMap.fallback ) {
        throw "Lowered XLIR erased item projection escaped the canonical dynamic carrier ABI"
    }
}

$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($generated,
        "=\s*xl6ListValueIteratorCreateV1\(v\d+\);")).Count -ne 2 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_create\(v\d+\);")).Count -ne 8 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_advance\(v\d+, &v\d+\);")).Count -ne 10 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_item_xvalue_retain\(v\d+\);")).Count -ne 9 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_key_ordinal_i64\(v\d+\);")).Count -ne 1 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_key_i64\(v\d+\);")).Count -ne 2 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_key_string_copy\(v\d+\);")).Count -ne 3 -or
     ([regex]::Matches($generated,
        "static xvalue\* xlir_xvalue_iterator_item_xvalue_retain\(")).Count -ne 1 -or
     ([regex]::Matches($generated, "\bfor\s*\(")).Count -ne 0 -or
     $generated.Contains("loopAbi") -or
     $generated.Contains("collectionAbiName") ) {
    throw "XLIR erased collection iterator C rendering is not mechanical from verified CFG"
}
foreach ( $cursorSymbol in $cursorSymbols ) {
    $loweredReleaseCount = @($lowered.instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $cursorSymbol
    }).Count
    $renderedReleaseCount = Get-XlirRenderedIteratorReleases $generated $cursorSymbol 'xlir_xvalue_iterator*' 'xlir_xvalue_iterator_destroy'
    if ( $renderedReleaseCount -ne $loweredReleaseCount ) {
        throw "Generated C does not preserve every erased collection cursor destroy edge: s$cursorSymbol"
    }
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$xlirExe`" && `"$xlirExe`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "Erased collection iterator strict compile/runtime failed"
}

Write-Output "XLIR erased collection iterator ABI gate ok: four statically shaped zero-parameter collection TypeIds, canonical dynamic item carrier, source-bound cursors, orthogonal key/item projections, explicit cleanup on continue/break/return/finally, deterministic mechanical C, and strict compile/runtime semantics."
exit 0
