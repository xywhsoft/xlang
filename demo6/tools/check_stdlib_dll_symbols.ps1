param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$stdlibPaths = @(
    (Join-Path $Root "src\stdlib\xl6_stdlib_data.c")
)
$cgenDir = Join-Path $Root "src\cgen"
$runtimeHeaderPath = Join-Path $Root "release\include\xl_runtime.h"
$commonPath = Join-Path $PSScriptRoot "xrt_snapshot_contract.ps1"
$minStdlibCsymbols = 161

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Read-CgenText($dir)
{
    $items = Get-ChildItem -Path $dir -Filter "*.c" -File -Recurse | Sort-Object FullName
    $parts = New-Object System.Collections.Generic.List[string]

    foreach ( $item in $items ) {
        $parts.Add((Read-Utf8Text $item.FullName))
    }
    return [string]::Join("`n", $parts)
}

function Print-Missing($title, $items)
{
    if ( $items.Count -eq 0 ) {
        return
    }

    Write-Host $title
    foreach ( $item in $items ) {
        Write-Host "  $item"
    }
}

$stdlibParts = New-Object System.Collections.Generic.List[string]
foreach ( $stdlibPath in $stdlibPaths ) {
    if ( Test-Path $stdlibPath ) {
        $stdlibParts.Add((Read-Utf8Text $stdlibPath))
    }
}
$stdlib = [string]::Join("`n", $stdlibParts)
$cgen = Read-CgenText $cgenDir

if ( ! (Test-Path -LiteralPath $runtimeHeaderPath -PathType Leaf) ) {
    throw "xlang runtime header missing: $runtimeHeaderPath"
}
if ( ! (Test-Path -LiteralPath $commonPath -PathType Leaf) ) {
    throw "XRT snapshot contract helper missing: $commonPath"
}

$runtimeHeader = Read-Utf8Text $runtimeHeaderPath
. $commonPath

$xrtExports = Read-XrtSnapshotRuntimeSymbols -Root $Root
Assert-XrtSnapshotRuntimeWiring -Root $Root

$crtSymbols = @(
    "strlen",
    "sin", "cos", "tan",
    "asin", "acos", "atan", "atan2",
    "sqrt", "pow", "exp", "log", "log10",
    "floor", "ceil", "fabs", "llabs"
)

$symbols = New-Object System.Collections.Generic.List[string]
$matches = [regex]::Matches($stdlib, '\{"[^"]+",\s*"[^"]+",\s*"[^"]+",\s*("[^"]+"|NULL)')

foreach ( $match in $matches ) {
    $raw = $match.Groups[1].Value
    if ( $raw -eq "NULL" ) {
        continue
    }
    $symbols.Add($raw.Trim('"'))
}

$uniqueSymbols = $symbols | Sort-Object -Unique
$failures = New-Object System.Collections.Generic.List[string]
$missingXrt = New-Object System.Collections.Generic.List[string]
$missingRuntime = New-Object System.Collections.Generic.List[string]
$unknown = New-Object System.Collections.Generic.List[string]
foreach ( $symbol in $uniqueSymbols ) {
    if ( $symbol.StartsWith("xrt") -or $symbol.StartsWith("xvo") ) {
        if ( ! $xrtExports.Contains($symbol) ) {
            $missingXrt.Add($symbol)
        }
    } elseif ( $symbol.StartsWith("xl6_xrt_") ) {
        if ( $cgen -notmatch [regex]::Escape($symbol) -and
             $runtimeHeader -notmatch [regex]::Escape($symbol) ) {
            $missingRuntime.Add($symbol)
        }
    } elseif ( $crtSymbols -notcontains $symbol ) {
        $unknown.Add($symbol)
    }
}

if ( $uniqueSymbols.Count -lt $minStdlibCsymbols ) {
    $failures.Add("standard library C symbol baseline regressed: $($uniqueSymbols.Count) < $minStdlibCsymbols") | Out-Null
}

if ( $failures.Count -ne 0 -or $missingXrt.Count -ne 0 -or $missingRuntime.Count -ne 0 -or $unknown.Count -ne 0 ) {
    Write-Host "Standard library DLL symbol check failed."
    Print-Missing "Export table failures:" $failures
    Print-Missing "Missing XRT symbols in xl_api_symbols.c:" $missingXrt
    Print-Missing "Missing generated or runtime-header helpers:" $missingRuntime
    Print-Missing "Unknown C symbols:" $unknown
    exit 1
}

Write-Host "Standard library DLL symbol check ok: $($uniqueSymbols.Count) C symbols, $($xrtExports.Count) runtime exports."
exit 0
