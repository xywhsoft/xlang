param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_future_cancel.xl"
$hostSource = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$outputDir = Join-Path $Root "build\tmp"
$typedJson = Join-Path $outputDir "xlir_future_cancel.typed.json"
$loweredJson = Join-Path $outputDir "xlir_future_cancel.lowered.json"
$generatedC = Join-Path $outputDir "xlir_future_cancel.c"
$executable = Join-Path $outputDir "xlir_future_cancel.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR future-cancel gate input is missing: $path"
    }
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR future-cancel typed IR failed" }
$typed = [System.IO.File]::ReadAllText($typedJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$intrinsics = @($typed.intrinsicCalls | Where-Object {
    $_.kindName -eq "future-cancel" -and $_.admissionName -eq "verified"
})
if ( $intrinsics.Count -ne 4 -or
     @($intrinsics | Where-Object {
        $_.operandCount -ne 1 -or $_.resultAbiName -ne "value" -or
        $_.errorAbiName -ne "no-raise"
     }).Count -ne 0 ) {
    throw "XLIR future-cancel intrinsic identity/admission ABI is incomplete"
}

& $xl ir $source --lowered --format json --emit $loweredJson
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR future-cancel lowered IR failed" }
$lowered = [System.IO.File]::ReadAllText($loweredJson, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$operations = @($lowered.instructions | Where-Object { $_.runtimeOpName -eq "future-cancel" })
$temporaryReleases = @($lowered.instructions | Where-Object {
    $_.kindName -eq "release" -and $_.origin -ne 0
})
if ( $operations.Count -ne 4 -or
     @($operations | Where-Object { $_.operandCount -ne 1 -or $_.type -eq 0 }).Count -ne 0 -or
     $temporaryReleases.Count -lt 1 ) {
    throw "XLIR future-cancel runtime operation or temporary ownership ABI is incomplete"
}

& $xl cgen $source --backend xlir --emit $generatedC
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR future-cancel C generation failed" }
$c = [System.IO.File]::ReadAllText($generatedC, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "static bool xlir_future_cancel(xfuture* future)",
    "future != NULL && xrtFutureCancel(future)",
    " = xlir_future_cancel("
) ) {
    if ( ! $c.Contains($required) ) {
        throw "XLIR future-cancel generated C is missing: $required"
    }
}

$command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include build\tmp\xlir_future_cancel.c test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\tmp\xlir_future_cancel.exe && build\tmp\xlir_future_cancel.exe'
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR future-cancel strict-C/runtime gate failed" }

Write-Output "XLIR future-cancel intrinsic/runtime/temporary ownership gate passed for task.cancel(future), named future.cancel(), and temporary task.cancel(future) spellings."
