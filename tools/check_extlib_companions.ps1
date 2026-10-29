param([string]$Root = "")

$ErrorActionPreference = 'Stop'
if ($Root -eq '') {
    $Root = Split-Path -Parent $PSScriptRoot
}
$Root = (Resolve-Path -LiteralPath $Root).Path

$sourceDirs = @(
    (Join-Path $Root 'release/extlibs'),
    (Join-Path $Root 'release/examples/extlibs')
)
$collisions = New-Object System.Collections.Generic.List[string]
foreach ($sourceDir in $sourceDirs) {
    foreach ($xl in Get-ChildItem -LiteralPath $sourceDir -Recurse -File -Filter '*.xl') {
        $c = [IO.Path]::ChangeExtension($xl.FullName, '.c')
        if (-not (Test-Path -LiteralPath $c)) { continue }
        $source = [IO.File]::ReadAllText($c)
        if ($source -notmatch '(?m)^\s*(?:static\s+)?(?:int|void)\s+main\s*\(') { continue }
        $relative = $xl.FullName.Substring($Root.Length).TrimStart('\', '/')
        $collisions.Add($relative) | Out-Null
    }
}

if ($collisions.Count -gt 0) {
    foreach ($path in $collisions) {
        Write-Host "same-stem C main conflicts with XLang source: $path"
    }
    exit 1
}
Write-Host 'Extlib companion source names: PASS'
