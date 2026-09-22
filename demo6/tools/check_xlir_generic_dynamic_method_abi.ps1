param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\cgen_generic_dynamic_method.xl"
$runtimeHost = Join-Path $Root "test\cases\xlir_generic_dynamic_method_host.c"
$outputDir = Join-Path $Root "build\test"
$typedA = Join-Path $outputDir "cgen_generic_dynamic_method.typed.a.json"
$typedB = Join-Path $outputDir "cgen_generic_dynamic_method.typed.b.json"
$loweredA = Join-Path $outputDir "cgen_generic_dynamic_method.lowered.a.json"
$loweredB = Join-Path $outputDir "cgen_generic_dynamic_method.lowered.b.json"
$generated = Join-Path $outputDir "cgen_generic_dynamic_method_xlir_generated.c"
$generatedAgain = Join-Path $outputDir "cgen_generic_dynamic_method_xlir_generated.again.c"
$xlirExe = Join-Path $outputDir "cgen_generic_dynamic_method_xlir.exe"

foreach ( $path in @($xl, $source, $runtimeHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR generic dynamic-method gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --format json --emit $typedA
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic dynamic-method typed IR failed" }
& $xl ir $source --format json --emit $typedB
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic dynamic-method repeated typed IR failed" }
& $xl ir $source --lowered --format json --emit $loweredA
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic dynamic-method lowered IR failed" }
& $xl ir $source --lowered --format json --emit $loweredB
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic dynamic-method repeated lowered IR failed" }

if ( (Get-FileHash -Algorithm SHA256 $typedA).Hash -ne
         (Get-FileHash -Algorithm SHA256 $typedB).Hash -or
     (Get-FileHash -Algorithm SHA256 $loweredA).Hash -ne
         (Get-FileHash -Algorithm SHA256 $loweredB).Hash ) {
    throw "XLIR generic dynamic-method IR is not byte-for-byte deterministic"
}

$lowered = [System.IO.File]::ReadAllText(
    $loweredA, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$sites = @($lowered.dynamicCallSites | Where-Object {
    $_.kindName -eq "method" -and
    $_.resultAbiName -eq "owned-first-value" -and
    $_.errorAbiName -eq "may-raise" -and
    $_.fallbackCollectionOpName -eq "invalid"
})
$runtimeCalls = @($lowered.instructions | Where-Object {
    $_.runtimeOpName -eq "xvalue-dynamic-method-call"
})
$verified = @($lowered.dynamicMethods | Where-Object {
    $_.adapterAbiName -eq "verified" -and
    [int]$_.runtimeTypeTag -eq 1 -and
    $_.name -in @("get", "replace")
})
$unsupportedSameName = @($lowered.dynamicMethods | Where-Object {
    $_.adapterAbiName -eq "unsupported" -and $_.name -eq "get"
})
if ( $sites.Count -ne 3 -or $runtimeCalls.Count -ne 3 -or
     $verified.Count -ne 2 -or $unsupportedSameName.Count -lt 1 ) {
    throw "XLIR generic dynamic-method fixture lost its verified/unsupported candidate partition"
}
foreach ( $site in $sites ) {
    $runtime = @($runtimeCalls | Where-Object {
        [int]$_.origin -eq [int]$site.origin -and
        [int]$_.dynamicCallSite -eq [int]$site.id -and
        $_.dynamicCallResultAbiName -eq "owned-first-value" -and
        [int]$_.operandCount -eq ([int]$site.argumentCount + 1)
    })
    if ( $runtime.Count -ne 1 ) {
        throw "XLIR generic dynamic-method call has no unique mechanical RuntimeOp"
    }
}
foreach ( $method in $verified ) {
    $target = @($lowered.callTargets | Where-Object {
        [uint64]$_.symbol -eq [uint64]$method.targetSymbol
    })
    if ( $target.Count -ne 1 -or [int]$target[0].kind -ne 1 -or
         $target[0].reachability -ne "transitive" -or
         [int]$target[0].returnType -ne [int]$method.returnType -or
         [int]$target[0].parameterCount -ne ([int]$method.parameterCount + 1) ) {
        throw "XLIR generic dynamic-method adapter lost its exact specialized CallTarget ABI"
    }
}

& $xl cgen $source --backend xlir --emit $generated
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic dynamic-method Cgen failed" }
& $xl cgen $source --backend xlir --emit $generatedAgain
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR generic dynamic-method repeated Cgen failed" }
if ( (Get-FileHash -Algorithm SHA256 $generated).Hash -ne
         (Get-FileHash -Algorithm SHA256 $generatedAgain).Hash ) {
    throw "XLIR generic dynamic-method generated C is not byte-for-byte deterministic"
}
$generatedText = [System.IO.File]::ReadAllText(
    $generated, [System.Text.Encoding]::UTF8)
foreach ( $token in @(
    "static xvalue* xlir_dynamic_method_dm0(",
    "static xvalue* xlir_dynamic_method_dm1(",
    "xlir_fn_4294967292(receiver)",
    "xlir_fn_4294967290(receiver, a0)",
    "dynamic method call has no matching verified overload"
) ) {
    if ( ! $generatedText.Contains($token) ) {
        throw "XLIR generic dynamic-method generated C lost its frozen adapter ABI: $token"
    }
}

Push-Location $Root
try {
    $xlirCommand = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $runtimeHost !XL6_CGEN_LINKS! -o $xlirExe && $xlirExe"
    & cmd.exe /d /v:on /c $xlirCommand
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR generic dynamic-method strict-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR generic dynamic-method ABI gate ok: complete registry preserved, unsupported same-name rows excluded from dispatch, specialized verified adapters emitted mechanically, deterministic IR/C, and strict C runtime."
exit 0
