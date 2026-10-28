param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/statistics_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/statistics')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[statistics] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/statistics/test/xrt_runtime.c', '-o', $runtime))
    $native = 'release/extlibs/statistics/statistics_native.c'
    $manifest = Get-Content -LiteralPath 'release/extlibs/statistics/xlang.package.json' -Raw | ConvertFrom-Json
    $moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
    $null = Run 'manifest macro compile' $gcc ($flags + $moduleFlags +
        @('-fsyntax-only', $native))
    $null = Run 'ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/statistics/test/abi_contract.c'))
    $test = Join-Path $work 'statistics_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @($native,
        'release/extlibs/statistics/test/test_statistics.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @()

    $sizeGuard = Join-Path $work 'statistics_size_guard.exe'
    $null = Run 'size guard compile' $gcc ($flags + @(
        'release/extlibs/statistics/test/size_guard.c', $runtime) +
        $links + @('-o', $sizeGuard))
    $null = Run 'size guard' $sizeGuard @()

    $example = Join-Path $work 'statistics_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/statistics/examples/statistics_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()

    $trendExample = Join-Path $work 'statistics_trend.exe'
    $null = Run 'C trend compile' $gcc ($flags + @(
        'release/extlibs/statistics/examples/trend_c.c', $runtime) +
        $links + @('-o', $trendExample))
    $cTrend = Run 'C trend' $trendExample @()

    $distributionExample = Join-Path $work 'statistics_distribution.exe'
    $null = Run 'C distribution compile' $gcc ($flags + @(
        'release/extlibs/statistics/examples/distribution_c.c', $runtime) +
        $links + @('-o', $distributionExample))
    $cDistribution = Run 'C distribution' $distributionExample @()

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang existing test' $xl @('run', '../test/cases/extlib_statistics.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/statistics/test/boundaries.xl')
        $null = Run 'XLang allocation error' $xl @('run', 'extlibs/statistics/test/allocations.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/statistics/examples/basic.xl')
        $goldenPath = Join-Path $root 'release/extlibs/statistics/examples/basic.expected.txt'
        $golden = [IO.File]::ReadAllText($goldenPath).Replace("`r`n", "`n").TrimEnd("`n")
        if ($cOutput -cne $golden -or $xlOutput -cne $golden) {
            throw 'C/XLang example output differs from the common golden file'
        }
        $directC = Run 'C example xl run' $xl @('run', 'extlibs/statistics/examples/statistics_c.c')
        if ($directC -cne $golden) {
            throw 'direct C example differs from the common golden file'
        }
        Write-Host '[statistics] C/XLang golden: PASS'

        $xlTrend = Run 'XLang trend' $xl @('run', 'extlibs/statistics/examples/trend.xl')
        $trendPath = Join-Path $root 'release/extlibs/statistics/examples/trend.expected.txt'
        $trendGolden = [IO.File]::ReadAllText($trendPath).Replace("`r`n", "`n").TrimEnd("`n")
        if ($cTrend -cne $trendGolden -or $xlTrend -cne $trendGolden) {
            throw 'C/XLang trend output differs from the common golden file'
        }
        $directTrend = Run 'C trend xl run' $xl @('run', 'extlibs/statistics/examples/trend_c.c')
        if ($directTrend -cne $trendGolden) {
            throw 'direct C trend differs from the common golden file'
        }
        Write-Host '[statistics] C/XLang trend golden: PASS'

        $xlDistribution = Run 'XLang distribution' $xl @('run',
            'extlibs/statistics/examples/distribution.xl')
        $distributionPath = Join-Path $root 'release/extlibs/statistics/examples/distribution.expected.txt'
        $distributionGolden = [IO.File]::ReadAllText($distributionPath).Replace("`r`n", "`n").TrimEnd("`n")
        if ($cDistribution -cne $distributionGolden -or
            $xlDistribution -cne $distributionGolden) {
            throw 'C/XLang distribution output differs from the common golden file'
        }
        $directDistribution = Run 'C distribution xl run' $xl @('run',
            'extlibs/statistics/examples/distribution_c.c')
        if ($directDistribution -cne $distributionGolden) {
            throw 'direct C distribution differs from the common golden file'
        }
        Write-Host '[statistics] C/XLang distribution golden: PASS'

        if (-not $SkipAot) {
            $target = Join-Path $work 'statistics_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/statistics/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $exampleAot = Join-Path $work 'statistics_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/statistics/examples/basic.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $golden) {
                throw 'XLang AOT example differs from the common golden file'
            }
        }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "statistics temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
