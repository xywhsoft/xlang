param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_dynamic_container_methods.xl"
$hostPath = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$precedenceSource = Join-Path $Root "test\cases\cgen_dynamic_method_arguments.xl"
$precedenceHost = Join-Path $Root "test\cases\xlir_dynamic_method_arguments_host.c"
$outputDir = Join-Path $Root "build\test"
foreach ( $path in @($xl, $source, $hostPath, $precedenceSource, $precedenceHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR dynamic collection-method gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Read-IrModule([bool]$lowered, [string]$output) {
    $arguments = @("ir", $source)
    if ( $lowered ) { $arguments += "--lowered" }
    $arguments += @("--format", "json", "--emit", $output)
    & $xl @arguments
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR dynamic collection-method IR dump failed"
    }
    return [System.IO.File]::ReadAllText(
        $output, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
}

function Get-Type($module, [int]$id) {
    return @($module.types | Where-Object { [int]$_.id -eq $id })[0]
}

$typedPath = Join-Path $outputDir "cgen_dynamic_container_methods.dynamic.typed.json"
$loweredPath = Join-Path $outputDir "cgen_dynamic_container_methods.dynamic.lowered.json"
$typed = Read-IrModule $false $typedPath
$lowered = Read-IrModule $true $loweredPath
$intrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "dynamic-collection-member"
})
$expectedOperations = @{
    "add" = 1
    "clear" = 4
    "contains" = 7
    "index-of" = 2
    "insert" = 1
    "pop" = 2
    "push" = 3
    "remove" = 4
    "remove-value" = 3
    "reserve" = 1
    "swap" = 1
    "take" = 3
}
foreach ( $expected in $expectedOperations.GetEnumerator() ) {
    $matches = @($intrinsics | Where-Object {
        $_.dynamicCollectionOpName -eq $expected.Key
    })
    if ( $matches.Count -ne [int]$expected.Value ) {
        throw "XLIR dynamic collection operation count changed: $($expected.Key)"
    }
}
if ( $intrinsics.Count -ne
     ($expectedOperations.Values | Measure-Object -Sum).Sum ) {
    throw "XLIR dynamic collection-method gate found an unexpected operation"
}

foreach ( $intrinsic in $intrinsics ) {
    $receiverType = Get-Type $typed ([int]$intrinsic.receiverType)
    $resultType = Get-Type $typed ([int]$intrinsic.resultType)
    $operands = @($typed.intrinsicOperands |
        Select-Object -Skip ([int]$intrinsic.firstOperand) `
                      -First ([int]$intrinsic.operandCount))
    if ( $intrinsic.admissionName -ne "verified" -or
         $intrinsic.dynamicCollectionOpName -eq "invalid" -or
         $intrinsic.typedCollectionOpName -ne "invalid" -or
         $intrinsic.collectionAbiName -ne "none" -or
         $intrinsic.resultAbiName -ne "owned" -or
         $intrinsic.errorAbiName -ne "no-raise" -or
         $null -eq $receiverType -or $null -eq $resultType -or
         [int]$receiverType.kind -ne 1 -or [int]$receiverType.cAbi -ne 26 -or
         [int]$receiverType.ownershipAbi -ne 2 -or
         [int]$receiverType.nullAbi -ne 2 -or
         $receiverType.collectionAbiName -ne "none" -or
         [int]$receiverType.paramCount -ne 0 -or
         [int]$intrinsic.resultType -ne [int]$intrinsic.receiverType ) {
        throw "XLIR dynamic collection method lost its exact open-xvalue ABI"
    }
    foreach ( $operand in $operands ) {
        if ( $operand.abiName -ne "consume" ) {
            throw "XLIR dynamic collection method operand is not an explicit consumed xvalue"
        }
    }
    $runtime = @($lowered.instructions | Where-Object {
        [int]$_.origin -eq [int]$intrinsic.origin -and
        $_.runtimeOpName -eq "xvalue-dynamic-collection-method"
    })
    if ( $runtime.Count -ne 1 -or
         $runtime[0].dynamicCollectionOpName -ne
             $intrinsic.dynamicCollectionOpName -or
         $runtime[0].typedCollectionOpName -ne "invalid" -or
         $runtime[0].collectionAbiName -ne "none" -or
         [int]$runtime[0].type -ne [int]$intrinsic.resultType -or
         [int]$runtime[0].operandCount -ne
             ([int]$intrinsic.operandCount + 1) ) {
        throw "XLIR dynamic collection method has no unique mechanical lowered operation"
    }
}

$runtimeOperations = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-dynamic-collection-method"
})
if ( $runtimeOperations.Count -ne $intrinsics.Count ) {
    throw "XLIR dynamic collection lowering emitted an unexpected runtime operation"
}

$generated = Join-Path $outputDir "cgen_dynamic_container_methods.dynamic.c"
$exe = Join-Path $outputDir "cgen_dynamic_container_methods.dynamic.exe"
& $xl cgen $source --backend xlir --emit $generated
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR dynamic collection-method Cgen failed"
}
$generatedText = [System.IO.File]::ReadAllText(
    $generated, [System.Text.Encoding]::UTF8)
$requiredTokens = @(
    "XLIR_DYNAMIC_COLLECTION_PUSH",
    "XLIR_DYNAMIC_COLLECTION_INSERT",
    "XLIR_DYNAMIC_COLLECTION_REMOVE",
    "XLIR_DYNAMIC_COLLECTION_REMOVE_VALUE",
    "XLIR_DYNAMIC_COLLECTION_POP",
    "XLIR_DYNAMIC_COLLECTION_TAKE",
    "XLIR_DYNAMIC_COLLECTION_CLEAR",
    "XLIR_DYNAMIC_COLLECTION_RESERVE",
    "XLIR_DYNAMIC_COLLECTION_SWAP",
    "XLIR_DYNAMIC_COLLECTION_ADD",
    "XLIR_DYNAMIC_COLLECTION_CONTAINS",
    "XLIR_DYNAMIC_COLLECTION_INDEX_OF"
)
foreach ( $token in $requiredTokens ) {
    if ( ! $generatedText.Contains($token) ) {
        throw "XLIR generated C lost dynamic collection operation: $token"
    }
}

Push-Location $Root
try {
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $hostPath !XL6_CGEN_LINKS! -o $exe && $exe"
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR dynamic collection-method strict-C runtime failed"
    }
} finally {
    Pop-Location
}

# Open `var` dispatch is class-first even when a class method has a standard
# collection spelling.  The collection operation remains an explicit IR
# fallback, never a Cgen spelling heuristic.
$precedenceTypedPath = Join-Path $outputDir "cgen_dynamic_method_arguments.precedence.typed.json"
$precedenceLoweredPath = Join-Path $outputDir "cgen_dynamic_method_arguments.precedence.lowered.json"
& $xl ir $precedenceSource --format json --emit $precedenceTypedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic method/collection precedence typed IR failed" }
& $xl ir $precedenceSource --lowered --format json --emit $precedenceLoweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic method/collection precedence lowered IR failed" }
$precedenceTyped = [System.IO.File]::ReadAllText(
    $precedenceTypedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$precedenceLowered = [System.IO.File]::ReadAllText(
    $precedenceLoweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$fallbackSites = @($precedenceTyped.dynamicCallSites | Where-Object {
    $_.kindName -eq "method" -and $_.memberName -eq "add" -and
    $_.fallbackCollectionOpName -eq "add" -and
    [int]$_.positionalCount -eq 1 -and [int]$_.keywordCount -eq 0
})
$namedClassOnlySites = @($precedenceTyped.dynamicCallSites | Where-Object {
    $_.kindName -eq "method" -and $_.memberName -eq "add" -and
    $_.fallbackCollectionOpName -eq "invalid" -and
    [int]$_.positionalCount -eq 0 -and [int]$_.keywordCount -eq 2
})
$fallbackRuntime = @($precedenceLowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-dynamic-method-call" -and
    $_.dynamicCollectionOpName -eq "add" -and [int]$_.operandCount -eq 2
})
if ( $fallbackSites.Count -ne 2 -or $namedClassOnlySites.Count -ne 1 -or
     $fallbackRuntime.Count -ne 2 ) {
    throw "XLIR dynamic method/collection precedence has no exact class-first fallback ABI"
}
$precedenceGenerated = Join-Path $outputDir "cgen_dynamic_method_arguments_xlir_generated.c"
$precedenceExe = Join-Path $outputDir "xlir_dynamic_method_arguments_host.exe"
& $xl cgen $precedenceSource --backend xlir --emit $precedenceGenerated
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR dynamic method/collection precedence Cgen failed" }
$precedenceGeneratedText = [System.IO.File]::ReadAllText(
    $precedenceGenerated, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "if (xrtValueType(receiver) != 12)",
    "xlir_xvalue_dynamic_collection_method(receiver, XLIR_DYNAMIC_COLLECTION_ADD",
    "dynamic method call has no matching verified overload"
) ) {
    if ( ! $precedenceGeneratedText.Contains($required) ) {
        throw "XLIR generated C lost dynamic method/collection precedence: $required"
    }
}
Push-Location $Root
try {
    $precedenceCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $precedenceHost !XL6_CGEN_LINKS! -o $precedenceExe && $precedenceExe"
    & $env:ComSpec /d /v:on /c $precedenceCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR dynamic method/collection precedence strict-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR dynamic collection-method gate ok: 12 explicit operations, class-first dynamic dispatch with explicit collection fallback, consumed arguments, owned results, mechanical lowering, strict C, and runtime."
exit 0
