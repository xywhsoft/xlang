param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_async_void.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$loweredJson = Join-Path $outputDir "xlir_async_void.lowered.json"
$generatedC = Join-Path $outputDir "xlir_async_void.c"
$executable = Join-Path $outputDir "xlir_async_void.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR async-void gate input is missing: $path"
    }
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-void lowered IR failed" }
$ir = [System.IO.File]::ReadAllText($loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$voidTargets = @($ir.callTargets | Where-Object { $_.kind -eq 4 -and $_.asyncResultAbiName -eq "void" })
$voidAwaits = @($ir.instructions | Where-Object { $_.kindName -eq "await" -and $_.asyncResultAbiName -eq "void" })
if ( $voidTargets.Count -ne 3 -or
     @($voidTargets | Where-Object { $_.workerReturnType -ne 0 }).Count -ne 0 -or
     @($voidTargets | Where-Object { $_.asyncDispatch -ne 1 }).Count -ne 0 ) {
    throw "XLIR async-void CallTarget ABI is not explicit and payload-free"
}
if ( $voidAwaits.Count -ne 4 -or
     @($voidAwaits | Where-Object { $_.type -ne 0 -or $_.result -eq 0 }).Count -ne 0 ) {
    throw "XLIR async-void AWAIT does not use a payload-free ERROR_TEST control token"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-void C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "unsigned char reserved;",
    "result->Value = NULL; result->Destroy = NULL; result->DestroyData = NULL;",
    "static void xlir_future_await_void(xfuture* future)",
    "outcome.ok = worker.ok;",
    "outcome = xlir_async_frame_retire_",
    "if (!outcome.ok) {"
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR async-void generated C is missing: $required"
    }
}

$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_async_void.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_async_void.exe && build\tmp\xlir_async_void.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async-void strict-C/runtime gate failed" }

Write-Output "XLIR async-void result/frame/await ABI gate passed."
