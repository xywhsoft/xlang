param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'xlir_iterator_gate_contract.ps1')

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_xvalue_iterator_scalar_projection.xl"
$rejectSource = Join-Path $Root "test\cases\cgen_xlir_set_u64_iterator_reject.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedJsonPath = Join-Path $outputDir "xlir_xvalue_iterator_scalar_projection.typed.json"
$loweredJsonPath = Join-Path $outputDir "xlir_xvalue_iterator_scalar_projection.lowered.json"
$rejectTypedJsonPath = Join-Path $outputDir "xlir_set_u64_iterator_reject.typed.json"
$rejectCPath = Join-Path $outputDir "xlir_set_u64_iterator_reject.c"
$cPath = Join-Path $outputDir "xlir_xvalue_iterator_scalar_projection.c"
$exePath = Join-Path $outputDir "xlir_xvalue_iterator_scalar_projection_host.exe"

foreach ( $path in @($xl, $source, $rejectSource, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR xvalue iterator scalar projection gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue iterator scalar projection typed JSON failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$loops = @($typed.nodes | Where-Object {
    $_.loopAbiName -like "xvalue-iterator-*"
})
if ( $loops.Count -ne 5 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-key-value" -and
        $_.loopKeyAbiName -eq "integer-i64"
     }).Count -ne 3 -or
     @($loops | Where-Object {
        $_.loopAbiName -eq "xvalue-iterator-value" -and
        $_.loopKeyAbiName -eq "none"
     }).Count -ne 2 ) {
    throw "Typed XLIR scalar iterator target/key layout is not exact"
}
foreach ( $projection in @(
    @{ Name = "u64"; RuntimeOp = "xvalue-iterator-item-u64"; Count = 1 },
    @{ Name = "bool"; RuntimeOp = "xvalue-iterator-item-bool"; Count = 2 },
    @{ Name = "f64"; RuntimeOp = "xvalue-iterator-item-f64"; Count = 2 }
) ) {
    if ( @($loops | Where-Object {
            $_.loopItemAbiName -eq $projection.Name
         }).Count -ne $projection.Count ) {
        throw "Typed XLIR scalar iterator item projection count changed: $($projection.Name)"
    }
}
$collectionTypes = @($typed.types | Where-Object {
    $_.kind -eq 6 -and $_.collectionAbiName -in @("list", "set") -and
    $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and @($_.params).Count -eq 1
})
$iteratorTypes = @($typed.types | Where-Object {
    $_.kind -eq 14 -and $_.cAbi -eq 37 -and $_.ownershipAbi -eq 2 -and
    $_.nullAbi -eq 1 -and @($_.params).Count -eq 1
})
if ( $collectionTypes.Count -ne 5 -or $iteratorTypes.Count -ne 5 ) {
    throw "Typed XLIR scalar iterator TypeId matrix is incomplete"
}
foreach ( $loop in $loops ) {
    $cursorType = @($iteratorTypes | Where-Object { $_.id -eq $loop.loopResourceType })
    if ( $cursorType.Count -ne 1 -or
         $loop.loopResourceAbiName -ne "xvalue-iterator-destroy" -or
         @($collectionTypes | Where-Object { $_.id -eq $cursorType[0].params[0] }).Count -ne 1 ) {
        throw "Typed XLIR scalar loop has no exact source-bound cursor TypeId: n$($loop.id)"
    }
}

& $xl ir $rejectSource --typed --format json --emit $rejectTypedJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR set<u64> iterator rejection typed JSON failed" }
$rejectedTyped = [System.IO.File]::ReadAllText(
    $rejectTypedJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$rejectedLoops = @($rejectedTyped.nodes | Where-Object { $_.kind -eq 19 })
if ( $rejectedLoops.Count -ne 1 -or @($rejectedLoops | Where-Object {
        $_.loopAbiName -ne "none" -or $_.loopItemAbiName -ne "none" -or
        $_.loopResourceAbiName -ne "none" -or $_.loopResourceType -ne 0
    }).Count -ne 0 ) {
    throw "Typed XLIR admitted set<u64> beyond the current XRT set element ABI"
}
$rejectCommand = "`"$xl`" cgen `"$rejectSource`" --backend xlir --emit `"$rejectCPath`" 2>&1"
$rejectOutput = (& $env:ComSpec /d /c $rejectCommand | Out-String)
$rejectExit = $LASTEXITCODE
if ( $rejectExit -eq 0 -or
     ! $rejectOutput.Contains("XLIR for loop has no admitted canonical iterator ABI") ) {
    throw "XLIR lowering did not preserve the explicit set<u64> runtime-contract rejection"
}

& $xl ir $source --lowered --format json --emit $loweredJsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue iterator scalar projection lowering failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$loopIds = @($loops | ForEach-Object { Get-XlirIteratorOrigins $typed $_ })
$loopInstructions = @($lowered.instructions | Where-Object { $_.origin -in $loopIds })
$cursorSymbols = @()
foreach ( $loop in $loops ) {
    Assert-XlirCheckedIteratorAdvance $lowered $loop
    $origins = @(Get-XlirIteratorOrigins $typed $loop)
    $instructions = @($loopInstructions | Where-Object { $_.origin -in $origins })
    $expectedItemOp = "xvalue-iterator-item-$($loop.loopItemAbiName)"
    $cursorStores = @(Get-XlirIteratorCursorStore $lowered $loop)
    $expectedKeyCount = if ( $loop.loopAbiName -eq "xvalue-iterator-key-value" ) { 1 } else { 0 }
    if ( @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-create"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-advance"
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq $expectedItemOp
         }).Count -ne 1 -or
         @($instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-iterator-key-i64"
         }).Count -ne $expectedKeyCount -or
         $cursorStores.Count -ne 1 ) {
        throw "Lowered XLIR scalar iterator operations are not canonical for n$($loop.id)"
    }
    $cursorSymbol = [long]$cursorStores[0].targetSymbol
    $cursorSymbols += $cursorSymbol
    if ( @($instructions | Where-Object {
            $_.kindName -eq "release" -and $_.type -eq $loop.loopResourceType -and
            $_.targetSymbol -eq $cursorSymbol
         }).Count -ne 2 ) {
        throw "Lowered XLIR scalar iterator has no unique canonical destroy: n$($loop.id)"
    }
}
if ( @($cursorSymbols | Select-Object -Unique).Count -ne 5 ) {
    throw "Lowered XLIR scalar iterator cursor storage aliases across loops"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR xvalue iterator scalar projection Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
foreach ( $projection in @(
    @{ Name = "u64"; RuntimeOp = "xvalue-iterator-item-u64"; Count = 1 },
    @{ Name = "bool"; RuntimeOp = "xvalue-iterator-item-bool"; Count = 2 },
    @{ Name = "f64"; RuntimeOp = "xvalue-iterator-item-f64"; Count = 2 }
) ) {
    if ( ([regex]::Matches($generated,
            "=\s*xlir_xvalue_iterator_item_$($projection.Name)\(v\d+\);")).Count -ne $projection.Count -or
         ([regex]::Matches($generated,
            "static .* xlir_xvalue_iterator_item_$($projection.Name)\(")).Count -ne 1 ) {
        throw "Generated C does not mechanically render scalar iterator projection: $($projection.Name)"
    }
}
if ( ([regex]::Matches($generated,
        "=\s*xl6ListValueIteratorCreateV1\(v\d+\);")).Count -ne 3 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_create\(v\d+\);")).Count -ne 2 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_advance\(v\d+, &v\d+\);")).Count -ne 5 -or
     ([regex]::Matches($generated,
        "=\s*xlir_xvalue_iterator_key_i64\(v\d+\);")).Count -ne 3 -or
     ([regex]::Matches($generated, "\bfor\s*\(")).Count -ne 0 -or
     $generated.Contains("loopAbi") ) {
    throw "XLIR scalar iterator C rendering is not mechanical from verified CFG"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR xvalue iterator scalar projection strict generated-C runtime failed"
}

Write-Output "XLIR xvalue iterator scalar projection gate ok: list/set share one cursor CFG, integer list keys remain orthogonal, list u64 plus list/set bool/f64 item RuntimeOps are exact, current XRT set<u64> rejection stays explicit, Cgen is mechanical, strict compile and runtime semantics pass."
exit 0
