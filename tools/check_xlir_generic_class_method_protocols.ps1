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
$cases = @(
    [pscustomobject]@{
        Name = "cgen_generic_self_method"
        Source = Join-Path $Root "test\cases\cgen_generic_self_method.xl"
    },
    [pscustomobject]@{
        Name = "cgen_generic_class_iter_protocol"
        Source = Join-Path $Root "test\cases\cgen_generic_class_iter_protocol.xl"
    }
)

foreach ( $path in @($xl, $hostSource) + @($cases.Source) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR generic class method/protocol gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$documents = @{}
foreach ( $case in $cases ) {
    $jsonPath = Join-Path $outputDir ($case.Name + ".lowered.json")
    $xlirCPath = Join-Path $outputDir ($case.Name + ".xlir.c")
    $repeatCPath = Join-Path $outputDir ($case.Name + ".xlir.repeat.c")

    & $xl ir $case.Source --lowered --format json --emit $jsonPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR generic class method/protocol lowering failed: $($case.Name)"
    }
    $documents[$case.Name] =
        [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) |
        ConvertFrom-Json

    & $xl cgen $case.Source --backend xlir --emit $xlirCPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR generic class method/protocol Cgen failed: $($case.Name)"
    }
    & $xl cgen $case.Source --backend xlir --emit $repeatCPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "repeated XLIR generic class method/protocol Cgen failed: $($case.Name)"
    }
    if ( [System.IO.File]::ReadAllText($xlirCPath, [System.Text.Encoding]::UTF8) -cne
         [System.IO.File]::ReadAllText($repeatCPath, [System.Text.Encoding]::UTF8) ) {
        throw "XLIR generic class method/protocol Cgen is not deterministic: $($case.Name)"
    }
    $exe = Join-Path $outputDir ($case.Name + ".xlir.exe")
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$xlirCPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exe`" && `"$exe`""
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "generic class method/protocol generated-C runtime failed: $($case.Name)"
    }
}

$selfDocument = $documents["cgen_generic_self_method"]
$selfSpecializations = @($selfDocument.genericSpecializations)
if ( $selfSpecializations.Count -ne 3 -or
     @($selfSpecializations.symbol | Sort-Object -Unique).Count -ne 3 -or
     @($selfSpecializations | Where-Object { @($_.typeArgs).Count -ne 1 }).Count -ne 0 ) {
    throw "generic self-call lowering did not close one exact monomorphic call graph"
}

$iteratorDocument = $documents["cgen_generic_class_iter_protocol"]
$iteratorSpecializations = @($iteratorDocument.genericSpecializations)
$loops = @($iteratorDocument.nodes | Where-Object { $_.kindName -eq "loop" })
$iteratorSymbols = @($iteratorSpecializations.symbol)
if ( $iteratorSpecializations.Count -ne 4 -or $loops.Count -ne 2 -or
     $loops[0].loopAbiName -ne "protocol-iterator-value" -or
     $loops[1].loopAbiName -ne "protocol-iterator-index-value" -or
     @($loops | Where-Object {
         $_.loopErrorAbiName -ne "may-raise" -or
         $_.loopResourceAbiName -ne "protocol-iterator-release" -or
         $_.loopProtocolIterTarget -eq 0 -or
         $_.loopProtocolNextTarget -eq 0 -or
         $_.loopProtocolIteratorType -eq 0 -or
         $_.loopProtocolNextResultType -eq 0 -or
         $_.loopProtocolItemType -eq 0 -or
         $iteratorSymbols -notcontains $_.loopProtocolIterTarget -or
         $iteratorSymbols -notcontains $_.loopProtocolNextTarget
     }).Count -ne 0 -or
     @($loops.loopProtocolIterTarget | Sort-Object -Unique).Count -ne 1 -or
     @($loops.loopProtocolNextTarget | Sort-Object -Unique).Count -ne 1 -or
     @($loops.loopProtocolIteratorType | Sort-Object -Unique).Count -ne 1 -or
     @($iteratorDocument.classLayouts | Where-Object {
         $_.type -eq $loops[0].loopProtocolIteratorType
     }).Count -ne 1 ) {
    throw "generic iterator protocol did not freeze one concrete LoopAbi/class-layout contract"
}

Write-Output "XLIR generic class method/protocol gate ok: self-call closure, implicit iterator specialization, concrete LoopAbi/ClassLayout facts, deterministic strict C, and canonical runtime agreement."
exit 0
