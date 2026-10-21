param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('shellwords_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/shellwords')
$links = @('-lWs2_32', '-lAdvapi32', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $output = & $program @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[shellwords] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/shellwords/test/xrt_runtime.c', '-o', $runtime))
    $native = 'release/extlibs/shellwords/shellwords_native.c'
    $null = Run 'native strict compile' $gcc ($flags + @('-fsyntax-only', $native))

    $test = Join-Path $work 'shellwords_c_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/shellwords/test/test_shellwords.c', $native, $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @()

    $argvTest = Join-Path $work 'shellwords_windows_argv.exe'
    $null = Run 'Windows argv compile' $gcc ($flags + @('-municode',
        'release/extlibs/shellwords/test/windows_argv.c', $native, $runtime) +
        $links + @('-o', $argvTest))
    $null = Run 'Windows argv' $argvTest @()

    $example = Join-Path $work 'shellwords_c_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/shellwords/examples/basic.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directCOutput = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/shellwords/examples/basic.c')
    if ($directCOutput -cne $cOutput) {
        throw 'direct C example output differs'
    }
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/shellwords.xl')
    $goldenPath = Join-Path $root 'release/extlibs/shellwords/examples/basic.expected.txt'
    $golden = [IO.File]::ReadAllText($goldenPath).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $golden -or $xlOutput -cne $golden) {
        throw 'C/XLang example output differs from the common golden file'
    }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang existing test' $xl @('run', '../test/cases/extlib_shellwords.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/shellwords/test/boundaries.xl')
        $null = Run 'XLang allocation failures' $xl @('run', 'extlibs/shellwords/test/allocations.xl')
        if (-not $SkipAot) {
            $testAot = Join-Path $work 'shellwords_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/shellwords/test/boundaries.xl', '-o', $testAot)
            $null = Run 'XLang boundaries AOT' $testAot @()
            $exampleAot = Join-Path $work 'shellwords_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'examples/extlibs/shellwords.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $golden) { throw 'XLang AOT output differs from golden file' }
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
