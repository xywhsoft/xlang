param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_erased_array_append.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_erased_array_append.typed.json"
$loweredPath = Join-Path $outputDir "xlir_erased_array_append.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_erased_array_append_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_erased_array_append_generated_again.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR erased array append gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Invoke-Xl @("ir", $source, "--format", "json", "--emit", $typedPath) `
    "XLIR erased array append Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$intrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "array-append"
})
if ( $intrinsics.Count -ne 4 ) {
    throw "Typed XLIR did not preserve erased array append and push"
}
$builtinReceiverCount = 0
$genericDynamicReceiverCount = 0
foreach ( $intrinsic in $intrinsics ) {
    $receiverType = @($typed.types | Where-Object { $_.id -eq $intrinsic.receiverType })
    $operand = $typed.intrinsicOperands[[int]$intrinsic.firstOperand]
    if ( $intrinsic.admissionName -ne "verified" -or
         $intrinsic.collectionAbiName -ne "array" -or
         $intrinsic.resultAbiName -ne "value" -or
         $intrinsic.errorAbiName -ne "no-raise" -or
         $operand.abiName -ne "consume" -or $receiverType.Count -ne 1 -or
         $receiverType[0].name -ne "array" -or
         $receiverType[0].nominalSymbol -ne 0 ) {
        throw "Erased array insertion has no exact receiver and consumed-item ABI"
    }
    if ( $receiverType[0].kind -eq 1 -and $receiverType[0].params.Count -eq 0 ) {
        $builtinReceiverCount += 1
    } elseif ( $receiverType[0].kind -eq 6 -and $receiverType[0].params.Count -eq 1 ) {
        $elementType = @($typed.types | Where-Object {
            $_.id -eq $receiverType[0].params[0]
        })
        if ( $elementType.Count -ne 1 -or $elementType[0].name -ne "var" -or
             $elementType[0].nominalSymbol -ne 0 ) {
            throw "Generic erased array receiver has no canonical open-var element TypeId"
        }
        $genericDynamicReceiverCount += 1
    } else {
        throw "Erased array insertion receiver is neither builtin paramCount=0 nor array<var>"
    }
}
if ( $builtinReceiverCount -ne 2 -or $genericDynamicReceiverCount -ne 2 ) {
    throw "Typed XLIR did not distinguish builtin erased array from generic array<var>"
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR erased array append lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$appends = @($instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-array-append-new"
})
if ( $appends.Count -ne 4 -or
     @($instructions | Where-Object { $_.runtimeOpName -eq "xvalue-i64" }).Count -ne 2 -or
     @($instructions | Where-Object { $_.runtimeOpName -eq "xvalue-string-copy" }).Count -ne 2 ) {
    throw "Lowered XLIR did not preserve explicit owned boxing and erased append-new operations"
}
foreach ( $append in $appends ) {
    $nextIndex = [int]$append.id
    if ( $nextIndex -ge $instructions.Count ) {
        throw "Erased array append is not followed by TRAP_IF_FALSE"
    }
    $next = $instructions[$nextIndex]
    if ( $next.kindName -ne "trap-if-false" -or
         ! (@($next.operands) -contains [int]$append.result) ) {
        throw "Erased array append does not route allocation failure through TRAP_IF_FALSE"
    }
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR erased array append Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR erased array append deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR erased array append C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtValueInt", "xrtValueString", "xrtValueArrayAppendNew",
    "if (!v", "abort()"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR erased array append C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $generatedFull = (Resolve-Path -LiteralPath $generatedPath).Path
    $hostFull = (Resolve-Path -LiteralPath $hostSource).Path
    $exe = Join-Path $outputDir "xlir_erased_array_append_host.exe"
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$generatedFull`" `"$hostFull`" !XL6_CGEN_LINKS! -o `"$exe`" && `"$exe`""
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR erased array append strict generated-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR erased array append gate ok: builtin and array<var> receiver identity, consumed boxing, append-new failure trap, deterministic C and strict runtime."
