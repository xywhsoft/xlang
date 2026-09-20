param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_class_explicit_constructor_base.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$generated = Join-Path $Root "build\test\xlir_class_explicit_constructor_base.gate.c"
$exe = Join-Path $Root "build\test\xlir_class_explicit_constructor_base.gate.exe"
foreach ($path in @($xl, $source, $hostSource)) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "XLIR explicit-class constructor gate input is missing: $path" }
}
Push-Location $Root
try {
    & $xl cgen $source --backend xlir --emit $generated
    if ($LASTEXITCODE -ne 0) { throw "XLIR explicit-class constructor Cgen failed" }
    $text = [System.IO.File]::ReadAllText($generated, [System.Text.Encoding]::UTF8)
    foreach ($required in @('$xlir.class.runtime-type', 'xrtValueObjectSetNew', 'static void xlir_fn_')) {
        if (!$text.Contains($required)) { throw "XLIR explicit-class constructor generated C lacks required ABI: $required" }
    }
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $hostSource !XL6_CGEN_LINKS! -o $exe && $exe"
    & cmd.exe /d /v:on /c $command
    if ($LASTEXITCODE -ne 0) { throw "XLIR explicit-class constructor generated-C runtime failed" }
} finally { Pop-Location }
Write-Output "XLIR explicit class constructor/default-base runtime ok."
