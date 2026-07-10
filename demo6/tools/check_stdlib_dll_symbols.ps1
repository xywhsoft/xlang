param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$stdlibPaths = @(
    (Join-Path $Root "src\stdlib\xl6_stdlib.c"),
    (Join-Path $Root "src\stdlib\xl6_stdlib_data.c")
)
$apiPath = Join-Path $Root "src\api\xl_api.c"
$cgenDir = Join-Path $Root "src\cgen"
$minStdlibCsymbols = 161
$minRuntimeExports = 370

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
$api = Read-Utf8Text $apiPath
$cgen = Read-CgenText $cgenDir

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
$xrtExports = New-Object System.Collections.Generic.HashSet[string]
$xrtExportMatches = [regex]::Matches($api, '(?m)^\s*XL_ADD_XRT_SYMBOL\s*\(\s*s\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)\s*;')
$xrtExportInvocationCount = [regex]::Matches($api, '(?m)^\s*XL_ADD_XRT_SYMBOL\s*\(').Count

if ( $xrtExportMatches.Count -ne $xrtExportInvocationCount ) {
    $failures.Add("non-canonical XL_ADD_XRT_SYMBOL invocation in src/api/xl_api.c") | Out-Null
}

foreach ( $match in $xrtExportMatches ) {
    $name = $match.Groups[1].Value
    if ( ! $xrtExports.Add($name) ) {
        $failures.Add("duplicate XL_ADD_XRT_SYMBOL export in xl_api.c: $name") | Out-Null
    }
}

foreach ( $symbol in $uniqueSymbols ) {
    if ( $symbol.StartsWith("xrt") -or $symbol.StartsWith("xvo") ) {
        if ( ! $xrtExports.Contains($symbol) ) {
            $missingXrt.Add($symbol)
        }
    } elseif ( $symbol.StartsWith("xl6_xrt_") ) {
        if ( $cgen -notmatch [regex]::Escape($symbol) ) {
            $missingRuntime.Add($symbol)
        }
    } elseif ( $crtSymbols -notcontains $symbol ) {
        $unknown.Add($symbol)
    }
}

if ( $uniqueSymbols.Count -lt $minStdlibCsymbols ) {
    $failures.Add("standard library C symbol baseline regressed: $($uniqueSymbols.Count) < $minStdlibCsymbols") | Out-Null
}

if ( $xrtExports.Count -lt $minRuntimeExports ) {
    $failures.Add("runtime export baseline regressed: $($xrtExports.Count) < $minRuntimeExports") | Out-Null
}

if ( $failures.Count -ne 0 -or $missingXrt.Count -ne 0 -or $missingRuntime.Count -ne 0 -or $unknown.Count -ne 0 ) {
    Write-Host "Standard library DLL symbol check failed."
    Print-Missing "Export table failures:" $failures
    Print-Missing "Missing XRT symbols in xl_api.c:" $missingXrt
    Print-Missing "Missing generated runtime helpers in cgen:" $missingRuntime
    Print-Missing "Unknown C symbols:" $unknown
    exit 1
}

Write-Host "Standard library DLL symbol check ok: $($uniqueSymbols.Count) C symbols, $($xrtExports.Count) runtime exports."
exit 0
