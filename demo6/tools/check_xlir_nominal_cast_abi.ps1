param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_nominal_cast_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_nominal_cast_abi_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_nominal_cast_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_nominal_cast_abi.lowered.json"
$generatedPath = Join-Path $outputDir "xlir_nominal_cast_abi_generated.c"
$generatedAgainPath = Join-Path $outputDir "xlir_nominal_cast_abi_generated_again.c"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR nominal cast ABI gate input is missing: $path"
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
    "XLIR nominal cast Typed IR failed"
$typed = [System.IO.File]::ReadAllText($typedPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$typeByName = @{}
foreach ( $type in @($typed.types) ) {
    if ( $type.name -in @("Base", "Child", "User", "Named") ) {
        $typeByName[$type.name] = $type
    }
}
foreach ( $name in @("Base", "Child", "User", "Named") ) {
    if ( ! $typeByName.ContainsKey($name) ) {
        throw "Typed XLIR nominal fixture lost the $name TypeId"
    }
}
$baseSet = @($typed.nominalIdentitySets | Where-Object {
    $_.targetType -eq $typeByName["Base"].id
})
$childSet = @($typed.nominalIdentitySets | Where-Object {
    $_.targetType -eq $typeByName["Child"].id
})
$userSet = @($typed.nominalIdentitySets | Where-Object {
    $_.targetType -eq $typeByName["User"].id
})
$namedSet = @($typed.nominalIdentitySets | Where-Object {
    $_.targetType -eq $typeByName["Named"].id
})
Assert-Count $baseSet 1 "Typed XLIR lost the Base inheritance closure"
Assert-Count $childSet 1 "Typed XLIR lost the Child inheritance closure"
Assert-Count $userSet 1 "Typed XLIR lost the User identity"
Assert-Count $namedSet 1 "Typed XLIR lost the Named witness closure"
if ( $baseSet[0].kind -ne 1 -or @($baseSet[0].identities).Count -ne 2 -or
     $childSet[0].kind -ne 1 -or @($childSet[0].identities).Count -ne 1 ) {
    throw "Typed XLIR class identity sets do not preserve derived-to-base acceptance"
}
if ( $namedSet[0].kind -ne 3 -or @($namedSet[0].identities).Count -ne 1 -or
     @($userSet[0].identities).Count -ne 1 -or
     [uint64]$namedSet[0].identities[0] -ne
         [uint64]$userSet[0].identities[0] ) {
    throw "Typed XLIR protocol identity set is not the closed witness-class set"
}

$casts = @($typed.nodes | Where-Object {
    $_.sourceKindName -eq "type_as" -and $_.castAbiName -ne "invalid"
})
Assert-Count @($casts | Where-Object {
    $_.castAbiName -eq "xvalue-to-nominal-checked"
}) 5 "Typed XLIR lost checked nominal casts"
Assert-Count @($casts | Where-Object {
    $_.castAbiName -eq "xvalue-to-nullable-nominal-try"
}) 4 "Typed XLIR lost nullable nominal try-casts"
Assert-Count @($casts | Where-Object {
    $_.castAbiName -eq "nullable-nominal-to-nominal-checked"
}) 4 "Typed XLIR lost checked nominal unwraps"
foreach ( $cast in $casts ) {
    $mayRaise = $cast.castAbiName -ne "xvalue-to-nullable-nominal-try"
    $expectedEffect = if ( $mayRaise ) { "may-raise" } else { "no-raise" }
    if ( $cast.castErrorAbiName -ne $expectedEffect ) {
        throw "Typed XLIR nominal cast has the wrong effect: $($cast.castAbiName)"
    }
    $plans = @($typed.exitPlans | Where-Object {
        $_.origin -eq $cast.id -and $_.kind -eq 4
    })
    Assert-Count $plans $(if ($mayRaise) { 1 } else { 0 }) `
        "Typed XLIR nominal cast has the wrong THROW exit-plan count"
}

Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
    "XLIR nominal cast lowering failed"
$lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8) |
    ConvertFrom-Json
$instructions = @($lowered.instructions)
$tests = @($instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-nominal-test"
})
$checked = @($instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-nominal-cast-checked"
})
$nullable = @($instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-nominal-cast-nullable"
})
Assert-Count $tests 10 "Lowered XLIR lost nominal membership predicates"
Assert-Count $checked 5 "Lowered XLIR lost checked nominal runtime casts"
Assert-Count $nullable 4 "Lowered XLIR lost nullable nominal runtime casts"
foreach ( $operation in @($tests + $checked + $nullable) ) {
    if ( $operation.operandCount -ne 1 -or $operation.payloadType -eq 0 ) {
        throw "Lowered nominal operation lost its borrowed source or identity-set target"
    }
}
foreach ( $operation in $checked ) {
    $next = $instructions[[int]$operation.id]
    if ( $next.kindName -ne "error-test" -or
         ! (@($next.operands) -contains [int]$operation.result) ) {
        throw "Checked nominal cast does not route TypeError through adjacent ERROR_TEST"
    }
}
foreach ( $operation in $nullable ) {
    $next = $instructions[[int]$operation.id]
    if ( $next.kindName -eq "error-test" -and
         (@($next.operands) -contains [int]$operation.result) ) {
        throw "No-raise nullable nominal cast unexpectedly owns an ERROR_TEST"
    }
}
$unwraps = @($instructions | Where-Object {
    $_.kindName -eq "nullable-unwrap" -and
    $_.castAbiName -eq "nullable-nominal-to-nominal-checked"
})
Assert-Count $unwraps 4 "Lowered XLIR lost nominal nullable unwrap identities"
foreach ( $unwrap in $unwraps ) {
    $origin = [int]$unwrap.origin
    Assert-Count @($instructions | Where-Object {
        $_.origin -eq $origin -and $_.kindName -eq "null-test"
    }) 1 "Checked nominal unwrap has no unique null proof"
    Assert-Count @($instructions | Where-Object {
        $_.origin -eq $origin -and $_.kindName -eq "error-raise"
    }) 1 "Checked nominal unwrap has no explicit TypeError raise"
}

Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
    "XLIR nominal cast Cgen failed"
Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
    "XLIR nominal cast deterministic Cgen failed"
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
        [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
    ) ) {
    throw "XLIR nominal cast C output is not deterministic"
}
$generated = [System.IO.File]::ReadAllText($generatedPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static bool xlir_nominal_set_",
    "switch (identity)",
    "xlir_nominal_runtime_tag(source)",
    "xrtValueType(source) != 12",
    "dynamic value does not match requested nominal target",
    "xrtValueRetain(xrtValueNull())",
    "xlir_pdispatch_"
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR nominal cast mechanical C emission is incomplete: $required"
    }
}

Push-Location $Root
try {
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generatedPath $hostSource !XL6_CGEN_LINKS! -o build\test\xlir_nominal_cast_abi.exe && build\test\xlir_nominal_cast_abi.exe"
    & $env:ComSpec /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) { throw "XLIR nominal cast strict-C runtime failed" }
} finally {
    Pop-Location
}

Write-Output "XLIR nominal cast ABI gate ok: verified inheritance/witness identity sets, class/protocol is and checked/nullable casts, explicit TypeError CFG, owned xvalue transfer, deterministic strict C and runtime semantics."
