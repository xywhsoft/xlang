param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'xlir_iterator_gate_contract.ps1')

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_set_i64_iterator_abi.xl"
$rejectSource = Join-Path $Root "test\cases\cgen_xlir_set_i64_key_reject.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedJsonPath = Join-Path $outputDir "xlir_set_i64_iterator_abi.typed.json"
$loweredJsonPath = Join-Path $outputDir "xlir_set_i64_iterator_abi.lowered.json"
$rejectTypedJsonPath = Join-Path $outputDir "xlir_set_i64_key_reject.typed.json"
$rejectCPath = Join-Path $outputDir "xlir_set_i64_key_reject.c"
$cPath = Join-Path $outputDir "xlir_set_i64_iterator_abi.c"
$exePath = Join-Path $outputDir "xlir_set_i64_iterator_abi_host.exe"

foreach ( $path in @($xl, $source, $rejectSource, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR set<i64> iterator ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR set<i64> iterator typed JSON failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$loops = @($typed.nodes | Where-Object {
    $_.loopAbiName -like "xvalue-iterator-*"
})
if ( $loops.Count -ne 9 -or @($loops | Where-Object {
        $_.loopAbiName -ne "xvalue-iterator-value" -or
        $_.loopKeyAbiName -ne "none" -or
        $_.loopItemAbiName -ne "i64" -or
        $_.loopErrorAbiName -ne "may-raise"
    }).Count -ne 0 ) {
    throw "Typed XLIR set<i64> value-only projection contract is not exact"
}
$i64Types = @($typed.types | Where-Object { $_.cAbi -eq 3 })
$setTypes = @($typed.types | Where-Object {
    $_.kind -eq 6 -and $_.collectionAbiName -eq "set" -and
    $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and
    @($_.params).Count -eq 1
})
$iteratorTypes = @($typed.types | Where-Object {
    $_.kind -eq 14 -and $_.collectionAbiName -eq "none" -and
    $_.cAbi -eq 37 -and $_.ownershipAbi -eq 2 -and
    $_.nullAbi -eq 1 -and @($_.params).Count -eq 1
})
if ( $i64Types.Count -ne 1 -or $setTypes.Count -ne 1 -or
     $setTypes[0].params[0] -ne $i64Types[0].id -or
     $iteratorTypes.Count -ne 1 -or
     $iteratorTypes[0].params[0] -ne $setTypes[0].id -or
     @($loops | Where-Object {
        $_.loopResourceAbiName -ne "xvalue-iterator-destroy" -or
        $_.loopResourceType -ne $iteratorTypes[0].id
     }).Count -ne 0 ) {
    throw "Typed XLIR set<i64> loop has no exact compiler-owned cursor TypeId"
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR set<i64> iterator lowering failed" }
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
         }).Count -ne 0 -or $cursorStores.Count -ne 1 ) {
        throw "Lowered XLIR set<i64> iterator operations are not canonical for n$($loop.id)"
    }
    $cursorSymbol = [long]$cursorStores[0].targetSymbol
    $cursorSymbols += $cursorSymbol
    $releaseCount = @($instructions | Where-Object {
        $_.kindName -eq "release" -and
        $_.type -eq $iteratorTypes[0].id -and
        $_.targetSymbol -eq $cursorSymbol
    }).Count
    if ( $releaseCount -lt 1 ) {
        throw "Lowered XLIR set<i64> iterator has an escaping path without destroy: n$($loop.id)"
    }
    $releaseTotal += $releaseCount
}
if ( $releaseTotal -ne 27 -or @($cursorSymbols | Select-Object -Unique).Count -ne 9 ) {
    throw "Lowered XLIR set<i64> iterator resource exit distribution changed"
}

& $xl ir $rejectSource --typed --format json --emit $rejectTypedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR set<i64> key rejection typed JSON failed" }
$rejectedTyped = [System.IO.File]::ReadAllText(
    $rejectTypedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$rejectedLoops = @($rejectedTyped.nodes | Where-Object { $_.kind -eq 19 })
if ( $rejectedLoops.Count -ne 1 -or @($rejectedLoops | Where-Object {
        $_.loopAbiName -ne "none" -or $_.loopKeyAbiName -ne "none" -or
        $_.loopItemAbiName -ne "none" -or
        $_.loopResourceAbiName -ne "none" -or $_.loopResourceType -ne 0
    }).Count -ne 0 ) {
    throw "Typed XLIR invented a key ABI for set<i64>"
}
$rejectCommand = "`"$xl`" cgen `"$rejectSource`" --backend xlir --emit `"$rejectCPath`" 2>&1"
$rejectOutput = (& $env:ComSpec /d /c $rejectCommand | Out-String)
$rejectExit = $LASTEXITCODE
if ( $rejectExit -eq 0 -or
     ! $rejectOutput.Contains("XLIR for loop has no admitted canonical iterator ABI") ) {
    throw "XLIR Cgen did not reject a two-target set loop without a verified key ABI"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR set<i64> iterator Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_create\(v\d+\);")).Count -ne 9 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_advance\(v\d+, &v\d+\);")).Count -ne 9 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_item_i64\(v\d+\);")).Count -ne 9 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_key_i64\(v\d+\);")).Count -ne 0 -or
     ([regex]::Matches($generated, "\bfor\s*\(")).Count -ne 0 -or
     $generated.Contains("loopAbi") ) {
    throw "XLIR set<i64> iterator C rendering is not mechanical from verified CFG"
}
foreach ( $cursorSymbol in $cursorSymbols ) {
    $loweredReleaseCount = @($lowered.instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $cursorSymbol
    }).Count
    $renderedReleaseCount = Get-XlirRenderedIteratorReleases $generated $cursorSymbol 'xlir_xvalue_iterator*' 'xlir_xvalue_iterator_destroy'
    if ( $renderedReleaseCount -ne $loweredReleaseCount ) {
        throw "Generated C does not preserve every set iterator destroy edge: s$cursorSymbol"
    }
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR set<i64> iterator strict generated-C runtime failed"
}

Write-Output "XLIR set<i64> iterator ABI gate ok: shared xvalue cursor and C helpers, exact value-only projection, no invented key ABI, stable named and owned-rvalue snapshots, LoopResource destroy on return/throw/finally, mechanical C, strict compile, and runtime semantics."
exit 0
