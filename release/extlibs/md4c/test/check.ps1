param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('md4c_guard_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/include', '-Irelease/extlibs/md4c')
$native = @('release/extlibs/md4c/md4c_native.c',
    'release/extlibs/md4c/third_party/md4c/md4c.c',
    'release/extlibs/md4c/third_party/md4c/md4c-html.c',
    'release/extlibs/md4c/third_party/md4c/entity.c')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $output = & $program @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[md4c] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'manifest C syntax' $gcc ($flags + @(
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_FILE_WHOLE', '-DXRT_MODULE_UNICODE',
        '-fsyntax-only', 'release/extlibs/md4c/md4c_native.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $runtimeSource = Join-Path $root 'release/extlibs/md4c/test/xrt_runtime.c'
    $xrtHeader = Join-Path $root 'lib/xrt/xrt.h'
    if (-not (Test-Path -LiteralPath $runtime) -or
        (Get-Item -LiteralPath $runtime).LastWriteTimeUtc -lt
            (Get-Item -LiteralPath $runtimeSource).LastWriteTimeUtc -or
        (Get-Item -LiteralPath $runtime).LastWriteTimeUtc -lt
            (Get-Item -LiteralPath $xrtHeader).LastWriteTimeUtc) {
        $null = Run 'XRT compile' $gcc ($flags + @('-c',
            'release/extlibs/md4c/test/xrt_runtime.c', '-o', $runtime))
    }
    $test = Join-Path $work 'md4c_test.exe'
    $null = Run 'C test compile' $gcc ($flags + $native +
        @('release/extlibs/md4c/test/test_md4c.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @()
    $example = Join-Path $work 'md4c_example.exe'
    $null = Run 'C example compile' $gcc ($flags +
        @('release/extlibs/md4c/examples/md4c_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directCOutput = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/md4c/examples/md4c_c.c')
    if ($directCOutput -cne $cOutput) {
        throw 'direct C example output differs'
    }

    $null = Run 'XLang boundaries' $xl @('run', 'release/extlibs/md4c/test/boundaries.xl')
    $null = Run 'XLang allocation failures' $xl @('run', 'release/extlibs/md4c/test/allocations.xl')
    $xlOutput = Run 'XLang example' $xl @('run', 'release/extlibs/md4c/examples/md4c.xl')
    if ($cOutput -cne $xlOutput) {
        throw "C/XLang md4c example outputs differ:`nC:`n$cOutput`nXL:`n$xlOutput"
    }
    Write-Host '[md4c] C/XLang parity: PASS'
    $null = Run 'Public XLang example' $xl @('run', 'release/examples/extlibs/md4c.xl')
    if (-not $SkipAot) {
        $target = Join-Path $work 'md4c_xl_test.exe'
        $null = Run 'XLang boundaries build' $xl @('build',
            'release/extlibs/md4c/test/boundaries.xl', '-o', $target)
        $null = Run 'XLang boundaries AOT' $target @()
        $allocationsAot = Join-Path $work 'md4c_xl_allocations.exe'
        $null = Run 'XLang allocations build' $xl @('build',
            'release/extlibs/md4c/test/allocations.xl', '-o', $allocationsAot)
        $null = Run 'XLang allocations AOT' $allocationsAot @()
        $exampleAot = Join-Path $work 'md4c_xl_example.exe'
        $null = Run 'XLang example build' $xl @('build',
            'release/extlibs/md4c/examples/md4c.xl', '-o', $exampleAot)
        $aotOutput = Run 'XLang example AOT' $exampleAot @()
        if ($aotOutput -cne $cOutput) {
            throw 'XLang AOT md4c example differs from C'
        }
        $publicAot = Join-Path $work 'md4c_public_xl_example.exe'
        $null = Run 'Public XLang example build' $xl @('build',
            'release/examples/extlibs/md4c.xl', '-o', $publicAot)
        $null = Run 'Public XLang example AOT' $publicAot @()
        $cExampleAot = Join-Path $work 'md4c_c_example.exe'
        $null = Run 'C example build' $xl @('build',
            'release/extlibs/md4c/examples/md4c_c.c', '-o', $cExampleAot)
        $cAotOutput = Run 'C example AOT' $cExampleAot @()
        if ($cAotOutput -cne $cOutput) {
            throw 'C AOT md4c example differs from GCC'
        }
    }
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw 'md4c test directory escaped the build root'
        }
        Remove-Item -LiteralPath $work -Recurse -Force
    }
}
