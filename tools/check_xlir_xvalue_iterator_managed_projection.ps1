param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'xlir_iterator_gate_contract.ps1')

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_xvalue_iterator_managed_projection.xl"
$hostSourcePath = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedJsonPath = Join-Path $outputDir "xlir_xvalue_iterator_managed_projection.typed.json"
$loweredJsonPath = Join-Path $outputDir "xlir_xvalue_iterator_managed_projection.lowered.json"
$cPath = Join-Path $outputDir "xlir_xvalue_iterator_managed_projection.c"
$exePath = Join-Path $outputDir "xlir_xvalue_iterator_managed_projection_host.exe"

foreach ( $path in @($xl, $source, $hostSourcePath) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR managed iterator projection gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR managed iterator projection typed JSON failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$loops = @($typed.nodes | Where-Object {
    $_.loopAbiName -like "xvalue-iterator-*"
})
if ( $loops.Count -ne 11 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-value" -and
        $_.loopKeyAbiName -eq "none" -and
        $_.loopKeyLifetimeAbiName -eq "none"
     }).Count -ne 9 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-key-value" -and
        $_.loopKeyAbiName -eq "integer-i64" -and
        $_.loopKeyLifetimeAbiName -eq "trivial-replace"
     }).Count -ne 2 -or
     @($loops | Where-Object {
        $_.loopItemAbiName -ne "string-copy" -or
        $_.loopItemLifetimeAbiName -ne "owned-iteration-release" -or
        $_.loopErrorAbiName -ne "may-raise" -or
        $_.loopResourceAbiName -ne "xvalue-iterator-destroy"
     }).Count -ne 0 ) {
    throw "Typed XLIR managed iterator projection/lifetime contract is not exact"
}

$stringTypes = @($typed.types | Where-Object {
    $_.cAbi -eq 25 -and $_.ownershipAbi -eq 2
})
$collectionTypes = @($typed.types | Where-Object {
    $_.kind -eq 6 -and $_.collectionAbiName -in @("list", "set") -and
    $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and
    @($_.params).Count -eq 1
})
$iteratorTypes = @($typed.types | Where-Object {
    $_.kind -eq 14 -and $_.cAbi -eq 37 -and $_.ownershipAbi -eq 2 -and
    $_.nullAbi -eq 1 -and @($_.params).Count -eq 1
})
if ( $stringTypes.Count -ne 1 -or $collectionTypes.Count -ne 2 -or
     $iteratorTypes.Count -ne 2 -or
     @($collectionTypes | Where-Object {
        $_.params[0] -ne $stringTypes[0].id
     }).Count -ne 0 ) {
    throw "Typed XLIR managed iterator TypeId matrix is incomplete"
}
foreach ( $loop in $loops ) {
    $cursorType = @($iteratorTypes | Where-Object {
        $_.id -eq $loop.loopResourceType
    })
    if ( $cursorType.Count -ne 1 -or
         @($collectionTypes | Where-Object {
            $_.id -eq $cursorType[0].params[0]
         }).Count -ne 1 ) {
        throw "Typed XLIR managed loop has no source-bound cursor TypeId: n$($loop.id)"
    }
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR managed iterator projection lowering failed"
}
$lowered = [System.IO.File]::ReadAllText(
    $loweredJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$loopIds = @($loops | ForEach-Object { Get-XlirIteratorOrigins $typed $_ })
$loopInstructions = @($lowered.instructions | Where-Object {
    $_.origin -in $loopIds
})
$itemSymbols = @()
$cursorSymbols = @()
$itemReleaseTotal = 0
$cursorReleaseTotal = 0
foreach ( $loop in $loops ) {
    Assert-XlirCheckedIteratorAdvance $lowered $loop
    $origins = @(Get-XlirIteratorOrigins $typed $loop)
    $instructions = @($loopInstructions | Where-Object {
        $_.origin -in $origins
    })
    $itemStores = @(Get-XlirIteratorTargetStores $typed $lowered $loop $stringTypes[0].id)
    $cursorStores = @(Get-XlirIteratorCursorStore $lowered $loop)
    $expectedKeyCount = if ( $loop.loopAbiName -eq "xvalue-iterator-key-value" ) { 1 } else { 0 }
    if ( @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-create"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-advance"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-item-string-copy"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-key-i64"
         }).Count -ne $expectedKeyCount -or
         $itemStores.Count -ne 1 -or $cursorStores.Count -ne 1 ) {
        throw "Lowered XLIR managed iterator operations are not canonical for n$($loop.id)"
    }
    $itemSymbol = [long]$itemStores[0].targetSymbol
    $cursorSymbol = [long]$cursorStores[0].targetSymbol
    $itemSymbols += $itemSymbol
    $cursorSymbols += $cursorSymbol
    $itemReleaseCount = @($instructions | Where-Object {
        $_.kindName -eq "release" -and $_.type -eq $stringTypes[0].id -and
        $_.targetSymbol -eq $itemSymbol
    }).Count
    $cursorReleaseCount = @($instructions | Where-Object {
        $_.kindName -eq "release" -and $_.type -eq $loop.loopResourceType -and
        $_.targetSymbol -eq $cursorSymbol
    }).Count
    if ( $itemReleaseCount -lt 1 -or $cursorReleaseCount -lt 1 ) {
        throw "Lowered XLIR managed iterator has an escaping path without ordered item/cursor cleanup: n$($loop.id)"
    }
    $itemReleaseTotal += $itemReleaseCount
    $cursorReleaseTotal += $cursorReleaseCount
}
if ( @($itemSymbols | Select-Object -Unique).Count -ne 11 -or
     @($cursorSymbols | Select-Object -Unique).Count -ne 11 -or
     $itemReleaseTotal -ne 37 -or $cursorReleaseTotal -ne 58 ) {
    throw "Lowered XLIR managed iterator resource exit distribution changed"
}
$returnNodeIds = @($typed.nodes | Where-Object {
    $_.kind -eq 12
} | ForEach-Object { $_.id })
if ( @($lowered.instructions | Where-Object {
        $_.runtimeOpName -eq "string-copy" -and $_.origin -in $returnNodeIds
     }).Count -ne 2 ) {
    throw "Returning an active managed iteration target did not create independent result ownership"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR managed iterator projection Cgen failed"
}
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($generated,
        "=\s*xl6ListValueIteratorCreateV1\(v\d+\);")).Count -ne 10 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_create\(v\d+\);")).Count -ne 1 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_advance\(v\d+, &v\d+\);")).Count -ne 11 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_item_string_copy\(v\d+\);")).Count -ne 11 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_key_i64\(v\d+\);")).Count -ne 2 -or
     ([regex]::Matches($generated,
        "static xlir_string xlir_xvalue_iterator_item_string_copy\(")).Count -ne 1 -or
     ([regex]::Matches($generated, "\bfor\s*\(")).Count -ne 0 -or
     $generated.Contains("loopItemAbi") -or
     $generated.Contains("loopItemLifetimeAbi") ) {
    throw "XLIR managed iterator C rendering is not mechanical from verified CFG"
}
foreach ( $itemSymbol in $itemSymbols ) {
    $loweredReleaseCount = @($lowered.instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $itemSymbol
    }).Count
    $renderedReleaseCount = Get-XlirRenderedIteratorReleases $generated $itemSymbol 'xlir_string' 'xlir_string_drop'
    if ( $renderedReleaseCount -ne $loweredReleaseCount ) {
        throw "Generated C does not preserve every owned iteration-target release: s$itemSymbol"
    }
}
foreach ( $cursorSymbol in $cursorSymbols ) {
    $loweredReleaseCount = @($lowered.instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $cursorSymbol
    }).Count
    $renderedReleaseCount = Get-XlirRenderedIteratorReleases $generated $cursorSymbol 'xlir_xvalue_iterator*' 'xlir_xvalue_iterator_destroy'
    if ( $renderedReleaseCount -ne $loweredReleaseCount ) {
        throw "Generated C does not preserve every iterator destroy edge: s$cursorSymbol"
    }
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSourcePath`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR managed iterator strict generated-C runtime failed"
}

Write-Output "XLIR managed iterator projection gate ok: list/set string-copy RuntimeOp, explicit target lifetime ABI, per-iteration release on fallthrough/continue/break/return/throw/finally, return ownership preserved through finally, cursor teardown ordering, mechanical C, strict compile and runtime semantics."
exit 0
