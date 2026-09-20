param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$compiler = Join-Path $Root "release\xl.exe"
$fixture = Join-Path $Root "test\cases\xlir_async_storage_address.xl"
$hostSource = Join-Path $Root "test\cases\xlir_module_init_host.c"
$outputDir = Join-Path $Root "build\test"
$jsonPath = Join-Path $outputDir "xlir_async_storage_address.lowered.json"
$cPath = Join-Path $outputDir "xlir_async_storage_address.generated.c"
$executable = Join-Path $outputDir "xlir_async_storage_address.exe"

foreach ( $path in @($compiler, $fixture, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR async storage-address gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $compiler ir $fixture --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async storage-address lowering failed" }
$ir = [IO.File]::ReadAllText($jsonPath, [Text.Encoding]::UTF8) |
    ConvertFrom-Json
$addresses = @($ir.instructions | Where-Object {
    $_.kindName -eq 'storage-address'
})
if ( $ir.asyncMachineCount -ne 1 -or $addresses.Count -ne 1 ) {
    throw "Lowered XLIR has no unique async STORAGE_ADDRESS operation"
}
$address = $addresses[0]
$containingBlocks = @($ir.asyncMachineBlocks | Where-Object {
    $first = [int64]$_.firstInstruction
    $last = $first + [int64]$_.instructionCount
    [int64]$address.index -ge $first -and [int64]$address.index -lt $last
})
if ( $containingBlocks.Count -ne 1 -or
     $address.operandCount -ne 0 -or $address.result -le 0 -or
     @($ir.types | Where-Object {
         $_.id -eq $address.type -and $_.kind -eq 15 -and
         $_.cAbi -eq 39 -and @($_.params).Count -eq 1
     }).Count -ne 1 ) {
    throw "Async STORAGE_ADDRESS has no exact machine block or storage-ref TypeId ABI"
}

& $compiler cgen $fixture --backend xlir --emit $cPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR async storage-address Cgen failed" }
$generated = [IO.File]::ReadAllText($cPath, [Text.Encoding]::UTF8)
if ( ! $generated.Contains('(void*)&s') -or
     $generated.Contains('recvInto') ) {
    throw "XLIR async Cgen did not mechanically address its admitted machine storage"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include `"$cPath`" -DXLIR_GENERATED_MODULE_PREINCLUDED=1 `"$hostSource`" !XL6_CGEN_LINKS! -o `"$executable`" && `"$executable`""
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) {
    throw "XLIR async storage-address strict-C/runtime gate failed"
}

Write-Output "XLIR async storage-address gate ok: exact storage-ref instruction, machine-local residence, strict C, and runtime."
exit 0
