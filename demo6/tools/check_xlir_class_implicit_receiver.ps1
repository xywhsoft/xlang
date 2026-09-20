param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_class_implicit_receiver.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$loweredJson = Join-Path $outputDir "xlir_class_implicit_receiver.lowered.json"
$generated = Join-Path $outputDir "xlir_class_implicit_receiver.gate.c"
$exe = Join-Path $outputDir "xlir_class_implicit_receiver.gate.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR declaration-driven class receiver gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

Push-Location $Root
try {
    & $xl ir $source --lowered --format json --emit $loweredJson
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR declaration-driven class receiver JSON lowering failed"
    }
    $json = [System.IO.File]::ReadAllText($loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $receiverTypes = @($json.types | Where-Object { $_.name -eq "ReceiverFacts" })
    if ( $receiverTypes.Count -ne 1 ) {
        throw "XLIR declaration-driven class receiver type is not unique"
    }
    $receiver = $receiverTypes[0]
    if ( $receiver.kind -ne 2 -or
         $receiver.collectionAbiName -ne "none" -or
         $receiver.runtimeResourceAbiName -ne "none" -or
         $receiver.persistenceValueAbiName -ne "class-schema" -or
         $receiver.cAbi -ne 26 -or
         $receiver.callableErasureAbiName -ne "invalid" -or
         $receiver.ownershipAbi -ne 2 -or
         $receiver.nullAbi -ne 2 -or
         $receiver.nominalSymbol -le 0 -or
         $receiver.typeValueId -notmatch '^0x[0-9A-F]{16}$' -or
         $receiver.typeValueId -eq '0x0000000000000000' ) {
        throw "XLIR declaration-driven class receiver has no canonical nominal xvalue TypeId"
    }
    $receiverType = [int]$receiver.id

    $targets = @($json.callTargets | Where-Object {
        $_.kind -eq 1 -and $_.reachability -eq 'transitive' -and
        $_.parameterCount -ge 1 -and
        @($_.parameterTypes)[0] -eq $receiverType -and
        @($_.parameterAbis)[0] -eq 2
    })
    if ( $targets.Count -ne 4 ) {
        throw "XLIR constructor/getter/setter/method do not each expose canonical slot-zero borrow ABI"
    }
    foreach ( $target in $targets ) {
        $functions = @($json.functions | Where-Object {
            $_.kind -eq 2 -and $_.symbol -eq $target.symbol -and
            $_.declaration -eq $target.declaration -and
            $_.parameterCount -eq $target.parameterCount -and
            $_.returnType -eq $target.returnType
        })
        if ( $functions.Count -ne 1 ) {
            throw "XLIR class callable function CFG does not match its declaration-driven receiver target"
        }
    }

    & $xl cgen $source --backend xlir --emit $generated
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR declaration-driven class receiver Cgen failed"
    }
    $generatedText = [System.IO.File]::ReadAllText($generated, [System.Text.Encoding]::UTF8)
    if ( ([regex]::Matches($generatedText, 'static\s+(?:void|int64_t)\s+xlir_fn_[0-9]+\(xvalue\*')).Count -lt 4 ) {
        throw "XLIR generated C does not preserve all four explicit borrowed receiver parameters"
    }
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $hostSource !XL6_CGEN_LINKS! -o $exe && $exe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR declaration-driven class receiver generated-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR declaration-driven class receiver gate ok: constructor/getter/setter/method canonical slot-zero borrow ABI, strict C, and runtime."
