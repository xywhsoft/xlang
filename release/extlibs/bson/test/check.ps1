param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('bson_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/include', '-Irelease/extlibs/bson')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $output = & $program @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[bson] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $null = Run 'ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/bson/test/abi_contract.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/bson/test/xrt_runtime.c', '-o', $runtime))
    foreach ($case in @('test_bson_smoke', 'test_boundaries')) {
        $target = Join-Path $work "$case.exe"
        $null = Run "$case compile" $gcc ($flags + @(
            'release/extlibs/bson/bson_native.c',
            "release/extlibs/bson/test/$case.c", $runtime) +
            $links + @('-o', $target))
        $arguments = if ($case -eq 'test_boundaries') { @($work) } else { @() }
        $null = Run $case $target $arguments
    }
    $example = Join-Path $work 'bson_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/bson/examples/bson_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $expectedPath = Join-Path $root 'release/extlibs/bson/examples/basic.expected.txt'
    $expected = [IO.File]::ReadAllText($expectedPath).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $expected) { throw 'C BSON example differs from fixed expectation' }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/bson/test/boundaries.xl')
        $null = Run 'XLang integration' $xl @('run', 'extlibs/bson/test/bson_test.xl')
        $null = Run 'XLang allocation failures' $xl @('run', 'extlibs/bson/test/allocations.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/bson/examples/bson.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang BSON examples differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[bson] C/XLang parity: PASS'
        $runOutput = Run 'C example via xl run' $xl @('run', 'extlibs/bson/examples/bson_c.c')
        if ($runOutput -cne $cOutput) {
            throw 'xl run BSON C example differs from GCC'
        }
        $cFile = Join-Path $work 'c.bson'
        $xlFile = Join-Path $work 'xlang.bson'
        $cFileOutput = Run 'C save/load example' $example @($cFile)
        $xlFileOutput = Run 'XLang save/load example' $xl @('run',
            'extlibs/bson/examples/bson.xl', '--', $xlFile)
        $expectedFileOutput = "$expected`nfile=ok"
        if ($cFileOutput -cne $expectedFileOutput -or
            $xlFileOutput -cne $expectedFileOutput -or
            -not (Test-Path -LiteralPath $cFile -PathType Leaf) -or
            -not (Test-Path -LiteralPath $xlFile -PathType Leaf) -or
            (Get-FileHash -LiteralPath $cFile).Hash -ne
                (Get-FileHash -LiteralPath $xlFile).Hash) {
            throw 'C/XLang BSON save/load example differs from fixed output or file bytes'
        }
        $core = Join-Path $work 'bson_xl_core.exe'
        $null = Run 'XLang core build' $xl @('build',
            'extlibs/bson/test/aot_core.xl', '-o', $core)
        $null = Run 'XLang core AOT' $core @()
        if (-not $SkipAot) {
            $target = Join-Path $work 'bson_xl_boundaries.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/bson/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $exampleAot = Join-Path $work 'bson_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/bson/examples/bson.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT BSON example differs from C'
            }
        }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
}
