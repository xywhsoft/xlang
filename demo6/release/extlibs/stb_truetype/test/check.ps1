param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$workRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $workRoot ('stb_truetype_guard_' + [guid]::NewGuid().ToString('N'))
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/stb_truetype/include')
$links = @('-lm')

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    if ($code -ne 0) { throw "${label}: exit $code`n$($lines -join "`n")" }
    Write-Host "[stb_truetype] ${label}: PASS"
    return ($lines -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    [void][IO.Directory]::CreateDirectory($work)
    $runtime = Join-Path $work 'xrt_runtime.o'
    $native = 'release/extlibs/stb_truetype/stb_truetype_native.c'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/stb_truetype/test/xrt_runtime.c', '-o', $runtime))
    $cTest = Join-Path $work 'stb_truetype_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        $native, 'release/extlibs/stb_truetype/test/test_stb_truetype.c',
        $runtime) + $links + @('-o', $cTest))
    $null = Run 'C boundaries' $cTest @()

    $cExample = Join-Path $work 'stb_truetype_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/examples/extlibs/stb_truetype_c.c', $runtime) +
        $links + @('-o', $cExample))
    $fontPath = Join-Path $root 'release/extlibs/stb_truetype/test/assets/DejaVuSans-test.ttf'
    $cOutput = Run 'C example' $cExample @($fontPath)
    $directC = Run 'C example direct xl run' $xl @('run',
        'release/examples/extlibs/stb_truetype_c.c', '--', $fontPath)
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/stb_truetype.xl', '--', $fontPath)
    if ($cOutput -cne $directC -or $cOutput -cne $xlOutput -or
        -not $cOutput.Contains('stb_truetype C/XL example: PASS')) {
        throw 'C/XLang font example output differs'
    }

    $null = Run 'XLang existing test' $xl @('run',
        'test/cases/extlib_stb_truetype.xl')
    $null = Run 'XLang boundaries' $xl @('run',
        'release/extlibs/stb_truetype/test/boundaries.xl')
    $null = Run 'XLang allocations' $xl @('run',
        'release/extlibs/stb_truetype/test/allocations.xl')
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $diagnostic = @(& $xl check 'release/extlibs/stb_truetype/test/private_bitmap_negative.xl' 2>&1 |
            ForEach-Object { $_.ToString() }) -join "`n"
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    if ($code -eq 0 -or -not $diagnostic.Contains('bitmapNative') -or
        -not $diagnostic.Contains('access')) {
        throw "private bitmap method was not rejected`n$diagnostic"
    }
    Write-Host '[stb_truetype] private bitmap method: PASS'

    if (-not $SkipAot) {
        $aot = Join-Path $work 'stb_truetype_boundaries.exe'
        $null = Run 'XLang boundaries AOT build' $xl @('build',
            'release/extlibs/stb_truetype/test/boundaries.xl', '-o', $aot)
        $null = Run 'XLang boundaries AOT' $aot @()
    }
} finally {
    Pop-Location
    $target = [IO.Path]::GetFullPath($work)
    if (-not $target.StartsWith($workRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $target"
    }
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}
