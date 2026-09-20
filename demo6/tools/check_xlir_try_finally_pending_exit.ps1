param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$xlirHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$cases = @(
    [pscustomobject]@{
        Name = "cgen_try_finally_pending_control"
        Source = Join-Path $Root "test\cases\cgen_try_finally_pending_control.xl"
    },
    [pscustomobject]@{
        Name = "cgen_try_finally_pending_async"
        Source = Join-Path $Root "test\cases\cgen_try_finally_pending_async.xl"
    }
)

foreach ( $path in @($xl, $xlirHost) + @($cases.Source) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR try/finally pending-exit gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$documents = @{}
foreach ( $case in $cases ) {
    $jsonPath = Join-Path $outputDir ($case.Name + ".lowered.json")
    & $xl ir $case.Source --lowered --format json --emit $jsonPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR try/finally pending-exit lowering failed: $($case.Name)"
    }
    $documents[$case.Name] =
        [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) |
        ConvertFrom-Json

    foreach ( $backend in @("xlir", "legacy") ) {
        $cPath = Join-Path $outputDir ($case.Name + "." + $backend + ".c")
        & $xl cgen $case.Source --backend $backend --emit $cPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "try/finally pending-exit Cgen failed: $($case.Name) $backend"
        }
        if ( $backend -eq "xlir" ) {
            $repeatPath = Join-Path $outputDir ($case.Name + ".xlir.repeat.c")
            & $xl cgen $case.Source --backend xlir --emit $repeatPath
            if ( $LASTEXITCODE -ne 0 -or
                 [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8) -cne
                 [System.IO.File]::ReadAllText($repeatPath, [System.Text.Encoding]::UTF8) ) {
                throw "XLIR try/finally pending-exit Cgen is not deterministic: $($case.Name)"
            }
        }
        $exe = Join-Path $outputDir ($case.Name + "." + $backend + ".exe")
        if ( $backend -eq "xlir" ) {
            $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$xlirHost`" !XL6_CGEN_LINKS! -o `"$exe`" && `"$exe`""
        } else {
            # Legacy owns main and imports its historical async adapters from
            # xl.dll. Keep that compatibility boundary out of the XLIR host,
            # which links directly to the generated-C XRT object above.
            $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror `"$cPath`" !XL6_CGEN_XLANG_LINKS! -o `"$exe`" && `"$exe`""
        }
        & $env:ComSpec /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) {
            throw "try/finally pending-exit runtime failed: $($case.Name) $backend"
        }
    }
}

function Get-OverrideOwnerRelease($document, [int]$line) {
    # Diagnostic THROW and lexical cleanup SCOPE have separate IR identities.
    $throws = @($document.nodes | Where-Object { $_.kindName -eq 'throw' -and $_.line -eq $line })
    if ($throws.Count -ne 1) { throw 'Missing finally override throw' }
    $plans = @($document.exitPlans | Where-Object { $_.origin -eq $throws[0].id -and $_.pointName -eq 'before-node' })
    if ($plans.Count -ne 1) { throw 'Missing typed override exit route' }
    $scope = $document.exitSteps[$plans[0].firstStep]
    $begins = @($document.instructions | Where-Object { $_.kindName -eq 'error-unwind-begin' -and $_.origin -eq $throws[0].id })
    if ($scope.kindName -ne 'scope' -or $begins.Count -ne 1) { throw 'Override lost its lexical unwind boundary' }
    $releases = @($document.instructions | Where-Object {
        $_.kindName -eq 'release' -and $_.origin -eq $scope.origin -and $_.errorScope -eq $begins[0].errorScope
    })
    foreach ($release in $releases) {
        $storage = @($document.storages | Where-Object symbol -eq $release.targetSymbol)
        if ($storage.Count -ne 1 -or $storage[0].kindName -ne 'temporary' -or
            @($document.instructions | Where-Object {
                $_.targetSymbol -eq $release.targetSymbol -and $_.kindName -in @('store','product-store-move')
            }).Count -ne 1) { throw 'Finally override has no unique initialized pending owner' }
    }
    return $releases
}

$control = $documents["cgen_try_finally_pending_control"]
$productMoves = @($control.instructions | Where-Object {
    $_.kindName -eq "product-store-move"
})
if ( $productMoves.Count -ne 2 -or
     $productMoves[0].type -ne $productMoves[1].type -or
     $productMoves[0].targetSymbol -eq $productMoves[1].targetSymbol ) {
    throw "try/finally product return must transfer through distinct local and finally pending owners"
}
$productProducer = @($control.instructions | Where-Object {
    $_.result -eq $productMoves[0].operands[0] -and $_.result -ne 0
})
if ( $productMoves[0].operandCount -ne 1 -or
     $productProducer.Count -ne 1 -or
     $productProducer[0].kindName -ne "product-make" -or
     $productProducer[0].type -ne $productMoves[0].type -or
     $productProducer[0].index -ge $productMoves[0].index ) {
    throw "try/finally product return must prepare the complete owned product before cleanup"
}
for ( $i = 0; $i -lt $productMoves.Count; $i++ ) {
    $productMove = $productMoves[$i]
    $storage = @($control.storages | Where-Object {
        $_.symbol -eq $productMove.targetSymbol -and $_.kindName -eq "temporary"
    })
    $productClears = @($control.instructions | Where-Object {
        $_.kindName -eq "move" -and $_.targetSymbol -eq $productMove.targetSymbol
    })
    if ( $storage.Count -ne 1 -or $productClears.Count -ne 1 -or
         $productClears[0].type -ne $productMove.type -or
         $productClears[0].operandCount -ne 1 -or
         $productClears[0].index -le $productMove.index + 1 -or
         $productClears[0].index + 1 -ge $control.instructions.Count ) {
        throw "try/finally pending product owner must be cleared exactly once after preparation"
    }
    $clear = $productClears[0]
    $load = $control.instructions[$clear.index - 1]
    $consumer = $control.instructions[$clear.index + 1]
    if ( $load.kindName -ne "load" -or $load.type -ne $productMove.type -or
         $load.targetSymbol -ne $productMove.targetSymbol -or
         $load.result -eq 0 -or $clear.operands[0] -ne $load.result -or
         $consumer.operandCount -ne 1 -or $consumer.operands[0] -ne $load.result ) {
        throw "try/finally product handoff must use an adjacent matching LOAD/MOVE/consumer"
    }
    if ( ($i -eq 0 -and $consumer.index -ne $productMoves[1].index) -or
         ($i -eq 1 -and $consumer.kindName -ne "return") ) {
        throw "try/finally product owner chain must end in finally storage then terminal return"
    }
}
if ( @(Get-OverrideOwnerRelease $control 83).Count -ne 1 -or
     @($control.instructions | Where-Object {
         $_.kindName -eq "discard" -and $_.type -eq 0
     }).Count -ne 0 ) {
    throw "sync try/finally pending-result transfer, override cleanup, or typed discard ABI is incomplete"
}

$async = $documents["cgen_try_finally_pending_async"]
$asyncStarts = @($async.instructions | Where-Object { $_.kindName -eq "async-start" })
$awaits = @($async.instructions | Where-Object { $_.kindName -eq "await" })
if ( $asyncStarts.Count -ne 3 -or $awaits.Count -ne 3 -or
     $async.asyncMachineCount -ne 0 -or $async.asyncSuspensionCount -ne 0 -or
     @(Get-OverrideOwnerRelease $async 23).Count -ne 1 -or
     @($async.instructions | Where-Object {
         $_.kindName -eq "discard" -and $_.type -eq 0
     }).Count -ne 0 ) {
    throw "async try/finally pending-result state machine or override cleanup ABI is incomplete"
}

Write-Output "XLIR try/finally pending-exit gate ok: scalar, string, class, array and product returns; nested/overriding return, throw, break and continue; async Future entry/await ABI; typed discards; deterministic strict C; and XLIR/legacy runtime agreement."
exit 0
