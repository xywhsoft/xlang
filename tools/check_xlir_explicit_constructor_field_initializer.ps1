param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_explicit_constructor_field_initializer.xl"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$lowered = Join-Path $Root "build\test\xlir_explicit_constructor_field_initializer.lowered.xlir"
$generated = Join-Path $Root "build\test\xlir_explicit_constructor_field_initializer.gate.c"
$exe = Join-Path $Root "build\test\xlir_explicit_constructor_field_initializer.gate.exe"
foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR explicit-constructor field-initializer gate input is missing: $path"
    }
}

Push-Location $Root
try {
    & $xl ir $source --lowered --format text --emit $lowered
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR explicit-constructor field-initializer lowering failed"
    }
    $ir = [System.IO.File]::ReadAllText($lowered, [System.Text.Encoding]::UTF8)
    foreach ( $required in @(
        "name=declared init=explicit", "name=untouched init=scalar-zero",
        "name=assigned init=scalar-zero", "runtime=xvalue-object-set-literal-new",
        "call opcode=none runtime=none"
    ) ) {
        if ( ! $ir.Contains($required) ) {
            throw "XLIR explicit constructor lacks declaration-driven field initialization IR: $required"
        }
    }

    & $xl cgen $source --backend xlir --emit $generated
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR explicit-constructor field-initializer Cgen failed"
    }
    $c = [System.IO.File]::ReadAllText($generated, [System.Text.Encoding]::UTF8)
    $declared = $c.IndexOf('xrtStrView("declared")', [System.StringComparison]::Ordinal)
    $untouched = $c.IndexOf('xrtStrView("untouched")', [System.StringComparison]::Ordinal)
    $assigned = $c.IndexOf('xrtStrView("assigned")', [System.StringComparison]::Ordinal)
    $createCall = $c.IndexOf('xlir_fn_2(', [Math]::Max(0, $assigned), [System.StringComparison]::Ordinal)
    if ( $declared -lt 0 -or $untouched -le $declared -or $assigned -le $untouched -or
         $createCall -le $assigned ) {
        throw "XLIR generated C does not initialize explicit/scalar fields before the create body"
    }
    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include $generated $hostSource !XL6_CGEN_LINKS! -o $exe && $exe"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR explicit-constructor field-initializer generated-C runtime failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR explicit constructor field-initializer gate ok: inherited/default/own/create ordering, strict C, and runtime."
