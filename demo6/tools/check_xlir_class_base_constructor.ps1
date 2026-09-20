param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$hostSource = Join-Path $Root "test\cases\xlir_default_class_layout_host.c"
$outputDir = Join-Path $Root "build\test"
$fixtures = @(
    @{ Name = "explicit and inherited default base constructor"; Source = "test\cases\cgen_base_create.xl"; ExplicitBase = $true },
    @{ Name = "overloaded inherited default base constructor"; Source = "test\cases\cgen_base_create_overloads.xl"; ExplicitBase = $false }
)

foreach ( $path in @($xl, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR base-constructor ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

Push-Location $Root
try {
    foreach ( $fixture in $fixtures ) {
        $source = Join-Path $Root $fixture.Source
        if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
            throw "XLIR base-constructor source is missing: $source"
        }
        $stem = [System.IO.Path]::GetFileNameWithoutExtension($source)
        $loweredPath = Join-Path $outputDir "$stem.lowered.xlir"
        $cPath = Join-Path $outputDir "$stem.generated.c"
        $exePath = Join-Path $outputDir "$stem.exe"

        & $xl ir $source --lowered --format text --emit $loweredPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR base-constructor lowering failed for $($fixture.Name)"
        }
        $lowered = [System.IO.File]::ReadAllText($loweredPath, [System.Text.Encoding]::UTF8)
        if ( $fixture.ExplicitBase -and ! $lowered.Contains("call=base-constructor") ) {
            throw "XLIR typed snapshot omitted the explicit base-constructor CallSite"
        }
        foreach ( $required in @(
            "runtime=xvalue-retain",
            " call opcode=none runtime=none ",
            " release opcode=none runtime=none "
        ) ) {
            if ( ! $lowered.Contains($required) ) {
                throw "XLIR base-constructor lowering omitted the canonical receiver/call ownership sequence: $required"
            }
        }
        if ( ([regex]::Matches($lowered, " call opcode=none runtime=none ")).Count -lt 2 ) {
            throw "XLIR base-constructor lowering did not preserve both the selected create call and fixture assertion calls"
        }

        & $xl cgen $source --backend xlir --emit $cPath
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR base-constructor Cgen failed for $($fixture.Name)"
        }
        $generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
        foreach ( $required in @("xrtValueRetain(", "xrtValueRelease(", "expect_i64(")) {
            if ( ! $generated.Contains($required) ) {
                throw "XLIR base-constructor C emission is incomplete: $required"
            }
        }

        $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
        & $env:ComSpec /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) {
            throw "XLIR base-constructor strict-C/runtime gate failed for $($fixture.Name)"
        }
    }
} finally {
    Pop-Location
}

Write-Output "XLIR base-constructor ABI gate ok: explicit base calls, inherited zero-argument create selection, receiver upcast/borrow ownership, overload resolution, strict C, and runtime."
exit 0
