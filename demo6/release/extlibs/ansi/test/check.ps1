param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/ansi_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/ansi')
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
    Write-Host "[ansi] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/ansi/test/xrt_runtime.c', '-o', $runtime))
    $test = Join-Path $work 'ansi_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/ansi/ansi_native.c',
        'release/extlibs/ansi/test/test_ansi.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @()
    $threads = Join-Path $work 'ansi_threads.exe'
    $null = Run 'C thread test compile' $gcc ($flags + @(
        'release/extlibs/ansi/ansi_native.c',
        'release/extlibs/ansi/test/test_threads.c', $runtime) +
        $links + @('-o', $threads))
    $null = Run 'C thread test' $threads @()
    $example = Join-Path $work 'ansi_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/ansi/ansi_native.c',
        'release/extlibs/ansi/examples/ansi_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()

    Push-Location (Join-Path $root 'release')
    try {
        $directOutput = Run 'single-file C example' $xl @('run',
            'extlibs/ansi/examples/ansi_c.c')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/ansi/test/boundaries.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/ansi/examples/ansi.xl')
        if ($cOutput -cne $directOutput -or $cOutput -cne $xlOutput) {
            throw "ANSI example outputs differ:`nC:`n$cOutput`nDirect:`n$directOutput`nXL:`n$xlOutput"
        }
        Write-Host '[ansi] C/XLang parity: PASS'
        $null = Run 'Public XLang example' $xl @('run', 'examples/extlibs/ansi.xl')
        if (-not $SkipAot) {
            $exampleAot = Join-Path $work 'ansi_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/ansi/examples/ansi.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'ANSI AOT example output differs from C'
            }
            $target = Join-Path $work 'ansi_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/ansi/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $cAot = Join-Path $work 'ansi_c_example.exe'
            $null = Run 'C example AOT build' $xl @('build',
                'extlibs/ansi/examples/ansi_c.c', '-o', $cAot)
            $cAotOutput = Run 'C example AOT' $cAot @()
            if ($cAotOutput -cne $cOutput) {
                throw 'ANSI C AOT example output differs from C'
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
        throw "ansi temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
