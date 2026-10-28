param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('semver_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/semver')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[semver] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $null = Run 'C ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/semver/test/abi_contract.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/semver/test/xrt_runtime.c', '-o', $runtime))

    $native = 'release/extlibs/semver/semver_native.c'
    $test = Join-Path $work 'semver_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @($native,
        'release/extlibs/semver/test/test_semver.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @()

    $example = Join-Path $work 'semver_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/semver/examples/semver_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directCOutput = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/semver/examples/semver_c.c')
    if ($directCOutput -cne $cOutput) {
        throw 'direct C example output differs'
    }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang existing test' $xl @('run', '../test/cases/extlib_semver.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/semver/test/boundaries.xl')
        $null = Run 'XLang allocation failures' $xl @('run', 'extlibs/semver/test/allocations.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/semver/examples/basic.xl')
        $goldenPath = Join-Path $root 'release/extlibs/semver/examples/basic.expected.txt'
        $golden = [IO.File]::ReadAllText($goldenPath).Replace("`r`n", "`n").TrimEnd("`n")
        if ($cOutput -cne $golden -or $xlOutput -cne $golden) {
            throw 'C/XLang example output differs from the common golden file'
        }
        Write-Host '[semver] C/XLang golden: PASS'

        if (-not $SkipAot) {
            $centralAot = Join-Path $work 'semver_central.exe'
            $null = Run 'XLang central build' $xl @('build',
                '../test/cases/extlib_semver.xl', '-o', $centralAot)
            $null = Run 'XLang central AOT' $centralAot @()
            $target = Join-Path $work 'semver_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/semver/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $allocationsAot = Join-Path $work 'semver_allocations.exe'
            $null = Run 'XLang allocation failures build' $xl @('build',
                'extlibs/semver/test/allocations.xl', '-o', $allocationsAot)
            $null = Run 'XLang allocation failures AOT' $allocationsAot @()
            $exampleAot = Join-Path $work 'semver_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/semver/examples/basic.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $golden) {
                throw 'XLang AOT example differs from the common golden file'
            }
            $cExampleAot = Join-Path $work 'semver_c_example.exe'
            $null = Run 'C example build' $xl @('build',
                'extlibs/semver/examples/semver_c.c', '-o', $cExampleAot)
            $cAotOutput = Run 'C example AOT' $cExampleAot @()
            if ($cAotOutput -cne $golden) {
                throw 'C AOT example differs from the common golden file'
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
