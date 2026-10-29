param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_runtime_is_as.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$supportSource = Join-Path $Root "test\cases\cgen_class_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_composite_cast_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_composite_cast_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_composite_cast_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_composite_cast_abi_generated_again.c"
foreach ( $path in @($xl, $source, $hostSource, $supportSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR composite cast ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

function Assert-Count([object[]]$Items, [int]$Expected, [string]$Message) {
    if ( @($Items).Count -ne $Expected ) {
        throw "$Message (expected $Expected, got $(@($Items).Count))"
    }
}

Invoke-Xl @("ir", $source, "--typed", "--format", "json", "--emit", $typedPath) `
    "XLIR composite cast Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$typedCasts = @($typed.nodes | Where-Object {
    $_.sourceKindName -eq "type_as" -and
    $_.castAbiName -in @("xvalue-to-union-checked", "xvalue-to-closure-checked")
})
Assert-Count @($typedCasts | Where-Object {
    $_.castAbiName -eq "xvalue-to-union-checked"
}) 2 "Typed XLIR lost checked dynamic union casts"
Assert-Count @($typedCasts | Where-Object {
    $_.castAbiName -eq "xvalue-to-closure-checked"
}) 3 "Typed XLIR lost checked dynamic callable casts"
foreach ( $cast in $typedCasts ) {
    if ( $cast.castErrorAbiName -ne "may-raise" -or $cast.storageType -eq 0 ) {
        throw "Typed XLIR composite cast lost its exact target/effect ABI"
    }
    $plans = @($typed.exitPlans | Where-Object {
        $_.origin -eq $cast.id -and $_.kind -eq 4
    })
    Assert-Count $plans 1 "Typed XLIR composite cast has no unique THROW exit plan"
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR composite cast lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$unionCasts = @($instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-union-cast-checked"
})
$closureCasts = @($instructions | Where-Object {
    $_.kindName -eq "runtime-call" -and
    $_.runtimeOpName -eq "xvalue-closure-cast-checked"
})
Assert-Count $unionCasts 2 "Lowered XLIR lost checked union RuntimeOps"
Assert-Count $closureCasts 3 "Lowered XLIR lost checked callable RuntimeOps"
foreach ( $operation in @($unionCasts + $closureCasts) ) {
    $expectedCast = if ( $operation.runtimeOpName -eq "xvalue-union-cast-checked" ) {
        "xvalue-to-union-checked"
    } else {
        "xvalue-to-closure-checked"
    }
    if ( $operation.operandCount -ne 1 -or $operation.type -eq 0 -or
         $operation.payloadType -ne $operation.type -or
         $operation.castAbiName -ne $expectedCast ) {
        throw "Lowered composite cast lost source/result/payload/Cast identity"
    }
    if ( $operation.runtimeOpName -eq "xvalue-union-cast-checked" -and
         $operation.calleeType -ne 0 ) {
        throw "Lowered union cast carries ambiguous callable metadata"
    }
    if ( $operation.runtimeOpName -eq "xvalue-closure-cast-checked" -and
         $operation.calleeType -ne $operation.type ) {
        throw "Lowered callable cast lost its exact reverse-adapter signature"
    }
    $nextIndex = [int]$operation.id
    if ( $nextIndex -ge $instructions.Count ) {
        throw "Lowered composite cast is not followed by ERROR_TEST"
    }
    $next = $instructions[$nextIndex]
    if ( $next.kindName -ne "error-test" -or
         ! (@($next.operands) -contains [int]$operation.result) ) {
        throw "Lowered composite cast does not route failure through adjacent ERROR_TEST"
    }
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR composite cast Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR composite cast deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR composite cast C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static bool xlir_xvalue_union_cast_checked(",
    "xlir_xvalue_type_id(source) != typeId",
    "dynamic value does not match requested union target",
    "static xlir_closure* xlir_xvalue_cast_closure_checked(",
    "!xrtValueIsCallable(source)",
    "dynamic value does not match requested callable target",
    "xlir_xvalue_closure_entry_t",
    "if (xrtGetError() == NULL)"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR composite cast mechanical C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $supportObject = Join-Path $outputDir "xlir_composite_cast_abi_support.o"
    $executable = Join-Path $outputDir "xlir_composite_cast_abi_xlir.exe"
    # The generated module is preincluded only into its host translation unit.
    # Compiling the independent support source separately proves that the gate
    # does not accidentally define the generated module twice.
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -c `"$supportSource`" -o `"$supportObject`" && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include `"$generatedPath`" `"$hostSource`" `"$supportObject`" !XL6_CGEN_LINKS! -o `"$executable`" && `"$executable`""
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR composite cast strict-C runtime failed" }

} finally {
    Pop-Location
}

Write-Output "XLIR composite cast ABI gate ok: exact union/callable identities, recoverable TypeError mismatch, reverse callable adapters, adjacent ERROR_TEST, and deterministic strict-C runtime semantics pass."
