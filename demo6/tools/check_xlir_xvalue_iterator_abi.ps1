param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'xlir_iterator_gate_contract.ps1')

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_list_i64_iterator_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedJsonPath = Join-Path $outputDir "xlir_list_i64_iterator_abi.typed.json"
$loweredJsonPath = Join-Path $outputDir "xlir_list_i64_iterator_abi.lowered.json"
$cPath = Join-Path $outputDir "xlir_list_i64_iterator_abi.c"
$exePath = Join-Path $outputDir "xlir_list_i64_iterator_abi_host.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR xvalue iterator ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue iterator typed JSON failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$loops = @($typed.nodes | Where-Object {
    $_.loopAbiName -like "xvalue-iterator-*"
})
if ( $loops.Count -ne 9 -or
     @($loops | Where-Object {
         $_.loopAbiName -eq "xvalue-iterator-value" -and
         $_.loopKeyAbiName -eq "none" -and
         $_.loopItemAbiName -eq "i64"
     }).Count -ne 5 -or
     @($loops | Where-Object {
         $_.loopAbiName -eq "xvalue-iterator-key-value" -and
         $_.loopKeyAbiName -eq "integer-i64" -and
         $_.loopItemAbiName -eq "i64"
     }).Count -ne 4 ) {
    throw "Typed XLIR xvalue iterator target/key/item contract is not exact"
}
$i64Types = @($typed.types | Where-Object { $_.cAbi -eq 3 })
$listTypes = @($typed.types | Where-Object {
    $_.kind -eq 6 -and $_.collectionAbiName -eq "list" -and
    $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and
    @($_.params).Count -eq 1
})
$iteratorTypes = @($typed.types | Where-Object {
    $_.kind -eq 14 -and $_.collectionAbiName -eq "none" -and
    $_.cAbi -eq 37 -and $_.ownershipAbi -eq 2 -and
    $_.nullAbi -eq 1 -and @($_.params).Count -eq 1
})
if ( $i64Types.Count -ne 1 -or $listTypes.Count -ne 1 -or
     $listTypes[0].params[0] -ne $i64Types[0].id -or
     $iteratorTypes.Count -ne 1 -or
     $iteratorTypes[0].params[0] -ne $listTypes[0].id -or
     @($loops | Where-Object {
        $_.loopErrorAbiName -ne "may-raise" -or
        $_.loopResourceAbiName -ne "xvalue-iterator-destroy" -or
        $_.loopResourceType -ne $iteratorTypes[0].id
     }).Count -ne 0 ) {
    throw "Typed XLIR list<i64> loop has no exact compiler-owned cursor type"
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue iterator lowering failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$loopIds = @($loops | ForEach-Object { Get-XlirIteratorOrigins $typed $_ })
$loopInstructions = @($lowered.instructions | Where-Object {
    $_.origin -in $loopIds
})
$releaseTotal = 0
$cursorSymbols = @()
foreach ( $loop in $loops ) {
    Assert-XlirCheckedIteratorAdvance $lowered $loop
    $origins = @(Get-XlirIteratorOrigins $typed $loop)
    $instructions = @($loopInstructions | Where-Object {
        $_.origin -in $origins
    })
    $cursorStores = @(Get-XlirIteratorCursorStore $lowered $loop)
    $expectedKeyCount = if ( $loop.loopAbiName -eq "xvalue-iterator-key-value" ) { 1 } else { 0 }
    if ( @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-create"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-advance"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-item-i64"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-key-i64"
         }).Count -ne $expectedKeyCount -or
         $cursorStores.Count -ne 1 ) {
        throw "Lowered XLIR xvalue iterator operations are not canonical for n$($loop.id)"
    }
    $cursorSymbol = [long]$cursorStores[0].targetSymbol
    $cursorSymbols += $cursorSymbol
    $releaseCount = @($instructions | Where-Object {
        $_.kindName -eq "release" -and
        $_.type -eq $iteratorTypes[0].id -and
        $_.targetSymbol -eq $cursorSymbol
    }).Count
    if ( $releaseCount -lt 1 ) {
        throw "Lowered XLIR xvalue iterator has an escaping path without destroy: n$($loop.id)"
    }
    $releaseTotal += $releaseCount
}
if ( $releaseTotal -ne 27 -or @($cursorSymbols | Select-Object -Unique).Count -ne 9 ) {
    throw "Lowered XLIR xvalue iterator resource exit distribution changed"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue iterator Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_create\(v\d+\);")).Count -ne 9 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_advance\(v\d+, &v\d+\);")).Count -ne 9 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_item_i64\(v\d+\);")).Count -ne 9 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_key_i64\(v\d+\);")).Count -ne 4 -or
     ([regex]::Matches($generated,
        "#define xlir_xvalue_iterator_create \(xl6ValueIteratorRuntimeV2Get\(\)->Create\)")).Count -ne 1 -or
     ([regex]::Matches($generated, "\bfor\s*\(")).Count -ne 0 -or
     $generated.Contains("loopAbi") ) {
    throw "XLIR xvalue iterator C rendering is not mechanical from verified CFG"
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

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR xvalue iterator strict generated-C runtime failed"
}

Write-Output "XLIR xvalue iterator ABI gate ok: dedicated cursor TypeId, orthogonal key/item projection, stable named and owned-rvalue list<i64> snapshots, explicit advance/item/key operations, LoopResource destroy on return/throw/finally, mechanical C, strict compile, and runtime semantics."
exit 0
