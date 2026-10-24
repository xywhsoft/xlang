param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$work = Join-Path ([IO.Path]::GetTempPath()) ('xlang-zstd-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $old = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $lines = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $old
    }
    if ($code -ne 0) { throw "${name}: exit $code`n$($lines -join "`n")" }
    Write-Host "[zstd] ${name}: PASS"
    return ($lines -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Ilib/xrt', '-Ilib/xruntime', '-Irelease/extlibs/zstd/include')
    $links = @('-lws2_32', '-ladvapi32', '-liphlpapi', '-lm')
    $objects = @()
    foreach ($source in @('test/support/extlib_codec_runtime.c',
        'release/extlibs/zstd/xrt_zstd.c', 'release/extlibs/zstd/zstd_native.c')) {
        $object = Join-Path $work ('source-' + $objects.Count + '.o')
        $null = Run "GCC $source" $gcc ($flags + @('-c', $source, '-o', $object))
        $objects += $object
    }
    $vendor = Join-Path $work 'vendor.o'
    $null = Run 'GCC zstd vendor' $gcc @('-std=c11', '-O2', '-c',
        'release/extlibs/zstd/third_party/zstd.c', '-o', $vendor)
    $objects += $vendor

    foreach ($name in @('test_zstd', 'test_file', 'test_allocations')) {
        $source = "release/extlibs/zstd/test/$name.c"
        $exe = Join-Path $work "$name.exe"
        $null = Run "GCC build $name" $gcc ($flags + @($source) + $objects + $links + @('-o', $exe))
        $null = Run "GCC $name" $exe @()
    }
    $source = 'release/extlibs/zstd/examples/basic_c.c'
    $cExe = Join-Path $work 'basic-gcc.exe'
    $null = Run 'GCC example build' $gcc ($flags + @($source) + $objects + $links + @('-o', $cExe))
    $gccOutput = Run 'GCC example' $cExe @()
    $directOutput = Run 'TCC single-file example' $xl @('run', $source)
    $directExe = Join-Path $work 'basic-tcc.exe'
    $null = Run 'TCC single-file build' $xl @('build', $source, '-o', $directExe)
    $builtOutput = Run 'TCC single-file executable' $directExe @()
    $xlOutput = Run 'XLang example' $xl @('run', 'release/extlibs/zstd/examples/basic.xl')
    foreach ($case in @('test/cases/extlib_zstd_progress.xl',
        'release/extlibs/zstd/test/wire_fixture.xl',
        'release/extlibs/zstd/test/file_ops.xl',
        'release/extlibs/zstd/test/concat_boundary.xl')) {
        $null = Run "XLang $case" $xl @('run', $case)
    }
    if ($gccOutput -cne 'zstd round trip: hello zstd' -or
        $gccOutput -cne $directOutput -or $gccOutput -cne $builtOutput -or
        $gccOutput -cne $xlOutput) { throw 'C/XLang example output mismatch' }
    if (-not $SkipAot) {
        $aot = Join-Path $work 'basic-xl.exe'
        $null = Run 'XLang AOT build' $xl @('build', 'release/extlibs/zstd/examples/basic.xl', '-o', $aot)
        $aotOutput = Run 'XLang AOT example' $aot @()
        if ($aotOutput -cne $gccOutput) { throw 'XLang AOT output mismatch' }
    }
    Write-Host 'zstd Windows package gate: PASS'
} finally {
    Pop-Location
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "zstd temporary work path escaped system temp: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
