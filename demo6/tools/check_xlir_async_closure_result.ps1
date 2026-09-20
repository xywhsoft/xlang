param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_async_closure_result.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$loweredJson = Join-Path $outputDir "xlir_async_closure_result.lowered.json"
$generatedC = Join-Path $outputDir "xlir_async_closure_result.c"
$executable = Join-Path $outputDir "xlir_async_closure_result.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR async-closure gate input is missing: $path"
    }
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-closure lowered IR failed" }
$ir = [System.IO.File]::ReadAllText($loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$targets = @($ir.callTargets | Where-Object { $_.kind -eq 4 -and $_.asyncResultAbiName -eq "retained-closure" })
$awaits = @($ir.instructions | Where-Object { $_.kindName -eq "await" -and $_.asyncResultAbiName -eq "retained-closure" })
if ( $targets.Count -ne 3 -or
     @($targets | Where-Object { $_.workerReturnType -eq 0 }).Count -ne 0 -or
     @($targets | Where-Object { $_.asyncDispatch -notin @(1, 2) }).Count -ne 0 ) {
    throw "XLIR async-closure CallTarget ABI is not explicit"
}
if ( @($targets | Where-Object { $_.asyncDispatch -eq 1 }).Count -ne 2 -or
     @($targets | Where-Object { $_.asyncDispatch -eq 2 }).Count -ne 1 ) {
    throw "XLIR async-closure gate did not cover both task-pool and ready-Future dispatch"
}
if ( $awaits.Count -ne 5 -or
     @($awaits | Where-Object { $_.type -eq 0 -or $_.result -eq 0 }).Count -ne 0 ) {
    throw "XLIR async-closure AWAIT does not carry an explicit retained result ABI"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-closure C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static void xlir_async_closure_free(void* value, void* data)",
    "xlir_closure_release(*(xlir_closure**)value);",
    "static xlir_closure* xlir_future_await_closure(xfuture* future)",
    "xlir_closure_retain(*value)",
    "xrtPromiseResolveOwned(promise, value, xlir_async_closure_free, NULL)",
    "result->Destroy = xlir_async_closure_free;"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR async-closure generated C is missing: $required"
    }
}

$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_async_closure_result.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_closure_result.exe && build\tmp\xlir_async_closure_result.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-closure strict-C/runtime gate failed" }

Write-Output "XLIR async-closure result ownership ABI gate passed."
