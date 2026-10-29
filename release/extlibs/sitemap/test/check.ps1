param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/sitemap/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$utf8 = [System.Text.UTF8Encoding]::new($false)
$workRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $workRoot ('sitemap_guard_' + [guid]::NewGuid().ToString('N'))

function Invoke-Checked([string] $name, [scriptblock] $action) {
    & $action
    if ($LASTEXITCODE -ne 0) { throw "$name failed ($LASTEXITCODE)" }
}

Push-Location $root
$cOutputPath = $null
$xlOutputPath = $null
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only '-Ilib/xrt' `
        '-Irelease/extlibs/sitemap/include' '-Irelease/extlibs/xml.sax/include' `
        '-Irelease/extlibs/_expat/vendor/expat/lib' @moduleFlags `
        release/extlibs/sitemap/sitemap_xml.c release/extlibs/sitemap/sitemap_robots.c
    if ($LASTEXITCODE -ne 0) { throw 'sitemap manifest macro compile failed' }

    Invoke-Checked 'sitemap C boundaries' { & $runner -Package sitemap -Source release/extlibs/sitemap/test/test_sitemap.c }
    foreach ($source in @(
        'test/cases/extlib_sitemap.xl',
        'release/extlibs/sitemap/test/boundaries.xl',
        'release/extlibs/sitemap/test/allocations.xl',
        'release/extlibs/sitemap/test/protocol_limits.xl'
    )) {
        Invoke-Checked "sitemap XLang test: $source" { & $xl run $source }
    }

    $cLines = @(& $runner -Package sitemap -Source release/extlibs/sitemap/examples/sitemap_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'sitemap C example failed' }
    $xlLines = @(& $xl run release/examples/extlibs/sitemap_pair.xl)
    if ($LASTEXITCODE -ne 0) { throw 'sitemap XLang example failed' }
    $cOutputPath = [IO.Path]::GetTempFileName()
    $xlOutputPath = [IO.Path]::GetTempFileName()
    [IO.File]::WriteAllText($cOutputPath, [string]::Join("`n", $cLines) + "`n", $utf8)
    [IO.File]::WriteAllText($xlOutputPath, [string]::Join("`n", $xlLines) + "`n", $utf8)
    & $python -I release/extlibs/sitemap/test/check_external.py $cOutputPath $xlOutputPath
    if ($LASTEXITCODE -ne 0) { throw 'sitemap independent XML check failed' }

    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory($work) | Out-Null
        $core = Join-Path $work 'sitemap_core.exe'
        Invoke-Checked 'sitemap core AOT build' {
            & $xl build release/extlibs/sitemap/test/core_aot.xl -o $core
        }
        $coreLines = @(& $core)
        if ($LASTEXITCODE -ne 0 -or $coreLines.Count -ne 0) {
            throw 'sitemap core AOT result differs'
        }
        Write-Output 'sitemap core AOT: PASS'
        $exampleAot = Join-Path $work 'sitemap_example.exe'
        Invoke-Checked 'sitemap example AOT build' {
            & $xl build release/examples/extlibs/sitemap_pair.xl -o $exampleAot
        }
        $aotLines = @(& $exampleAot)
        if ($LASTEXITCODE -ne 0) { throw 'sitemap example AOT failed' }
        [IO.File]::WriteAllText($xlOutputPath,
            [string]::Join("`n", $aotLines) + "`n", $utf8)
        Invoke-Checked 'sitemap AOT independent XML check' {
            & $python -I release/extlibs/sitemap/test/check_external.py $cOutputPath $xlOutputPath
        }
        Write-Output 'sitemap example AOT: PASS'
    }

    Write-Output 'sitemap Windows C/XLang/XML gate: PASS'
} finally {
    if ($cOutputPath) { Remove-Item -LiteralPath $cOutputPath -Force }
    if ($xlOutputPath) { Remove-Item -LiteralPath $xlOutputPath -Force }
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($workRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe sitemap work directory: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
    Pop-Location
}
