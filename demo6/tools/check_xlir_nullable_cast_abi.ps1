param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_nullable_cast_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_nullable_cast_abi_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_nullable_cast_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_nullable_cast_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_nullable_cast_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_nullable_cast_abi_generated_again.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR nullable cast ABI gate input is missing: $path"
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

$expected = [ordered]@{
    "xvalue-to-nullable-bool-try" = 1
    "xvalue-to-nullable-i64-try" = 2
    "xvalue-to-nullable-u64-try" = 1
    "xvalue-to-nullable-f64-try" = 1
    "xvalue-to-nullable-time-try" = 1
    "xvalue-to-nullable-string-try" = 3
    "value-to-tagged-nullable" = 1
    "managed-to-pointer-nullable" = 2
    "tagged-nullable-to-value-checked" = 2
    "nullable-pointer-to-managed-checked" = 2
}

Invoke-Xl @("ir", $source, "--typed", "--format", "json", "--emit", $typedPath) `
    "XLIR nullable cast Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$casts = @($typed.nodes | Where-Object {
    $_.sourceKindName -eq "type_as" -and $_.castAbiName -ne "invalid"
})
Assert-Count $casts 16 "Typed XLIR did not preserve every nullable cast"
foreach ( $entry in $expected.GetEnumerator() ) {
    Assert-Count @($casts | Where-Object { $_.castAbiName -eq $entry.Key }) `
        $entry.Value "Typed XLIR nullable cast partition is incomplete for $($entry.Key)"
}
foreach ( $cast in $casts ) {
    $mayRaise = $cast.castAbiName -in @(
        "xvalue-to-nullable-string-try",
        "tagged-nullable-to-value-checked",
        "nullable-pointer-to-managed-checked"
    )
    $expectedEffect = if ( $mayRaise ) { "may-raise" } else { "no-raise" }
    if ( $cast.castErrorAbiName -ne $expectedEffect ) {
        throw "Typed XLIR nullable cast has the wrong closed failure ABI: $($cast.castAbiName)"
    }
    $plans = @($typed.exitPlans | Where-Object {
        $_.origin -eq $cast.id -and $_.kind -eq 4
    })
    Assert-Count $plans $(if ($mayRaise) { 1 } else { 0 }) `
        "Typed XLIR nullable cast has the wrong THROW exit-plan count"
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR nullable cast lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$taggedTry = @($instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-cast-nullable-tagged"
})
$stringTry = @($instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-cast-nullable-string"
})
Assert-Count $taggedTry 6 "Lowered XLIR lost tagged nullable try-casts"
Assert-Count $stringTry 3 "Lowered XLIR lost string nullable try-casts"
foreach ( $operation in @($taggedTry + $stringTry) ) {
    if ( $operation.operandCount -ne 1 -or $operation.type -eq 0 -or
         $operation.payloadType -eq 0 -or $operation.castAbiName -eq "invalid" ) {
        throw "Lowered nullable try-cast lost source/result/element/cast identity"
    }
}
foreach ( $operation in $stringTry ) {
    $next = $instructions[[int]$operation.id]
    if ( $next.kindName -ne "error-test" -or
         ! (@($next.operands) -contains [int]$operation.result) ) {
        throw "Nullable string try-cast does not route allocation failure through adjacent ERROR_TEST"
    }
}
foreach ( $operation in $taggedTry ) {
    $next = $instructions[[int]$operation.id]
    if ( $next.kindName -eq "error-test" -and
         (@($next.operands) -contains [int]$operation.result) ) {
        throw "No-raise tagged nullable try-cast unexpectedly owns an ERROR_TEST"
    }
}
$wraps = @($instructions | Where-Object {
    $_.kindName -eq "nullable-wrap" -and $_.castAbiName -ne "invalid"
})
$unwraps = @($instructions | Where-Object {
    $_.kindName -eq "nullable-unwrap" -and $_.castAbiName -ne "invalid"
})
Assert-Count $wraps 3 "Lowered XLIR lost explicit nullable lift identities"
Assert-Count $unwraps 4 "Lowered XLIR lost explicit checked nullable unwrap identities"
foreach ( $unwrap in $unwraps ) {
    $origin = [int]$unwrap.origin
    Assert-Count @($instructions | Where-Object {
        $_.origin -eq $origin -and $_.kindName -eq "null-test"
    }) 1 "Checked nullable unwrap has no unique null proof"
    Assert-Count @($instructions | Where-Object {
        $_.origin -eq $origin -and $_.kindName -eq "error-raise"
    }) 1 "Checked nullable unwrap has no explicit TypeError raise"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR nullable cast Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR nullable cast deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR nullable cast C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_try_cast_xlir_nullable_",
    "xlir_xvalue_type_id(item) != exactTypeId",
    "xrtValueGetBool(item, &value)",
    "xrtValueGetInt(item, &value)",
    "xrtValueGetUInt(item, &value)",
    "xrtValueGetFloat(item, &value)",
    "xrtValueGetTime(item, &value)",
    "xlir_xvalue_try_cast_nullable_string",
    "nullable string cast result allocation failed",
    'xlir_error_create_at("TypeError"'
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR nullable cast mechanical C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_nullable_cast_abi.exe && build\test\xlir_nullable_cast_abi.exe"
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nullable cast strict-C runtime failed" }
} finally {
    Pop-Location
}

Write-Output "XLIR nullable cast ABI gate ok: closed try/lift/unwrap identities, mismatch-null semantics, exact element TypeIds, explicit TypeError CFG, owned string transfer, deterministic strict C and runtime semantics."
