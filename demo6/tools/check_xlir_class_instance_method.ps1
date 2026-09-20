param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_class_instance_method.xl"
$classHost = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$generated = Join-Path $Root "build\test\xlir_class_instance_method.gate.c"
$exe = Join-Path $Root "build\test\xlir_class_instance_method.gate.exe"

foreach ( $path in @($xl, $source, $classHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR instance-class method gate input is missing: $path"
    }
}

Push-Location $Root
try {
    & $xl cgen $source --backend xlir --emit $generated
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR instance-class method Cgen failed"
    }
    $text = [System.IO.File]::ReadAllText($generated, [System.Text.Encoding]::UTF8)
    foreach ( $required in @('static void xlir_fn_', 'static int64_t xlir_fn_', 'xrtValueObjectSetNew', 'xrtValueObjectGet') ) {
        if ( ! $text.Contains($required) ) {
            throw "XLIR instance-class method generated C lacks required ABI: $required"
        }
    }
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $classHost !XL6_CGEN_LINKS! -o $exe && $exe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR instance-class method generated-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR instance xvalue class method runtime ok."
