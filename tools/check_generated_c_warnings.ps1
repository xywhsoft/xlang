param(
    [string]$Root = "",
    [string]$Compiler = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}
$Root = (Resolve-Path -LiteralPath $Root).Path

if ( $Compiler -eq "" ) {
    $Compiler = if ( $env:XL6_BOOTSTRAP_CC ) { $env:XL6_BOOTSTRAP_CC } else { "gcc" }
}

$compilerCommand = Get-Command $Compiler -ErrorAction SilentlyContinue
if ( $null -eq $compilerCommand ) {
    Write-Host "generated C warning check requires GCC in PATH: $Compiler"
    exit 1
}

$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
$runtimeDir = Join-Path $outputDir "__xl_runtime"
$runtimeHeader = Join-Path $Root "release\include\xl_runtime.h"
$extlibAbiHeader = Join-Path $Root "release\include\xl_extlib_abi.h"
$runtimeDeclarationHeader = Join-Path $Root "release\include\xruntime_decl.h"
if ( ! (Test-Path -LiteralPath $xl -PathType Leaf) ) {
    Write-Host "xl.exe not found: $xl"
    exit 1
}

New-Item -ItemType Directory -Path $runtimeDir -Force | Out-Null
Copy-Item -LiteralPath $runtimeHeader -Destination (Join-Path $runtimeDir "xl_runtime.h") -Force
Copy-Item -LiteralPath $extlibAbiHeader -Destination (Join-Path $runtimeDir "xl_extlib_abi.h") -Force
Copy-Item -LiteralPath $runtimeDeclarationHeader -Destination (Join-Path $runtimeDir "xruntime_decl.h") -Force

$cases = @(
    @{
        Name = "net_dns_trim"
        Source = "test\cases\stdlib_net_trim_dns_only.xl"
        RejectPreprocessed = '\bxl6Typed(Array|List|Set|Dict)'
    },
    @{ Name = "dynamic_to_typed_containers"; Source = "test\cases\cgen_dynamic_to_typed_containers.xl" },
    @{ Name = "try_finally_pending_control"; Source = "test\cases\cgen_try_finally_pending_control.xl" },
    @{ Name = "persistence_array_var_field"; Source = "test\cases\cgen_persistence_array_var_field.xl" },
    @{ Name = "dev_lisp"; Source = "release\projects\dev\lisp\lisp.xl" },
    @{ Name = "dev_mlplatform"; Source = "release\projects\dev\mlplatform\mlplatform.xl" }
)

Push-Location $Root
try {
    foreach ( $case in $cases ) {
        $source = Join-Path $Root $case.Source
        $output = Join-Path $outputDir ("cgen_warning_{0}.c" -f $case.Name)

        & $xl cgen $source --emit $output
        if ( $LASTEXITCODE -ne 0 ) {
            Write-Host "cgen failed for warning check: $($case.Source)"
            exit 1
        }

        & $compilerCommand.Source `
            -std=c11 `
            -Werror=implicit-function-declaration `
            -Werror=return-type `
            -Werror=incompatible-pointer-types `
            "-I$outputDir" `
            -fsyntax-only `
            $output
        if ( $LASTEXITCODE -ne 0 ) {
            Write-Host "generated C warning check failed: $($case.Source)"
            exit 1
        }

        if ( $case.ContainsKey("RejectPreprocessed") ) {
            $preprocessed = Join-Path $outputDir ("cgen_warning_{0}.i" -f $case.Name)
            & $compilerCommand.Source `
                -std=c11 `
                "-I$outputDir" `
                -E `
                $output `
                -o $preprocessed
            if ( $LASTEXITCODE -ne 0 ) {
                Write-Host "generated C preprocessing failed: $($case.Source)"
                exit 1
            }
            if ( Select-String -LiteralPath $preprocessed -Pattern $case.RejectPreprocessed -Quiet ) {
                Write-Host "generated C retained a disabled runtime family: $($case.Source)"
                exit 1
            }
        }
    }
} finally {
    Pop-Location
}

Write-Host "generated C warning check ok: $($cases.Count) representative units"
exit 0
