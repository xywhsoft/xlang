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
    Write-Error "XRT header ABI check requires GCC in PATH: $Compiler"
    exit 1
}

$include = Join-Path $Root "release\include"
$xrtInclude = Join-Path $Root "lib\xrt"
$source = @'
#define XL6_C_API
#define XL_RUNTIME_NEED_LIFECYCLE 1
#define XL_RUNTIME_NEED_STRING 1
#define XL_RUNTIME_NEED_XVALUE 1
#define XL_RUNTIME_NEED_PRINT 1
#define XL_RUNTIME_NEED_ARGV 1
#define XL_RUNTIME_NEED_POINT_MOVE 1
#include "xl_runtime.h"

_Static_assert(_Generic((str)0, char*: 1, default: 0), "str ABI drift");
_Static_assert(sizeof(xl6_xrt_datetime) == sizeof(xdatetime), "datetime ABI drift");
_Static_assert(sizeof(xl6_xrt_rng) == sizeof(xrng), "RNG ABI drift");
_Static_assert(sizeof(xatomicptr) == sizeof(void*), "atomic pointer ABI drift");

int main(void)
{
    xrtcallresult result;
    xl6_call_result bridge;
    return (int)(sizeof(result) == sizeof(bridge));
}
'@

$cases = @(
    $source,
    "#define XRT_MODULE_ALL`n#include <xrt.h>`n$source",
    "$source`n#include <xrt.h>`n"
)

foreach ( $case in $cases ) {
    $case | & $compilerCommand.Source `
        -x c `
        -std=c11 `
        -Wall `
        -Wextra `
        "-I$include" `
        "-I$xrtInclude" `
        -fsyntax-only `
        -
    if ( $LASTEXITCODE -ne 0 ) {
        Write-Error "XRT and xlang header ABI coexistence check failed"
        exit $LASTEXITCODE
    }
}

Write-Host "XRT and xlang header ABI coexistence ok: 3 include orders"
exit 0
