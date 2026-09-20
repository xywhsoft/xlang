param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
$repeatDir = Join-Path $Root "build\tmp"
$fixtures = @(
    @{ Name = "xlir_closure_capture"; Source = "test/cases/xlir_closure_capture.xl"; Host = "test/cases/xlir_closure_capture_host.c" },
    @{ Name = "xlir_closure_nested_capture"; Source = "test/cases/xlir_closure_nested_capture.xl"; Host = "test/cases/xlir_closure_nested_capture_host.c" },
    @{ Name = "xlir_closure_return"; Source = "test/cases/xlir_closure_return.xl"; Host = "test/cases/xlir_closure_return_host.c" },
    @{ Name = "xlir_function_value"; Source = "test/cases/xlir_function_value.xl"; Host = "test/cases/xlir_function_value_host.c" }
)

if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    throw "XLIR closure runtime gate requires: $xl"
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null
[System.IO.Directory]::CreateDirectory($repeatDir) | Out-Null

Push-Location $Root
try {
    foreach ( $fixture in $fixtures ) {
        $source = Join-Path $Root $fixture.Source
        $hostPath = Join-Path $Root $fixture.Host
        $generated = Join-Path $outputDir "$($fixture.Name)_generated.c"
        $repeat = Join-Path $repeatDir "$($fixture.Name)_repeat.c"
        $executable = Join-Path $outputDir "$($fixture.Name)_host.exe"
        foreach ( $path in @($source, $hostPath) ) {
            if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
                throw "XLIR closure runtime gate input is missing: $path"
            }
        }

        & $xl cgen $source --backend xlir --emit $generated
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR closure C generation failed: $($fixture.Name)"
        }
        & $xl cgen $source --backend xlir --emit $repeat
        if ( $LASTEXITCODE -ne 0 -or
             ! [System.Linq.Enumerable]::SequenceEqual(
                 [byte[]][System.IO.File]::ReadAllBytes($generated),
                 [byte[]][System.IO.File]::ReadAllBytes($repeat)) ) {
            throw "XLIR closure C is not byte-for-byte deterministic: $($fixture.Name)"
        }

        $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror ' +
            $fixture.Host.Replace('/', '\') + ' !XL6_CGEN_LINKS! -o ' +
            $executable.Substring($Root.Length + 1) + ' && ' +
            $executable.Substring($Root.Length + 1)
        & $env:ComSpec /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR closure strict-C/runtime gate failed: $($fixture.Name)"
        }
    }
} finally {
    Pop-Location
}

Write-Host "XLIR basic closure runtime gate passed: capture, nested capture, returned closure and named function value; deterministic C and strict runtime."
