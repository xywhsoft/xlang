$ErrorActionPreference = 'Stop'
$root = 'd:\GIT\x-lang\demo6\release\extlibs'
$out = @()
Get-ChildItem $root -Directory | Where-Object { $_.Name -notlike '_*' } | ForEach-Object {
    $pkgPath = Join-Path $_.FullName 'xlang.package.json'
    if (Test-Path $pkgPath) {
        $raw = [System.IO.File]::ReadAllText($pkgPath, [System.Text.Encoding]::UTF8)
        $pkg = $raw | ConvertFrom-Json
        $hasC = $false
        if ($pkg.artifact -and $pkg.artifact.cSources -and $pkg.artifact.cSources.Count -gt 0) { $hasC = $true }
        $depCount = 0
        if ($pkg.dependencies) { $depCount = $pkg.dependencies.Count }
        $out += [PSCustomObject]@{
            name = $pkg.name
            version = $pkg.version
            hasC = $hasC
            deps = $depCount
        }
    }
}
$json = $out | ConvertTo-Json -Depth 3
[System.IO.File]::WriteAllText('d:\GIT\x-lang\extlibs_meta.json', $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Total public libs: $($out.Count)"
Write-Host "Saved to d:\GIT\x-lang\extlibs_meta.json"
