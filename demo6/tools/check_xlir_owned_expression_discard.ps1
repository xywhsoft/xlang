param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_owned_expression_discard.xl"
$hostSource = Join-Path $Root "test\cases\xlir_owned_expression_discard_host.c"
$collectionSource = Join-Path $Root "test\cases\cgen_array_literal_expr_elements.xl"
$collectionHostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$borrowedSource = Join-Path $Root "test\cases\bad_xlir_borrowed_managed_expression_discard.xl"
$outputDir = Join-Path $Root "build\test"
$textPath = Join-Path $outputDir "xlir_owned_expression_discard.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_owned_expression_discard.lowered.json"
$cPath = Join-Path $outputDir "xlir_owned_expression_discard_generated.c"
$collectionJsonPath = Join-Path $outputDir "xlir_collection_expression_discard.lowered.json"
$collectionCPath = Join-Path $outputDir "xlir_collection_expression_discard.generated.c"
$borrowedCPath = Join-Path $outputDir "bad_xlir_borrowed_managed_expression_discard.c"
$exePath = Join-Path $outputDir "xlir_owned_expression_discard_host.exe"
$collectionExePath = Join-Path $outputDir "xlir_collection_expression_discard.exe"

foreach ( $path in @(
    $xl, $source, $hostSource, $collectionSource, $collectionHostSource,
    $borrowedSource
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR owned expression discard gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format text --emit $textPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR owned expression discard text IR failed" }
$text = [System.IO.File]::ReadAllText($textPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($text, "kind=temporary")).Count -ne 12 -or
     ([regex]::Matches($text, " release ")).Count -ne 10 -or
     ([regex]::Matches($text, " discard ")).Count -ne 1 -or
     ! $text.Contains("runtime=xvalue-binary-add") ) {
    throw "XLIR owned expression discard has no exact temporary/store/release sequence"
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR owned expression discard JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$temporarySymbols = @{}
foreach ( $storage in @($json.storages | Where-Object { $_.kindName -eq "temporary" }) ) {
    $temporarySymbols[[string]$storage.symbol] = $true
}
$releases = @($json.instructions | Where-Object { $_.kindName -eq "release" })
$discards = @($json.instructions | Where-Object { $_.kindName -eq "discard" })
if ( $temporarySymbols.Count -ne 12 -or $releases.Count -ne 10 -or $discards.Count -ne 1 -or
     @($releases | Where-Object {
         $_.operandCount -ne 0 -or ! $temporarySymbols.ContainsKey([string]$_.targetSymbol)
     }).Count -ne 0 -or $discards[0].operandCount -ne 1 ) {
    throw "XLIR owned expression discard JSON has no verified temporary-storage release ABI"
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR owned expression discard Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($generated, "xrtValueRelease\(s")).Count -ne 10 -or
     ! $generated.Contains("(void)v") ) {
    throw "XLIR owned expression discard C rendering is not mechanical from RELEASE/DISCARD"
}

# A standalone collection literal is still an owned expression.  Typed XLIR
# fixes its xvalue array TypeId; lowering must evaluate and append every item,
# then route the result through the same temporary STORE/RELEASE contract as
# any other discarded managed rvalue.
& $xl ir $collectionSource --lowered --format json --emit $collectionJsonPath
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR collection expression discard JSON IR failed"
}
$collectionJson = [System.IO.File]::ReadAllText(
    $collectionJsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$arrayCreates = @($collectionJson.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-array-new"
})
$arrayAppends = @($collectionJson.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-array-append-new"
})
$collectionDiscardStores = @($collectionJson.instructions | Where-Object {
    $_.kindName -eq "store" -and $_.sourceLine -eq 0
})
$collectionDiscardReleases = @($collectionJson.instructions | Where-Object {
    $_.kindName -eq "release" -and $_.sourceLine -eq 0
})
if ( $arrayCreates.Count -ne 6 -or $arrayAppends.Count -ne 11 -or
     $collectionDiscardStores.Count -ne 2 -or
     $collectionDiscardReleases.Count -ne 2 ) {
    throw "XLIR collection expression has no exact construct/append/temporary-release ABI"
}
foreach ( $release in $collectionDiscardReleases ) {
    if ( @($collectionDiscardStores | Where-Object {
        $_.targetSymbol -eq $release.targetSymbol -and $_.type -eq $release.type
    }).Count -ne 1 ) {
        throw "XLIR discarded collection owner is not released from its exact temporary storage"
    }
}

& $xl cgen $collectionSource --backend xlir --emit $collectionCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR collection expression discard Cgen failed" }
$collectionGenerated = [System.IO.File]::ReadAllText(
    $collectionCPath, [System.Text.Encoding]::UTF8)
if ( ([regex]::Matches($collectionGenerated, " = xrtValueArray\(\);")).Count -ne 6 -or
     ([regex]::Matches($collectionGenerated, " = xrtValueArrayAppendNew\(")).Count -ne 11 ) {
    throw "XLIR collection expression C rendering is not mechanical from RuntimeOps"
}

$previousPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$borrowedOutput = (& $xl cgen $borrowedSource --backend xlir --emit $borrowedCPath 2>&1 | Out-String)
$borrowedExit = $LASTEXITCODE
$ErrorActionPreference = $previousPreference
$normalizedBorrowedOutput = $borrowedOutput -replace '\s+', ''
if ( $borrowedExit -eq 0 -or
     ! $normalizedBorrowedOutput.Contains("XLIRexpressionstatementhasnoexactowned-rvaluestoragereleaseABI") ) {
    throw "XLIR borrowed managed expression discard did not stop at its explicit ownership boundary: $borrowedOutput"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR owned expression discard strict generated-C runtime failed" }

$collectionCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$collectionCPath`" `"$collectionHostSource`" !XL6_CGEN_LINKS! -o `"$collectionExePath`" && `"$collectionExePath`""
& $env:ComSpec /d /v:on /c $collectionCommand
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR collection expression discard strict generated-C runtime failed"
}

Write-Output "XLIR owned expression discard gate ok: owned xvalue and collection rvalues materialize into TEMPORARY storage and RELEASE, collection elements retain ordered RuntimeOps, trivial observations use DISCARD, borrowed managed values remain an explicit boundary, strict C and runtime."
exit 0
