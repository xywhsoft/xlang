param([switch]$SkipAot, [switch]$RealServer)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('com_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
[IO.Directory]::CreateDirectory($work) | Out-Null
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/com/include')
$links = @('-lole32', '-loleaut32', '-lws2_32', '-ladvapi32', '-liphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[com] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'public header' $gcc ($flags + @('-fsyntax-only', '-x', 'c',
        'release/extlibs/com/include/com.h'))
    $null = Run 'public header with XRT value' $gcc ($flags + @(
        '-DXRT_MODULE_VALUE_CONTAINER', '-fsyntax-only', '-x', 'c',
        'release/extlibs/com/include/com.h'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $runtimeSource = 'release/extlibs/com/test/xrt_runtime.c'
    $null = Run 'XRT compile' $gcc ($flags + @('-c', $runtimeSource, '-o', $runtime))
    $test = Join-Path $work 'test.exe'
    $null = Run 'C test compile' $gcc ($flags + @('release/extlibs/com/x_com_artifact.c',
        'release/extlibs/com/test/test_com.c', $runtime) + $links + @('-o', $test))
    $null = Run 'C test' $test @()
    $example = Join-Path $work 'example.exe'
    $null = Run 'C example compile' $gcc ($flags + @('release/extlibs/com/x_com_artifact.c',
        'release/extlibs/com/test/com_test_fixture.c', 'release/extlibs/com/examples/basic_c.c',
        $runtime) + $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $realC = ''
    if ($RealServer) {
        $real = Join-Path $work 'real_server.exe'
        $null = Run 'Real server C compile' $gcc ($flags + @('release/extlibs/com/x_com_artifact.c',
            'release/extlibs/com/test/real_server_c.c', $runtime) + $links + @('-o', $real))
        $realC = Run 'Real server C' $real @()
    }

    Push-Location (Join-Path $root 'release')
    try {
        $cleanupOutput = Run 'Implicit COM cleanup' $xl @('run',
            'extlibs/com/test/implicit_object_cleanup.xl')
        if ($cleanupOutput -cne 'com implicit cleanup: PASS') {
            throw "Implicit COM cleanup output differs: $cleanupOutput"
        }
        if (-not $SkipAot) {
            $cleanupExe = Join-Path $work 'implicit_object_cleanup.exe'
            $null = Run 'Implicit COM cleanup AOT build' $xl @('build',
                'extlibs/com/test/implicit_object_cleanup.xl', '-o', $cleanupExe)
            if ((Run 'Implicit COM cleanup AOT' $cleanupExe @()) -cne $cleanupOutput) {
                throw 'Implicit COM cleanup AOT output differs from JIT'
            }
        }
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/com/test/boundaries.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/com/examples/basic.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang output differs:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[com] C/XLang parity: PASS'
        $null = Run 'Public example' $xl @('run', 'examples/extlibs/com.xl')
        if ($RealServer) {
            $realXl = Run 'Real server XLang' $xl @('run', 'extlibs/com/test/real_server.xl')
            if ($realC -cne $realXl) {
                throw "Real server C/XLang output differs:`nC:`n$realC`nXL:`n$realXl"
            }
            Write-Host '[com] Real server parity: PASS'
            $null = Run 'Real server object XLang' $xl @('run', 'extlibs/com/test/real_server_object.xl')
        }
        if (-not $SkipAot) {
            $core = Join-Path $work 'aot_core.exe'
            $null = Run 'XLang core AOT build' $xl @('build', 'extlibs/com/test/aot_core.xl', '-o', $core)
            $null = Run 'XLang core AOT' $core @()
            $target = Join-Path $work 'boundaries_aot.exe'
            $null = Run 'XLang AOT build' $xl @('build', 'extlibs/com/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang AOT' $target @()
        }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('com_guard_')) {
        throw "COM test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
