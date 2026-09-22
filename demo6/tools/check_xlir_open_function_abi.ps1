param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_function_bare_call.xl"
$xlirRuntimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test\xlir_open_function"
$typed = Join-Path $outputDir "typed.json"
$lowered = Join-Path $outputDir "lowered.json"
$generatedA = Join-Path $outputDir "generated-a.c"
$generatedB = Join-Path $outputDir "generated-b.c"
$xlirExe = Join-Path $outputDir "xlir.exe"

foreach ( $path in @($xl, $source, $xlirRuntimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR open-function ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format json --emit $typed
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR open-function typed IR construction failed"
}
$typedModule = [System.IO.File]::ReadAllText(
    $typed, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$openTypes = @($typedModule.types | Where-Object {
    $_.kind -eq 4 -and $_.callableErasureAbiName -eq "open" -and
    $_.cAbi -eq 26 -and $_.ownershipAbi -eq 2 -and $_.nullAbi -eq 2 -and
    @($_.params).Count -eq 0 -and @($_.returns).Count -eq 0 -and
    $_.callableResultType -eq 0
})
$fixedTypes = @($typedModule.types | Where-Object {
    $_.kind -eq 4 -and $_.callableErasureAbiName -eq "fixed" -and
    $_.cAbi -eq 31 -and $_.callableResultType -ne 0
})
$arrayVariadicTypes = @($typedModule.types | Where-Object {
    $_.kind -eq 4 -and $_.callableErasureAbiName -eq "array-variadic" -and
    $_.cAbi -eq 31 -and @($_.params).Count -eq 2 -and
    $_.callableResultType -ne 0
})
if ( $openTypes.Count -ne 1 -or $fixedTypes.Count -lt 1 -or
     $arrayVariadicTypes.Count -ne 1 ) {
    throw "XLIR function TypeIds do not separate open erasure from exact closure signatures"
}

$callableInitializers = @($typedModule.valueInitializers | Where-Object {
    $_.runtimeOpName -eq "xvalue-callable-from-closure" -and
    $_.errorAbiName -eq "may-raise" -and
    $_.callableErasureAbiName -in @("fixed", "array-variadic")
})
if ( $callableInitializers.Count -ne 3 -or
     @($callableInitializers | Where-Object {
         $_.callableErasureAbiName -eq "fixed"
     }).Count -ne 1 -or
     @($callableInitializers | Where-Object {
         $_.callableErasureAbiName -eq "array-variadic"
     }).Count -ne 2 ) {
    throw "XLIR open-function initializers lack exact callable construction ABIs"
}
foreach ( $initializer in $callableInitializers ) {
    if ( @($typedModule.exitPlans | Where-Object {
             $_.origin -eq $initializer.declaration -and $_.kind -eq 4 -and
             $_.targetScopeDepth -eq 0 -and $_.targetRegion -eq 0
         }).Count -ne 1 ) {
        throw "XLIR may-raise callable initializer has no unique explicit ExitPlan"
    }
}

$dynamicSites = @($typedModule.dynamicCallSites | Where-Object {
    $_.kindName -eq "function" -and $_.errorAbiName -eq "may-raise" -and
    $_.fallbackCollectionOpName -eq "invalid"
})
if ( $dynamicSites.Count -ne 4 -or
     @($dynamicSites | Where-Object { $_.keywordCount -ne 0 }).Count -ne 0 ) {
    throw "XLIR open-function calls are not canonical DynamicCallSites"
}

& $xl ir $source --lowered --format json --emit $lowered
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR open-function lowering failed"
}
$loweredModule = [System.IO.File]::ReadAllText(
    $lowered, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$constructionInstructions = @($loweredModule.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-callable-from-closure" -and
    $_.callableErasureAbiName -in @("fixed", "array-variadic") -and
    $_.calleeType -ne 0 -and @($_.operands).Count -eq 1
})
$dynamicInstructions = @($loweredModule.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-dynamic-call" -and
    $_.dynamicCallSite -ge 0
})
$namedArrayAdapter = @($loweredModule.closureTargets | Where-Object {
    $_.kind -eq 2 -and $_.captureCount -eq 0 -and
    $_.signatureType -eq $arrayVariadicTypes[0].id -and
    @($_.callParameterNames).Count -eq 2
})
if ( $constructionInstructions.Count -ne 3 -or
     $dynamicInstructions.Count -ne 4 -or
     $namedArrayAdapter.Count -ne 1 -or
     @($loweredModule.instructions | Where-Object {
         $_.kindName -eq "error-test"
     }).Count -lt 7 ) {
    throw "XLIR lowering lost callable construction, dynamic invocation, adapter or failure CFG"
}

& $xl cgen $source --backend xlir --emit $generatedA
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR open-function Cgen failed"
}
& $xl cgen $source --backend xlir --emit $generatedB
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR open-function deterministic Cgen repeat failed"
}
$generatedAText = [System.IO.File]::ReadAllText(
    $generatedA, [System.Text.Encoding]::UTF8)
$generatedBText = [System.IO.File]::ReadAllText(
    $generatedB, [System.Text.Encoding]::UTF8)
if ( $generatedAText -cne $generatedBText ) {
    throw "XLIR open-function Cgen is not deterministic"
}
foreach ( $required in @(
    "xlir_closure_to_xvalue_scalar(",
    "xlir_xvalue_dynamic_call_owned(",
    "xrtValueArray()",
    "xrtValueArrayAppend(",
    "runtime array-variadic callable frame"
) ) {
    if ( ! $generatedAText.Contains($required) ) {
        throw "XLIR generated C lacks a mechanical open-callable ABI: $required"
    }
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$generatedA`" `"$xlirRuntimeHost`" !XL6_CGEN_LINKS! -o `"$xlirExe`" && `"$xlirExe`""
    & cmd.exe /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR open-function generated-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR open function erasure, fixed/array-variadic callable adapters, explicit failure CFG, deterministic Cgen, and strict runtime ABI ok."
