param(
    [int]$Iterations = 10000,
    [int]$Seed = 0x584C3601,
    [switch]$Sanitize
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build\test\fuzz"
$IsWindowsHost = $env:OS -eq "Windows_NT"
$Compiler = if ( $env:XL6_FUZZ_CC ) { $env:XL6_FUZZ_CC } else { "gcc" }
$OutputName = if ( $Sanitize ) { "xl6_frontend_fuzz_sanitize" } else { "xl6_frontend_fuzz" }
$OutputPath = Join-Path $BuildDir ($OutputName + $(if ( $IsWindowsHost ) { ".exe" } else { "" }))
$Sources = @(
    "test/fuzz/xl6_frontend_fuzz.c",
    "src/common/xl6_console.c",
    "src/common/xl6_output.c",
    "src/diag/xl6_diag.c",
    "src/ast/xl6_ast.c",
    "src/lexer/xl6_token.c",
    "src/lexer/xl6_lexer_api.c",
    "src/lexer/xl6_lexer_gen.c",
    "src/parser/xl6_parser.c",
    "src/parser/xl6_parser_api.c",
    "tools/xrt_single_header_impl.c"
)

if ( $Iterations -lt 1 ) {
    throw "Iterations must be positive"
}
if ( ! (Get-Command $Compiler -ErrorAction SilentlyContinue) ) {
    throw "frontend fuzz compiler not found: $Compiler"
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$Arguments = @("-std=c11", "-O1", "-g", "-I.")
if ( $Sanitize ) {
    $Arguments += @("-fsanitize=address,undefined", "-fno-omit-frame-pointer")
}
$Arguments += $Sources
if ( $IsWindowsHost ) {
    $Arguments += @("-lws2_32", "-liphlpapi", "-lshell32", "-lbcrypt", "-lcrypt32")
} else {
    $Arguments += @("-pthread", "-ldl", "-lm")
}
$Arguments += @("-o", $OutputPath)

Push-Location $Root
try {
    & $Compiler @Arguments
    if ( $LASTEXITCODE -ne 0 ) {
        throw "frontend fuzz build failed with code $LASTEXITCODE"
    }
    & $OutputPath $Iterations $Seed
    if ( $LASTEXITCODE -ne 0 ) {
        throw "frontend fuzz run failed with code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

Write-Host "Frontend fuzz guard ok: $Iterations iterations, sanitize=$($Sanitize.IsPresent)."
exit 0
