param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$packageDir = Join-Path $Root "release\extlibs"
$commonPath = Join-Path $PSScriptRoot "xrt_snapshot_contract.ps1"
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

if ( ! (Test-Path -LiteralPath $packageDir -PathType Container) ) {
    Write-Host "release extlibs directory missing: $packageDir"
    exit 1
}
if ( ! (Test-Path -LiteralPath $commonPath -PathType Leaf) ) {
    Write-Host "XRT snapshot contract helper missing: $commonPath"
    exit 1
}

. $commonPath

$publicXrt = Read-XrtSnapshotRuntimeSymbols -Root $Root
Assert-XrtSnapshotRuntimeWiring -Root $Root
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
        if ( ! $publicXrt.Contains($symbol) ) {
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

Write-Host "Package XRT symbol boundary ok: $($files.Count) package C/H files, $($usedSymbols.Count) public XRT symbols used, $($publicXrt.Count) runtime exports."
exit 0
