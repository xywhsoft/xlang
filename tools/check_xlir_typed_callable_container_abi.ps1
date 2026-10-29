param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_typed_container_functions.xl"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_diagnostic_host.c"
$support = Join-Path $Root "test\cases\cgen_class_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_typed_callable_container_abi.typed.json"
$loweredJson = Join-Path $outputDir "xlir_typed_callable_container_abi.lowered.json"
$generatedC = Join-Path $outputDir "xlir_typed_callable_container_abi.c"
$generatedCSecond = Join-Path $outputDir "xlir_typed_callable_container_abi.second.c"

foreach ( $path in @($xl, $source, $genericHost, $support) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR typed callable-container ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Get-TypeById($document, $id) {
    return @($document.types | Where-Object { $_.id -eq $id })[0]
}

function Get-NodeById($document, $id) {
    return @($document.nodes | Where-Object { $_.id -eq $id })[0]
}

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed callable-container Typed IR construction failed"
}
& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed callable-container lowering failed"
}
$typed = [System.IO.File]::ReadAllText(
    $typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [System.IO.File]::ReadAllText(
    $loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json

$literalEffects = @($typed.nodes | Where-Object {
    $_.kindName -eq "collection" -and
    $_.sourceKindName -in @(
        "array_literal", "list_literal", "set_literal", "dict_literal") -and
    $_.implicitConversionErrorAbiName -eq "may-raise"
})
if ( $literalEffects.Count -ne 4 ) {
    throw "XLIR callable-container literals do not own four exact implicit conversion effects: $($literalEffects.Count)"
}
foreach ( $literal in $literalEffects ) {
    $plans = @($typed.exitPlans | Where-Object {
        $_.origin -eq $literal.id -and $_.kind -eq 4 -and
        $_.targetScopeDepth -eq 0 -and $_.targetRegion -eq 0
    })
    if ( $plans.Count -ne 1 ) {
        throw "XLIR callable-container literal n$($literal.id) has no unique THROW ExitPlan"
    }
}

$boxes = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-callable-from-closure"
})
if ( $boxes.Count -ne 5 ) {
    throw "XLIR callable-container checked callable construction count is incomplete: $($boxes.Count)"
}
$literalBoxes = @($boxes | Where-Object {
    (Get-NodeById $lowered $_.origin).implicitConversionErrorAbiName -eq
        "may-raise"
})
if ( $literalBoxes.Count -ne 4 ) {
    throw "XLIR callable-container literals did not emit four effect-linked callable constructors"
}
foreach ( $box in $boxes ) {
    $signature = Get-TypeById $lowered $box.calleeType
    $next = @($lowered.instructions)[$box.index + 1]
    if ( $box.operandCount -ne 1 -or $box.callableErasureAbiName -ne "fixed" -or
         $null -eq $signature -or $signature.name -ne "function" -or
         $signature.cAbi -ne 31 -or $signature.ownershipAbi -ne 2 -or
         $null -eq $next -or $next.kindName -ne "error-test" -or
         $next.origin -ne $box.origin -or $next.operandCount -ne 1 -or
         $next.operands[0] -ne $box.result ) {
        throw "XLIR callable-container construction has no exact signature, erasure, and immediate failure CFG ABI"
    }
}

$loops = @($typed.nodes | Where-Object {
    $_.sourceKindName -eq "for" -and
    $_.loopItemAbiName -eq "closure-copy" -and
    $_.loopItemLifetimeAbiName -eq "owned-iteration-release" -and
    $_.loopResourceAbiName -eq "xvalue-iterator-destroy"
})
if ( $loops.Count -ne 4 ) {
    throw "XLIR callable-container array/list/set/dict loop ABI coverage is incomplete: $($loops.Count)"
}
$projections = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-project-closure-copy"
})
if ( $projections.Count -ne 8 ) {
    throw "XLIR callable-container owned closure projection count is incomplete: $($projections.Count)"
}
foreach ( $projection in $projections ) {
    $signature = Get-TypeById $lowered $projection.type
    if ( $projection.operandCount -ne 1 -or
         $projection.calleeType -ne $projection.type -or
         $null -eq $signature -or $signature.name -ne "function" -or
         $signature.cAbi -ne 31 -or $signature.ownershipAbi -ne 2 ) {
        throw "XLIR callable-container projection has no exact owned signature-carrying closure ABI"
    }
}

$expressionCalls = @($typed.nodes | Where-Object {
    $_.kindName -eq "call" -and $_.callKindName -eq "function-value" -and
    $_.targetSymbol -eq 0
})
if ( $expressionCalls.Count -ne 3 ) {
    throw "XLIR callable-container complex callee normalization is incomplete: $($expressionCalls.Count)"
}
foreach ( $call in $expressionCalls ) {
    $instructions = @($lowered.instructions | Where-Object {
        $_.origin -eq $call.id -and $_.kindName -eq "closure-call"
    })
    if ( $instructions.Count -ne 1 -or
         $instructions[0].calleeType -eq 0 -or
         $instructions[0].operandCount -ne 3 ) {
        throw "XLIR callable-container expression call has no unique exact CLOSURE_CALL"
    }
    $invoke = $instructions[0]
    $calleeLoad = @($lowered.instructions | Where-Object {
        $_.result -eq $invoke.operands[0] -and $_.kindName -eq "load"
    })
    if ( $calleeLoad.Count -ne 1 -or $calleeLoad[0].targetSymbol -lt 0xF0000000 ) {
        throw "XLIR callable-container owned projection is not anchored across invocation"
    }
    $release = @($lowered.instructions | Where-Object {
        $_.kindName -eq "release" -and $_.targetSymbol -eq $calleeLoad[0].targetSymbol -and
        $_.type -eq $invoke.calleeType -and $_.index -gt $invoke.index
    })
    $errorTest = @($lowered.instructions | Where-Object {
        $_.kindName -eq "error-test" -and $_.origin -eq $invoke.origin -and
        $_.index -gt $invoke.index
    } | Select-Object -First 1)
    if ( $release.Count -ne 1 -or $errorTest.Count -ne 1 -or
         $release[0].index -ge $errorTest[0].index ) {
        throw "XLIR callable-container owned projection must release before both invocation result edges"
    }
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed callable-container C generation failed"
}
& $xl cgen $source --backend xlir --emit $generatedCSecond
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR typed callable-container repeat C generation failed"
}
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedC),
        [byte[]][System.IO.File]::ReadAllBytes($generatedCSecond)) ) {
    throw "XLIR typed callable-container generated C is not byte-for-byte deterministic"
}
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static xvalue* xlir_closure_to_xvalue_scalar(",
    "static xlir_closure* xlir_xvalue_project_closure_try_copy(",
    " = xlir_closure_to_xvalue_scalar(v",
    " = xlir_xvalue_project_closure_try_copy(v",
    "xlir_closure_invoke_"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR typed callable-container generated C is missing mechanical IR emission: $required"
    }
}
foreach ( $forbidden in @(
    "xl6_array_function_get", "xl6_list_function_get",
    "xl6_dict_function_get", "xl6_infer_callable_container",
    "xl6_source_callee_kind"
) ) {
    if ( $c.Contains($forbidden) ) {
        throw "XLIR typed callable-container Cgen reconstructed source semantics: $forbidden"
    }
}

Push-Location $Root
try {
    $xlirCommand = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_typed_callable_container_abi.c test\cases\xlir_generated_module_diagnostic_host.c test\cases\cgen_class_host.c !XL6_CGEN_XLANG_LINKS! -o build\test\xlir_typed_callable_container_abi.exe && build\test\xlir_typed_callable_container_abi.exe'
    & $env:ComSpec /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR typed callable-container strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR typed callable-container ABI gate passed: four effect-owned literals, checked exact boxing, closure-copy loops and reads, expression-callee normalization, deterministic strict C, and runtime assertions."
