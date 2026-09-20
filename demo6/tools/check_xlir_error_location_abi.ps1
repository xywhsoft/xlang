param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_error_location_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_program_entry_host.c"
$outputDir = Join-Path $Root "build\test"
$typedPath = Join-Path $outputDir "xlir_error_location_abi.typed.json"
$loweredPath = Join-Path $outputDir "xlir_error_location_abi.lowered.json"
$cPath = Join-Path $outputDir "xlir_error_location_abi.generated.c"
$exePath = Join-Path $outputDir "xlir_error_location_abi.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR error location ABI gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --typed --format json --emit $typedPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR error location Typed IR failed" }
$typed = [System.IO.File]::ReadAllText(
    $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$expectedMembers = @(
    "error-message-copy",
    "error-kind-copy",
    "error-file-copy",
    "error-line",
    "error-column",
    "error-code",
    "error-system-code",
    "error-domain-copy",
    "error-operation-copy",
    "error-data-copy",
    "error-cause-ref"
)
foreach ( $abi in $expectedMembers ) {
    if ( @($typed.nodes | Where-Object { $_.memberAbiName -eq $abi }).Count -lt 1 ) {
        throw "Typed XLIR omitted canonical Error member ABI: $abi"
    }
}

& $xl ir $source --lowered --format json --emit $loweredPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR error location lowering failed" }
$lowered = [System.IO.File]::ReadAllText(
    $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$raise = @($lowered.instructions | Where-Object { $_.kindName -eq "error-raise" })
if ( $raise.Count -ne 1 -or $raise[0].sourceLine -le 0 -or
     $raise[0].sourceColumn -le 0 ) {
    throw "Lowered XLIR Error raise has no canonical source position"
}
foreach ( $operation in $expectedMembers ) {
    if ( @($lowered.instructions | Where-Object {
             $_.runtimeOpName -eq $operation
         }).Count -lt 1 ) {
        throw "Lowered XLIR omitted explicit Error RuntimeOp: $operation"
    }
}

& $xl cgen $source --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR error location Cgen failed" }
$generated = [System.IO.File]::ReadAllText(
    $cPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "xrtErrorBuildAt(&desc, &location)",
    "xrtErrorFile(",
    "xrtErrorLine(",
    "xrtErrorColumn("
) ) {
    if ( ! $generated.Contains($required) ) {
        throw "XLIR Error C emission omitted mechanical XRT ABI: $required"
    }
}
if ( $generated -notmatch
     'xlir_error_create_at\("ValueError", v[0-9]+, [1-9][0-9]*, [1-9][0-9]*\)' ) {
    throw "XLIR Error C emission did not preserve the verified throw source position"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED=1 -include `"$cPath`" `"$hostSource`" !XL6_CGEN_LINKS! -o `"$exePath`" && `"$exePath`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR Error source-location strict-C/runtime gate failed"
}

Write-Output "XLIR Error ABI gate ok: canonical member identities, explicit RuntimeOps/source position, XRT immutable location, strict C, and runtime."
