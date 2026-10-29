param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_var_runtime_objects.xl"
$runtimeHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$expectSupport = Join-Path $Root "test\cases\xlir_expect_support.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "runtime_callable_value.typed.json"
$loweredPath = Join-Path $outputDir "runtime_callable_value.lowered.json"
$cPath = Join-Path $outputDir "runtime_callable_value.xlir.c"
$cRepeatPath = Join-Path $outputDir "runtime_callable_value.repeat.xlir.c"
$exe = Join-Path $outputDir "runtime_callable_value.xlir.exe"

foreach ( $path in @(
    $xl, $source, $runtimeHost, $expectSupport
) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR runtime-callable value gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "runtime-callable Typed XLIR failed" }
& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "runtime-callable Lowered XLIR failed" }
& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "runtime-callable XLIR Cgen failed" }
& $xl cgen $source --backend xlir --emit $cRepeatPath
if ( $LASTEXITCODE -ne 0 ) { throw "repeated runtime-callable XLIR Cgen failed" }
if ( ! [System.Linq.Enumerable]::SequenceEqual(
        [byte[]][System.IO.File]::ReadAllBytes($cPath),
        [byte[]][System.IO.File]::ReadAllBytes($cRepeatPath)) ) {
    throw "runtime-callable XLIR Cgen is not byte deterministic"
}

$typed = [System.IO.File]::ReadAllText(
    $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$products = @($lowered.types | Where-Object { $_.cAbi -eq 32 })
foreach ($arity in @(2, 5)) {
    if (@($products | Where-Object { $_.params.Count -eq $arity }).Count -ne 1) {
        throw "Runtime callable lost its exact $arity-field product result ABI"
    }
}
# Class and native-backed class values retain nominal language identity even
# though both have the same physical xvalue representation.
foreach ($className in @("Pair", "Native")) {
    $class = @($lowered.types | Where-Object { $_.name -eq $className })
    if ($class.Count -ne 1 -or $class[0].cAbi -ne 26 -or $class[0].ownershipAbi -ne 2 -or
        $class[0].typeValueId -eq "0x0000000000000000" -or
        @($lowered.instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-class-identity-bind" -and
            $_.payloadType -eq $class[0].id -and $_.type -eq $class[0].id
        }).Count -eq 0) { throw "Erased $className value lost its nominal managed identity" }
}
if (@($lowered.instructions | Where-Object { $_.runtimeOpName -eq "xvalue-native-handle-take" }).Count -eq 0) {
    throw "Native-backed class boxing lost explicit handle ownership transfer"
}
$sites = @($typed.dynamicCallSites)
if ( $sites.Count -ne 7 -or
     @($sites | Where-Object {
         $_.resultAbiName -ne "owned-first-value" -or
         $_.errorAbiName -ne "may-raise"
     }).Count -ne 0 -or
     @($sites | Where-Object { $_.keywordCount -gt 0 }).Count -ne 2 ) {
    throw "Typed XLIR lost its explicit owned-first dynamic-call projection or keyword frame partition"
}

$dynamic = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-dynamic-call"
})
if ( $dynamic.Count -ne $sites.Count ) {
    throw "Lowered XLIR did not preserve every verified dynamic-call site"
}
foreach ( $instruction in $dynamic ) {
    $site = @($sites | Where-Object {
        $_.id -eq $instruction.dynamicCallSite
    })
    $test = @($lowered.instructions | Where-Object {
        $_.id -eq ([int]$instruction.id + 1) -and
        $_.kindName -eq "error-test" -and $_.operandCount -eq 1 -and
        $_.operands[0] -eq $instruction.result
    })
    if ( $site.Count -ne 1 -or
         $instruction.dynamicCallResultAbiName -ne "owned-first-value" -or
         $site[0].resultAbiName -ne $instruction.dynamicCallResultAbiName -or
         $test.Count -ne 1 ) {
        throw "Lowered dynamic call lost its linked projection, frame, or immediate error ABI"
    }
}

$callableBoxes = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -in @(
        "xvalue-callable-from-closure", "xvalue-nullable-closure-box"
    )
})
if ( $callableBoxes.Count -lt 6 ) {
    throw "runtime-callable fixture has no complete closure erasure population"
}
foreach ( $instruction in $callableBoxes ) {
    $identity = @($lowered.types | Where-Object {
        $_.id -eq $instruction.runtimeIdentityType
    })
    if ( $instruction.calleeType -eq 0 -or
         $instruction.runtimeIdentityType -ne $instruction.calleeType -or
         $identity.Count -ne 1 -or
         $identity[0].typeValueId -eq "0x0000000000000000" ) {
        throw "closure erasure lost its concrete callable signature identity"
    }
}

$generated = [System.IO.File]::ReadAllText(
    $cPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xlir_xvalue_dynamic_call_owned(",
    "if (xrtCallResultCount(&result) == 0)",
    "xrtValueTypeIdBind(value, typeId)",
    "xrtValueObjectSetNew("
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "generated runtime-callable C lost a verified ABI operation: $required"
    }
}
foreach ( $forbidden in @(
    "dynamic call did not return exactly one value",
    "xrtCallResultCount(&result) != 1",
    "xlir_xvalue_dynamic_call("
) ) {
    if ( $generated.Contains($forbidden) ) {
        throw "generated runtime-callable C regressed to an inferred call-result policy: $forbidden"
    }
}

Push-Location $Root
try {
    $strictCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$runtimeHost`" !XL6_CGEN_LINKS! -o `"$exe`" && `"$exe`""
    & $env:ComSpec /d /v:on /c $strictCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "runtime-callable strict generated-C runtime failed"
    }
} finally {
    Pop-Location
}

& $xl run $source $expectSupport --backend xlir
if ( $LASTEXITCODE -ne 0 ) {
    throw "runtime-callable XLIR package runtime failed"
}
Write-Output "XLIR runtime-callable value ABI gate ok: concrete signature TypeValueId, dict/array variadic adapters, explicit owned-first multi-result projection, immediate error CFG, deterministic Cgen, strict C, and package runtime."
exit 0
