param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_function_direct_owned_args.xl"
$captureSource = Join-Path $Root "test\cases\xlir_closure_closure_capture.xl"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$lowered = Join-Path $Root "build\test\cgen_function_direct_owned_args.gate.xlir"
$loweredJson = Join-Path $Root "build\test\cgen_function_direct_owned_args.gate.json"
$generated = Join-Path $Root "build\test\cgen_function_direct_owned_args.gate.c"
$exe = Join-Path $Root "build\test\cgen_function_direct_owned_args.gate.exe"
$captureJson = Join-Path $Root "build\test\xlir_closure_closure_capture.gate.json"
$captureGenerated = Join-Path $Root "build\test\xlir_closure_closure_capture.gate.c"
$captureExe = Join-Path $Root "build\test\xlir_closure_closure_capture.gate.exe"
# Dedicated lexical-storage and runtime-differential fixtures:
# hir_closure_for_capture.xl, hir_closure_for_named_typed_array.xl,
# hir_closure_for_string_capture.xl.
$loopCaptureNames = @(
    "hir_closure_for_capture",
    "hir_closure_for_named_typed_array",
    "hir_closure_for_string_capture"
)

foreach ( $path in @($xl, $source, $captureSource, $runtimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR function-value ownership gate input is missing: $path"
    }
}
foreach ( $name in $loopCaptureNames ) {
    $path = Join-Path $Root "test\cases\$name.xl"
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR lexical loop-capture gate input is missing: $path"
    }
}

Push-Location $Root
try {
    & $xl ir $source --lowered --emit $lowered
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR function-value ownership lowering failed"
    }
    $text = [System.IO.File]::ReadAllText($lowered, [System.Text.Encoding]::UTF8)
    if ( $text -notmatch 'callable=fixed[^\r\n]*name=function[^\r\n]*params=\[t\d+/default/borrow\]' -or
         $text -notmatch 'storage s\d+ kind=temporary' -or
         $text -notmatch 'runtime=xvalue-retain' -or
         $text -notmatch 'closure-call[^\r\n]*\r?\n\s+i\d+ release ' -or
         ([regex]::Matches($text, 'load[^\r\n]*\r?\n\s+i\d+ binary opcode=add[^\r\n]*\r?\n\s+i\d+ store').Count -lt 2) ) {
        throw "XLIR function-value call lacks an explicit borrowed-parameter or compound-assignment ABI"
    }
    & $xl ir $source --lowered --format json --emit $loweredJson
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR function-value ownership JSON lowering failed"
    }
    $json = [System.IO.File]::ReadAllText($loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    if ( $null -eq $json.types -or
         $null -eq ($json.types | Where-Object { @($_.parameterAbis) -contains "borrow" }) -or
         $null -eq ($json.storages | Where-Object { $_.kindName -eq "temporary" }) ) {
        throw "XLIR JSON type/storage table lost the function parameter or temporary ownership ABI"
    }

    & $xl cgen $source --backend xlir --emit $generated
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR function-value ownership Cgen failed"
    }
    $text = [System.IO.File]::ReadAllText($generated, [System.Text.Encoding]::UTF8)
    foreach ( $required in @('xrtValueRetain(', 'xrtValueRelease(', 'xlir_closure_release(') ) {
        if ( ! $text.Contains($required) ) {
            throw "XLIR function-value generated C lacks required ownership ABI: $required"
        }
    }
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $runtimeHost !XL6_CGEN_LINKS! -o $exe && $exe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR function-value ownership generated-C runtime failed"
    }

    & $xl ir $captureSource --lowered --format json --emit $captureJson
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR nested closure-capture effect lowering failed"
    }
    $captureModule = [System.IO.File]::ReadAllText(
        $captureJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $captureTargets = @($captureModule.closureTargets | Where-Object {
        $_.kind -eq 1 -and $_.captureCount -eq 1 -and $_.symbol -ne 0
    })
    if ( $captureTargets.Count -ne 1 ) {
        throw "XLIR nested callable has no unique closure-owned capture ABI"
    }
    $captureFunction = @($captureModule.functions | Where-Object {
        $_.symbol -eq $captureTargets[0].symbol -and
        $_.declaration -eq $captureTargets[0].declaration -and
        $_.errorAbi -eq "may-raise"
    })
    if ( $captureFunction.Count -ne 1 ) {
        throw "XLIR nested callable has no effect-bearing function CFG"
    }
    $firstCaptureInstruction = [int]$captureFunction[0].firstInstruction + 1
    $lastCaptureInstruction = [int]$captureFunction[0].firstInstruction +
        [int]$captureFunction[0].instructionCount
    $captureBody = @($captureModule.instructions | Where-Object {
        $_.id -ge $firstCaptureInstruction -and $_.id -le $lastCaptureInstruction
    })
    if ( @($captureBody | Where-Object { $_.kindName -eq "closure-call" }).Count -ne 2 -or
         @($captureBody | Where-Object {
             $_.kindName -eq "error-test" -and $_.targetSymbol -eq 0
         }).Count -ne 2 -or
         @($captureBody | Where-Object { $_.kindName -eq "throw" }).Count -ne 2 -or
         @($captureBody | Where-Object { $_.kindName -eq "release" }).Count -ne 0 -or
         @($captureModule.storages | Where-Object {
             $_.kindName -eq "capture" -and $_.owner -eq $captureTargets[0].declaration
         }).Count -ne 1 ) {
        throw "XLIR nested callable effect released enclosing cleanup instead of its closure environment"
    }

    & $xl cgen $captureSource --backend xlir --emit $captureGenerated
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR nested closure-capture effect Cgen failed"
    }
    $captureCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $captureGenerated $runtimeHost !XL6_CGEN_LINKS! -o $captureExe && $captureExe"
    & cmd.exe /d /v:on /c $captureCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR nested closure-capture effect generated-C runtime failed"
    }

    foreach ( $name in $loopCaptureNames ) {
        $loopSource = Join-Path $Root "test\cases\$name.xl"
        $loopJson = Join-Path $Root "build\test\$name.gate.json"
        $loopGenerated = Join-Path $Root "build\test\$name.gate.c"
        $loopExe = Join-Path $Root "build\test\$name.gate.exe"

        & $xl ir $loopSource --lowered --format json --emit $loopJson
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR lexical loop-capture lowering failed: $name"
        }
        $loopModule = [System.IO.File]::ReadAllText(
            $loopJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $loopCaptures = @($loopModule.closureCaptures)
        if ( $loopCaptures.Count -ne 1 ) {
            throw "XLIR lexical loop-capture fixture has no unique environment transfer: $name"
        }
        $loopCapture = $loopCaptures[0]
        $expectedCaptureAbi = if ( $name -eq "hir_closure_for_string_capture" ) {
            "copy-string"
        } else {
            "copy-value"
        }
        $loopSourceNode = @($loopModule.nodes | Where-Object {
            $_.symbol -eq $loopCapture.sourceSymbol -and
            $_.kindName -eq "name" -and $_.targetSymbol -eq 0
        })[0]
        $targetsNode = if ( $null -ne $loopSourceNode ) {
            @($loopModule.nodes | Where-Object {
                $_.id -eq $loopSourceNode.parent
            })[0]
        } else { $null }
        $forNode = if ( $null -ne $targetsNode ) {
            @($loopModule.nodes | Where-Object {
                $_.id -eq $targetsNode.parent
            })[0]
        } else { $null }
        $sourceStorage = @($loopModule.storages | Where-Object {
            $_.symbol -eq $loopCapture.sourceSymbol
        })
        $captureStorage = @($loopModule.storages | Where-Object {
            $_.symbol -eq $loopCapture.storageSymbol
        })
        if ( $loopCapture.abiName -ne $expectedCaptureAbi -or
             $null -eq $loopSourceNode -or
             $null -eq $targetsNode -or
             $targetsNode.sourceKindName -ne "for_targets" -or
             $null -eq $forNode -or $forNode.sourceKindName -ne "for" -or
             $sourceStorage.Count -ne 1 -or
             $sourceStorage[0].kindName -ne "local" -or
             $sourceStorage[0].declaration -ne $loopSourceNode.id -or
             $captureStorage.Count -ne 1 -or
             $captureStorage[0].kindName -ne "capture" -or
             $captureStorage[0].type -ne $loopCapture.type ) {
            throw "XLIR for-target did not become an explicit lexical storage and closure-frame transfer: $name"
        }

        & $xl cgen $loopSource --backend xlir --emit $loopGenerated
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR lexical loop-capture Cgen failed: $name"
        }
        $loopC = [System.IO.File]::ReadAllText(
            $loopGenerated, [System.Text.Encoding]::UTF8)
        foreach ( $required in @(
            "xlir_closure_env_", "environment)->c0",
            "environment_free"
        ) ) {
            if ( ! $loopC.Contains($required) ) {
                throw "XLIR lexical loop-capture generated C lost its mechanical frame ABI: $name / $required"
            }
        }
        if ( $expectedCaptureAbi -eq "copy-string" -and
             (! $loopC.Contains("->c0 = xrtStrDup(") -or
              ! $loopC.Contains("xrtFree(environment->c0)")) ) {
            throw "XLIR string loop capture lost its copy/drop pair: $name"
        }
        $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $loopGenerated $runtimeHost !XL6_CGEN_LINKS! -o $loopExe && $loopExe"
        & cmd.exe /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR lexical loop-capture strict-C runtime failed: $name"
        }
    }
} finally {
    Pop-Location
}

Write-Output "XLIR function signature borrow, universal closure-call effect, nested capture cleanup ownership, lexical for-target frame transfers, string copy/drop, strict runtimes and scalar compound-assignment ABIs ok."
