param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_default_class_layout.xl"
$classHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$classSupport = Join-Path $Root "test\cases\xlir_default_class_layout_support.c"
$generated = Join-Path $Root "build\test\xlir_default_class_layout.gate.c"
$exe = Join-Path $Root "build\test\xlir_default_class_layout.gate.exe"
$artifactExe = Join-Path $Root "build\test\xlir_default_class_layout.artifact.exe"

foreach ( $path in @($xl, $source, $classHost, $classSupport) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR default-class layout gate input is missing: $path"
    }
}

Push-Location $Root
try {
    & $xl cgen $source --backend xlir --emit $generated
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR default-class layout Cgen failed"
    }
    $text = [System.IO.File]::ReadAllText($generated, [System.Text.Encoding]::UTF8)
    foreach ( $required in @('xl6ClassRuntimeV2Get()->Create',
            'xl6ClassRuntimeV2Get()->CommitTake', 'xrtValueObjectSetNew',
            'xrtStrView("x")', 'xrtStrView("z")') ) {
        if ( ! $text.Contains($required) ) {
            throw "XLIR default-class layout generated C lacks required object ABI: $required"
        }
    }
    # The generated module initializer is deliberately internal.  Include it
    # into the host translation unit so this runtime gate exercises the same
    # embedding boundary without promoting a Cgen-private symbol to an ABI.
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $classHost !XL6_CGEN_XLANG_LINKS! -o $exe && $exe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR default-class layout generated-C runtime failed"
    }
    & $xl build $source $classSupport --backend xlir -o $artifactExe
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR default-class layout standalone artifact build failed"
    }
    & $artifactExe
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR default-class layout standalone artifact runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR default xvalue class layout runtime ok."
