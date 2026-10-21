param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/css_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $ErrorActionPreference = 'Continue'
    $output = & $program @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[css] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $native = 'release/extlibs/css/css_native.c'
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'manifest C syntax' $gcc ($flags + @(
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_UNICODE', '-fsyntax-only', $native))
    $null = Run 'ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/css/test/abi_contract.c'))
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/css/test/runtime.c', '-o', $runtime))

    foreach ($name in @('test_css', 'test_css_threads')) {
        $target = Join-Path $work "$name.exe"
        $null = Run "$name compile" $gcc ($flags + @(
            $native, "release/extlibs/css/test/$name.c", $runtime) +
            $links + @('-o', $target))
        $null = Run $name $target @()
    }

    $example = Join-Path $work 'css_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/examples/extlibs/css_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directCOutput = Run 'C example direct xl run' $xl @('run',
        'release/examples/extlibs/css_c.c')
    if ($cOutput -cne $directCOutput) {
        throw 'direct CSS C example output differs'
    }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang existing test' $xl @('run', '../test/cases/extlib_css.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/css/test/boundaries.xl')
        $null = Run 'XLang allocations' $xl @('run', 'extlibs/css/test/allocations.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'examples/extlibs/css.xl')
        if ($cOutput -cne $xlOutput) {
            throw 'C/XLang CSS example outputs differ'
        }
        Write-Host '[css] C/XLang parity: PASS'

        if (-not $SkipAot) {
            $core = Join-Path $work 'css_core.exe'
            $null = Run 'XLang core build' $xl @('build',
                'extlibs/css/test/aot_core.xl', '-o', $core)
            $null = Run 'XLang core AOT' $core @()
            $target = Join-Path $work 'css_boundaries.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/css/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $exampleAot = Join-Path $work 'css_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'examples/extlibs/css.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT CSS example differs from C'
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
        throw "css temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
