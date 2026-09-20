param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$runtimePath = Join-Path $Root "release\include\xl_runtime.h"
$cgenPath = Join-Path $Root "src\cgen"
$paths = @($runtimePath) + @(
    Get-ChildItem -LiteralPath $cgenPath -File -Filter "*.c" |
        Select-Object -ExpandProperty FullName
)
$obsolete = @(
    "XRT_OBJMODE_",
    "XL6_OBJMODE_",
    "xl6_xrt_move_to_shared",
    "xl6TypedArrayShare",
    "xl6TypedListShare",
    "xl6TypedSetShare",
    "xl6TypedDictShare",
    "_copy_mode("
)
$runtime = [System.IO.File]::ReadAllText($runtimePath, [System.Text.Encoding]::UTF8)
$failures = New-Object System.Collections.Generic.List[string]

foreach ( $pattern in $obsolete ) {
    $matches = @(Select-String -LiteralPath $paths -SimpleMatch $pattern)
    foreach ( $match in $matches ) {
        $failures.Add(
            "obsolete ownership API: $($match.Path):$($match.LineNumber): $($match.Line.Trim())"
        ) | Out-Null
    }
}

foreach ( $kind in @("Array", "List", "Set", "Dict") ) {
    foreach ( $operation in @("Retain", "Clone")) {
        $signature = "static inline xt$($kind.ToLowerInvariant()) xl6Typed${kind}${operation}("
        if ( !$runtime.Contains($signature) ) {
            $failures.Add("missing typed container operation: xl6Typed${kind}${operation}") | Out-Null
        }
    }

    $copyName = "xl6_xrt_typed_$($kind.ToLowerInvariant())_copy("
    if ( !$runtime.Contains($copyName) ) {
        $failures.Add("missing typed container copy helper: $copyName") | Out-Null
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures | Select-Object -First 30 ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "XRT ownership contract ok: retain, move and explicit clone/copy only."
exit 0
