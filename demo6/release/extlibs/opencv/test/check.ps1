param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('opencv_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $release 'xl.exe'
[void][IO.Directory]::CreateDirectory($work)

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    if ($code -ne 0) { throw "${label}: exit $code`n$($output -join "`n")" }
    Write-Host "[opencv] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-DXRT_MODULE_ONCE', '-DXRT_MODULE_CHARSET', '-DXRT_MODULE_PATH_SYSTEM',
    '-DXRT_MODULE_ENVIRONMENT', '-DXRT_MODULE_STRING', '-I../lib/xrt')
$links = @('-lws2_32', '-ladvapi32', '-liphlpapi', '-lm')

Push-Location $release
try {
    $wrapper = Join-Path $work 'opencv_artifact.o'
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'strict wrapper build' $gcc ($flags + @('-c', 'extlibs/opencv/opencv_artifact.c', '-o', $wrapper))
    $null = Run 'XRT runtime build' $gcc ($flags + @('-c', 'extlibs/opencv/test/runtime.c', '-o', $runtime))

    $example = Join-Path $work 'image_info.exe'
    $null = Run 'C example build' $gcc ($flags + @('extlibs/opencv/examples/image_info_c.c',
        $wrapper, $runtime) + $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directOutput = Run 'C example direct JIT' $xl @('run', 'extlibs/opencv/examples/image_info_c.c')
    $directExe = Join-Path $work 'image_info_direct.exe'
    $null = Run 'C example direct build' $xl @('build', 'extlibs/opencv/examples/image_info_c.c', '-o', $directExe)
    $directAot = Run 'C example direct executable' $directExe @()
    $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/opencv/examples/image_info.xl')
    if ($cOutput -cne $xlOutput -or $cOutput -cne $directOutput -or $cOutput -cne $directAot) {
        throw 'C/XLang image results differ'
    }

    $boundary = Join-Path $work 'boundary.exe'
    $null = Run 'C boundary build' $gcc ($flags + @('extlibs/opencv/test/boundary_c.c',
        $wrapper, $runtime) + $links + @('-o', $boundary))
    $null = Run 'C boundary' $boundary @()
    $null = Run 'XLang boundary' $xl @('run', 'extlibs/opencv/test/boundary.xl')

    $previousShim = $env:OPENCV_XLANG_DLL
    try {
        $env:OPENCV_XLANG_DLL = (Resolve-Path -LiteralPath 'extlibs/opencv/bin/opencv_xlang.dll').Path
        $null = Run 'explicit shim path' $boundary @()
        if (-not $SkipAot) {
            $aot = Join-Path $work 'opencv_aot_core.exe'
            $null = Run 'XLang core AOT build' $xl @('build',
                'extlibs/opencv/test/aot_core.xl', '-o', $aot)
            $null = Run 'XLang core AOT call' $aot @()
        }
    } finally {
        $env:OPENCV_XLANG_DLL = $previousShim
    }

    $utf8Image = Join-Path $work '中文.ppm'
    Copy-Item -LiteralPath 'extlibs/opencv/test/fixture.ppm' -Destination $utf8Image -Force
    $utf8 = Join-Path $work 'utf8_path.exe'
    $null = Run 'C UTF-8 path build' $gcc ($flags + @('extlibs/opencv/test/utf8_path_c.c',
        $wrapper, $runtime) + $links + @('-o', $utf8))
    $null = Run 'C UTF-8 path' $utf8 @($utf8Image)
    $null = Run 'XLang UTF-8 path' $xl @('run', 'extlibs/opencv/test/utf8_path.xl', '--', $utf8Image)

    $partialDir = [IO.Path]::Combine($work, 'partial')
    [void][IO.Directory]::CreateDirectory($partialDir)
    $partialDll = Join-Path $partialDir 'partial_opencv.dll'
    $probe = Join-Path $partialDir 'partial_probe.exe'
    $null = Run 'partial shim build' $gcc @('-std=c11', '-O2', '-Wall', '-Wextra',
        '-Werror', '-shared', 'extlibs/opencv/test/partial_shim.c', '-o', $partialDll)
    $null = Run 'partial probe build' $gcc ($flags + @('extlibs/opencv/test/partial_probe.c',
        $wrapper, $runtime) + $links + @('-o', $probe))
    $previousShim = $env:OPENCV_XLANG_DLL
    try {
        $env:OPENCV_XLANG_DLL = $partialDll
        Push-Location $partialDir
        try {
            $null = Run 'partial shim rejected' $probe @()
        } finally {
            Pop-Location
        }
    } finally {
        $env:OPENCV_XLANG_DLL = $previousShim
    }
} finally {
    Pop-Location
    $safeRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
    $target = [IO.Path]::GetFullPath($work)
    if (!$target.StartsWith($safeRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe opencv work directory' }
    Remove-Item -LiteralPath $target -Recurse -Force
}
