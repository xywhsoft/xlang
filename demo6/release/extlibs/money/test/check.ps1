param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$expected = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/money/examples/money.expected.txt') -Encoding UTF8
$workRoot = Join-Path $root 'build/test/money_guard'
$work = Join-Path $workRoot ([guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)

Push-Location $root
try {
    & $runner -Package money -Source release/extlibs/money/test/test_money.c
    if ($LASTEXITCODE -ne 0) { throw 'money C arithmetic boundaries failed' }

    foreach ($source in @(
        'test/cases/extlib_money.xl',
        'test/cases/extlib_money_boundaries.xl',
        'release/extlibs/money/test/error_kinds.xl',
        'release/extlibs/money/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "money XLang test failed: $source" }
    }

    & (Join-Path $PSScriptRoot 'check_examples.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'money C/XLang example comparison failed' }
    $direct = & $xl run release/extlibs/money/examples/money.c
    if ($LASTEXITCODE -ne 0) { throw 'money direct C example failed' }
    if (($direct -join "`n") -cne ($expected -join "`n")) {
        throw 'money direct C example differs from fixed output'
    }

    if (-not $SkipAot) {
        $core = Join-Path $work 'money_core.exe'
        & $xl build release/extlibs/money/test/aot_core.xl -o $core
        if ($LASTEXITCODE -ne 0) { throw 'money core AOT build failed' }
        $coreLines = @(& $core)
        if ($LASTEXITCODE -ne 0 -or $coreLines.Count -ne 0) {
            throw 'money core AOT execution failed'
        }
        Write-Host '[money] core AOT: PASS'

        $full = Join-Path $work 'money_example.exe'
        & $xl build release/examples/extlibs/money.xl -o $full
        if ($LASTEXITCODE -ne 0) { throw 'money example AOT build failed' }
        $fullLines = @(& $full)
        if ($LASTEXITCODE -ne 0 -or
            ([string]::Join("`n", @($fullLines | ForEach-Object { $_ -replace "`r$", '' }))) -cne
            ([string]::Join("`n", $expected))) {
            throw 'money example AOT differs from fixed output'
        }
        Write-Host '[money] example AOT: PASS'
    }

    Write-Output 'money Windows C/XLang gate: PASS'
} finally {
    Pop-Location
    $safeRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $target = [IO.Path]::GetFullPath($work)
    if (-not $target.StartsWith($safeRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "unsafe money test work path: $target"
    }
    Remove-Item -LiteralPath $target -Recurse -Force
}
