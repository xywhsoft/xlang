param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$stdlibPath = Join-Path $Root "src\stdlib\xl6_stdlib.c"
$docDir = Join-Path $Root "docs"
$testDir = Join-Path $Root "test\cases"
$exampleDir = Join-Path $Root "examples"

function Read-Utf8Text($path)
{
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Read-ManyUtf8Text($dir, $filter)
{
    $items = Get-ChildItem -Path $dir -Filter $filter -File | Sort-Object FullName
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
$docs = Read-ManyUtf8Text $docDir "*.md"
$tests = Read-ManyUtf8Text $testDir "stdlib_*.xl"
$examples = Read-ManyUtf8Text $exampleDir "stdlib_*.xl"

$funcs = New-Object System.Collections.Generic.List[string]
$matches = [regex]::Matches($stdlib, '\{"([^"]+)",\s*"([^"]+)",\s*"__xl_std_[^"]+"')

foreach ( $match in $matches ) {
    $ns = $match.Groups[1].Value
    $name = $match.Groups[2].Value

    if ( !$ns.StartsWith("__") ) {
        $funcs.Add("$ns.$name")
    }
}

$publicFuncs = $funcs | Sort-Object -Unique
$missingDocs = New-Object System.Collections.Generic.List[string]
$missingTests = New-Object System.Collections.Generic.List[string]
$missingExamples = New-Object System.Collections.Generic.List[string]

foreach ( $func in $publicFuncs ) {
    $pattern = [regex]::Escape($func) + '\('

    if ( $docs -notmatch $pattern ) {
        $missingDocs.Add($func)
    }
    if ( $tests -notmatch $pattern ) {
        $missingTests.Add($func)
    }
    if ( $examples -notmatch $pattern ) {
        $missingExamples.Add($func)
    }
}

if ( $missingDocs.Count -ne 0 -or $missingTests.Count -ne 0 -or $missingExamples.Count -ne 0 ) {
    Write-Host "Standard library coverage check failed."
    Print-Missing "Missing docs:" $missingDocs
    Print-Missing "Missing tests:" $missingTests
    Print-Missing "Missing examples:" $missingExamples
    exit 1
}

Write-Host "Standard library coverage ok: $($publicFuncs.Count) public functions."
exit 0
