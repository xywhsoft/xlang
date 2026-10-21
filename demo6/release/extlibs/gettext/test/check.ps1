param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$workRelative = 'build/test/gettext_guard_' + [guid]::NewGuid().ToString('N')
$work = Join-Path $root $workRelative
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/gettext')
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
    Write-Host "[gettext] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/gettext/test/xrt_runtime.c', '-o', $runtime))
    $native = @('release/extlibs/gettext/gettext_native.c',
        'release/extlibs/gettext/gettext_plural.c')
    $test = Join-Path $work 'gettext_test.exe'
    $null = Run 'C test compile' $gcc ($flags + $native +
        @('release/extlibs/gettext/test/test_gettext.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @($workRelative)

    $example = Join-Path $work 'gettext_example.exe'
    $null = Run 'C example compile' $gcc ($flags +
        @('release/extlibs/gettext/examples/gettext_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directC = Run 'C example xl run' $xl @('run',
        'release/extlibs/gettext/examples/gettext_c.c')
    if ($directC -cne $cOutput) {
        throw 'direct C gettext example differs from GCC output'
    }

    $null = Run 'XLang existing test' $xl @('run', 'test/cases/extlib_gettext.xl')
    $null = Run 'XLang boundaries' $xl @('run', 'release/extlibs/gettext/test/boundaries.xl')
    $null = Run 'XLang allocations' $xl @('run', 'release/extlibs/gettext/test/allocations.xl')
    $null = Run 'XLang input limits' $xl @('run', 'release/extlibs/gettext/test/input_limits.xl')
    if (-not $SkipAot) {
        $target = Join-Path $work 'gettext_xl_test.exe'
        $null = Run 'XLang boundaries build' $xl @('build',
            'release/extlibs/gettext/test/boundaries.xl', '-o', $target)
        $null = Run 'XLang boundaries AOT' $target @()
    }

    Push-Location (Join-Path $root 'release')
    try {
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/gettext/examples/gettext.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang gettext example outputs differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[gettext] C/XLang parity: PASS'
        $null = Run 'Public XLang example' $xl @('run', 'examples/extlibs/gettext.xl')

        if (-not $SkipAot) {
            $exampleAot = Join-Path $work 'gettext_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/gettext/examples/gettext.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT gettext example differs from C'
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
