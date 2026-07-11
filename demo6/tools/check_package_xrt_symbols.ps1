param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$packageDir = Join-Path $Root "release\extlibs"
$xrtPath = Join-Path $Root "lib\xrt\xrt.h"
$apiPath = Join-Path $Root "src\api\xl_api_symbols.c"
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function New-StringSet()
{
    return ,[System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
}

function Strip-CText($text)
{
    $options = [System.Text.RegularExpressions.RegexOptions]::Singleline
    $text = [regex]::Replace($text, '/\*.*?\*/', '', $options)
    $text = [regex]::Replace($text, '//[^\r\n]*', '')
    $text = [regex]::Replace($text, '"(?:\\.|[^"\\])*"', '""')
    $text = [regex]::Replace($text, "'(?:\\.|[^'\\])*'", "''")
    return $text
}

function Read-PublicXrtSymbols($path)
{
    $symbols = New-StringSet
    $text = Read-Utf8Text $path
    $matches = [regex]::Matches($text, 'XXAPI\s+[^;{]+?\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')

    foreach ( $match in $matches ) {
        $symbols.Add($match.Groups[1].Value) | Out-Null
    }
    return ,$symbols
}

function Read-InjectedXrtSymbols($path)
{
    $symbols = New-StringSet
    $text = Read-Utf8Text $path
    $matches = [regex]::Matches($text, '(?m)^\s*XL_ADD_XRT_SYMBOL\s*\(\s*s\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)\s*;')
    $invocations = [regex]::Matches($text, '(?m)^\s*XL_ADD_XRT_SYMBOL\s*\(').Count

    if ( $matches.Count -ne $invocations ) {
        Add-Failure "non-canonical XL_ADD_XRT_SYMBOL invocation in src/api/xl_api_symbols.c"
    }

    foreach ( $match in $matches ) {
        $name = $match.Groups[1].Value
        if ( ! $symbols.Add($name) ) {
            Add-Failure "duplicate XRT runtime symbol injection: $name"
        }
    }
    return ,$symbols
}

if ( ! (Test-Path -LiteralPath $packageDir -PathType Container) ) {
    Write-Host "release extlibs directory missing: $packageDir"
    exit 1
}
if ( ! (Test-Path -LiteralPath $xrtPath -PathType Leaf) ) {
    Write-Host "XRT single header missing: $xrtPath"
    exit 1
}
if ( ! (Test-Path -LiteralPath $apiPath -PathType Leaf) ) {
    Write-Host "xl API source missing: $apiPath"
    exit 1
}

$publicXrt = Read-PublicXrtSymbols $xrtPath
$injectedXrt = Read-InjectedXrtSymbols $apiPath
$usedSymbols = New-StringSet
$files = @(Get-ChildItem -LiteralPath $packageDir -Recurse -File -Include "*.c", "*.h" | Sort-Object FullName)

foreach ( $file in $files ) {
    $text = Strip-CText (Read-Utf8Text $file.FullName)
    $matches = [regex]::Matches($text, '\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')
    $fileMissing = New-StringSet

    foreach ( $match in $matches ) {
        $symbol = $match.Groups[1].Value
        if ( ! $publicXrt.Contains($symbol) ) {
            continue
        }
        $usedSymbols.Add($symbol) | Out-Null
        if ( ! $injectedXrt.Contains($symbol) ) {
            $fileMissing.Add($symbol) | Out-Null
        }
    }

    foreach ( $symbol in ($fileMissing | Sort-Object) ) {
        $relative = $file.FullName.Substring((Resolve-Path -LiteralPath $Root).Path.Length + 1).Replace("\", "/")
        Add-Failure "package C source uses public XRT symbol not injected into xl runtime: $relative -> $symbol"
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

Write-Host "Package XRT symbol boundary ok: $($files.Count) package C/H files, $($usedSymbols.Count) public XRT symbols used, $($injectedXrt.Count) runtime exports."
exit 0
