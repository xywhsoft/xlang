param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('msgpack_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/msgpack')
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
    Write-Host "[msgpack] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $null = Run 'manifest C syntax' $gcc ($flags + @(
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_BUFFER_HEX',
        '-DXRT_MODULE_CODEC_HEX', '-DXRT_MODULE_STRING',
        '-DXRT_MODULE_UNICODE', '-DXRT_MODULE_VALUE_CONTAINER',
        '-fsyntax-only', 'release/extlibs/msgpack/msgpack_native.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/msgpack/test/xrt_runtime.c', '-o', $runtime))
    foreach ($case in @('test_msgpack', 'test_boundaries', 'test_allocations')) {
        $target = Join-Path $work "$case.exe"
        $null = Run "$case compile" $gcc ($flags + @(
            'release/extlibs/msgpack/msgpack_native.c',
            "release/extlibs/msgpack/test/$case.c", $runtime) +
            $links + @('-o', $target))
        $null = Run $case $target @()
    }
    $example = Join-Path $work 'msgpack_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/msgpack/examples/msgpack_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $expectedPath = Join-Path $root 'release/extlibs/msgpack/examples/basic.expected.txt'
    $expected = [IO.File]::ReadAllText($expectedPath).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $expected) {
        throw "C MessagePack example differs from fixed expectation:`n$cOutput"
    }
    $directCOutput = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/msgpack/examples/msgpack_c.c')
    if ($directCOutput -cne $cOutput) {
        throw 'direct C example output differs'
    }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/msgpack/test/boundaries.xl')
        $null = Run 'XLang allocation failures' $xl @('run', 'extlibs/msgpack/test/allocations.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/msgpack/examples/msgpack.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang MessagePack examples differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[msgpack] C/XLang parity: PASS'
        $null = Run 'Public XLang example' $xl @('run', 'examples/extlibs/msgpack.xl')
        if (-not $SkipAot) {
            foreach ($case in @('aot_core', 'boundaries')) {
                $target = Join-Path $work "msgpack_xl_$case.exe"
                $null = Run "XLang $case build" $xl @('build',
                    "extlibs/msgpack/test/$case.xl", '-o', $target)
                $null = Run "XLang $case AOT" $target @()
            }
            $exampleAot = Join-Path $work 'msgpack_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/msgpack/examples/msgpack.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT MessagePack example differs from C'
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
