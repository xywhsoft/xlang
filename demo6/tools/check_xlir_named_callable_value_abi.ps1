param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$fixtures = @(
    @{ Name = "cgen_var_call_arg_destroy_class"; NamedCount = 1; StrictC = $true },
    @{ Name = "extlib_tar_progress_min"; NamedCount = 1; StringCallback = $true },
    @{ Name = "extlib_tar_progress"; NamedCount = 2; StringCallback = $true }
)

foreach ( $path in @($xl, $runtimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR named-callable value gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

Push-Location $Root
try {
    foreach ( $fixture in $fixtures ) {
        $name = $fixture.Name
        $source = Join-Path $Root "test\cases\$name.xl"
        $typedPath = Join-Path $outputDir "$name.named_callable.typed.json"
        $loweredPath = Join-Path $outputDir "$name.named_callable.lowered.json"
        $cPath = Join-Path $outputDir "$name.named_callable.xlir.c"
        $cRepeatPath = Join-Path $outputDir "$name.named_callable.repeat.xlir.c"

        if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
            throw "XLIR named-callable source is missing: $source"
        }
        & $xl ir $source --format json --emit $typedPath
        if ( $LASTEXITCODE -ne 0 ) { throw "$name Typed XLIR failed" }
        & $xl ir $source --lowered --format json --emit $loweredPath
        if ( $LASTEXITCODE -ne 0 ) { throw "$name Lowered XLIR failed" }
        & $xl cgen $source --backend xlir --emit $cPath
        if ( $LASTEXITCODE -ne 0 ) { throw "$name XLIR Cgen failed" }
        & $xl cgen $source --backend xlir --emit $cRepeatPath
        if ( $LASTEXITCODE -ne 0 ) { throw "$name repeated XLIR Cgen failed" }
        if ( ! [System.Linq.Enumerable]::SequenceEqual(
                [byte[]][System.IO.File]::ReadAllBytes($cPath),
                [byte[]][System.IO.File]::ReadAllBytes($cRepeatPath)) ) {
            throw "$name XLIR Cgen is not byte deterministic"
        }

        $typed = [System.IO.File]::ReadAllText(
            $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $lowered = [System.IO.File]::ReadAllText(
            $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $named = @($typed.nodes | Where-Object {
            $_.namedCallableAbiName -eq "capture-free-adapter"
        })
        if ( $named.Count -ne $fixture.NamedCount ) {
            throw "$name has an unexpected named-callable ABI population"
        }
        foreach ( $node in $named ) {
            $signature = @($typed.types | Where-Object {
                $_.id -eq $node.namedCallableSignatureType
            })
            if ( $node.targetSymbol -eq 0 -or
                 $node.namedCallableTargetSymbol -eq 0 -or
                 $node.targetSymbol -eq $node.namedCallableTargetSymbol -or
                 $node.namedCallableSignatureType -eq 0 -or
                 $signature.Count -ne 1 -or $signature[0].kind -ne 4 -or
                 $signature[0].cAbi -ne 31 -or
                 $signature[0].callableErasureAbiName -notin
                     @("fixed", "array-variadic") ) {
                throw "$name named callable did not separate frontend name identity from executable declaration/signature identity"
            }
            if ( $fixture.StringCallback ) {
                $firstParameter = @($typed.types | Where-Object {
                    $_.id -eq $signature[0].params[0]
                })
                if ( $firstParameter.Count -ne 1 -or
                     $firstParameter[0].cAbi -ne 25 -or
                     $signature[0].parameterAbis[0] -ne "borrow" ) {
                    throw "$name callback lost its explicit managed-string borrow ABI"
                }
            }

            $make = @($lowered.instructions | Where-Object {
                $_.origin -eq $node.id -and $_.kindName -eq "closure-make" -and
                $_.type -eq $node.namedCallableSignatureType -and
                $_.targetSymbol -ne 0
            })
            $adapter = @($lowered.closureTargets | Where-Object {
                $_.kind -eq 2 -and $_.symbol -eq $make[0].targetSymbol -and
                $_.adaptedTargetSymbol -eq $node.namedCallableTargetSymbol -and
                $_.signatureType -eq $node.namedCallableSignatureType -and
                $_.captureCount -eq 0
            })
            $oldLoads = @($lowered.instructions | Where-Object {
                $_.kindName -eq "load" -and $_.targetSymbol -eq $node.targetSymbol
            })
            if ( $make.Count -ne 1 -or $adapter.Count -ne 1 -or
                 $oldLoads.Count -ne 0 ) {
                throw "$name did not lower its named callable to one capture-free adapter without declaration-name storage"
            }
            $boxes = @($lowered.instructions | Where-Object {
                $_.kindName -eq "runtime-call" -and
                $_.runtimeOpName -eq "xvalue-callable-from-closure" -and
                $_.calleeType -eq $node.namedCallableSignatureType
            })
            $checkedBoxes = @($boxes | Where-Object {
                $box = $_
                @($lowered.instructions | Where-Object {
                    $_.id -eq $box.id + 1 -and $_.kindName -eq "error-test" -and
                    $_.origin -eq $box.origin
                }).Count -eq 1
            })
            if ( $boxes.Count -lt 1 -or $checkedBoxes.Count -ne $boxes.Count ) {
                throw "$name callable erasure has no explicit checked runtime conversion"
            }
        }

        $generated = [System.IO.File]::ReadAllText(
            $cPath, [System.Text.Encoding]::UTF8)
        foreach ( $forbidden in @(
            "namedCallableAbi", "namedCallableTargetSymbol",
            "namedCallableSignatureType", "xl6IrLower", "xl6Cgen"
        ) ) {
            if ( $generated.Contains($forbidden) ) {
                throw "$name generated C leaked frontend/backend inference: $forbidden"
            }
        }

        if ( $fixture.StrictC ) {
            $exe = Join-Path $outputDir "$name.named_callable.xlir.exe"
            $strictCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$runtimeHost`" !XL6_CGEN_LINKS! -o `"$exe`" && `"$exe`""
            & $env:ComSpec /d /v:on /c $strictCommand
            if ( $LASTEXITCODE -ne 0 ) {
                throw "$name strict generated-C runtime failed"
            }
        }

        & $xl run $source --backend xlir
        if ( $LASTEXITCODE -ne 0 ) { throw "$name XLIR package runtime failed" }
        & $xl run $source --backend legacy
        if ( $LASTEXITCODE -ne 0 ) { throw "$name legacy differential runtime failed" }
    }
} finally {
    Pop-Location
}

Write-Output "XLIR named-callable value ABI gate ok: canonical declaration/signature identity, managed-string callback bridge, capture-free adapter, checked callable erasure, deterministic Cgen, strict C, package runtime, and legacy differential."
exit 0
