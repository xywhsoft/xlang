param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$sources = @(
    (Join-Path $Root "test\cases\xlir_class_static_storage.xl")
    (Join-Path $Root "test\cases\xlir_class_static_storage_write.xl")
)
$staticHost = Join-Path $Root "test\cases\xlir_class_static_storage_host.c"
$generated = Join-Path $Root "build\test\xlir_class_static_storage.gate.c"
$exe = Join-Path $Root "build\test\xlir_class_static_storage.gate.exe"

foreach ( $path in @($xl, $staticHost) + $sources ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR static-class storage gate input is missing: $path"
    }
}

Push-Location $Root
try {
    foreach ( $source in $sources ) {
        & $xl cgen $source --backend xlir --emit $generated
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR static-class storage Cgen failed: $source"
        }
        $text = [System.IO.File]::ReadAllText($generated, [System.Text.Encoding]::UTF8)
        foreach ( $required in @('static char* s', 'static char* xlir_fn_', 'int xlang_main(xvalue* arg)') ) {
            if ( ! $text.Contains($required) ) {
                throw "XLIR static-class storage generated C lacks required ABI: $required"
            }
        }
        if ( $source.EndsWith("xlir_class_static_storage.xl") -and ! $text.Contains('xrtStrDup("default")') ) {
            throw "XLIR static-class storage generated C lacks canonical static initialization"
        }
        if ( $source.EndsWith("xlir_class_static_storage_write.xl") -and
             (! $text.Contains('char* xlir_released = s4294967295; s4294967295 = NULL; xrtFree(xlir_released)') -or ! $text.Contains('xrtStrDup("second")')) ) {
            throw "XLIR static-class storage generated C lacks managed replacement ABI"
        }
        if ( $text.Contains('RuntimeConfig.name') -or $text.Contains('sourceKind') ) {
            throw "XLIR static-class storage C retains source-level member recovery"
        }
        $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $staticHost !XL6_CGEN_LINKS! -o $exe && $exe"
        & cmd.exe /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR static-class storage generated-C runtime failed: $source"
        }
    }
} finally {
    Pop-Location
}

Write-Output "XLIR static class storage runtime ok."
