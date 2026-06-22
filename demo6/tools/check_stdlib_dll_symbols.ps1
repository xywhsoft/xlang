param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$stdlibPath = Join-Path $Root "src\stdlib\xl6_stdlib.c"
$apiPath = Join-Path $Root "src\api\xl_api.c"
$cgenDir = Join-Path $Root "src\cgen"

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

$stdlib = Read-Utf8Text $stdlibPath
$api = Read-Utf8Text $apiPath
$cgen = Read-CgenText $cgenDir

$crtSymbols = @(
    "strlen",
    "sin", "cos", "tan",
    "asin", "acos", "atan", "atan2",
    "sqrt", "pow", "exp", "log", "log10",
    "floor", "ceil", "fabs"
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
$missingXrt = New-Object System.Collections.Generic.List[string]
$missingRuntime = New-Object System.Collections.Generic.List[string]
$unknown = New-Object System.Collections.Generic.List[string]

foreach ( $symbol in $uniqueSymbols ) {
    if ( $symbol.StartsWith("xrt") ) {
        $needle = "XL_ADD_XRT_SYMBOL(s, $symbol)"
        if ( $api -notmatch [regex]::Escape($needle) ) {
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

if ( $missingXrt.Count -ne 0 -or $missingRuntime.Count -ne 0 -or $unknown.Count -ne 0 ) {
    Write-Host "Standard library DLL symbol check failed."
    Print-Missing "Missing XRT symbols in xl_api.c:" $missingXrt
    Print-Missing "Missing generated runtime helpers in cgen:" $missingRuntime
    Print-Missing "Unknown C symbols:" $unknown
    exit 1
}

Write-Host "Standard library DLL symbol check ok: $($uniqueSymbols.Count) C symbols."
exit 0
