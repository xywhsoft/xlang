param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/qr_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/qr/include')
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
    Write-Host "[qr] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'C ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/qr/test/abi_contract.c'))
    $null = Run 'manifest C syntax' $gcc ($flags + @(
        '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_VALUE_CONTAINER',
        '-DXRT_MODULE_UNICODE', '-fsyntax-only',
        'release/extlibs/qr/qr_native.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/qr/test/xrt_runtime.c', '-o', $runtime))
    $test = Join-Path $work 'qr_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/qr/qr_native.c',
        'release/extlibs/qr/test/test_qr.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @()

    $example = Join-Path $work 'qr_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/examples/extlibs/qr_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $null = Run 'Central XLang test' $xl @('run', 'test/cases/extlib_qr.xl')

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/qr/test/boundaries.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'examples/extlibs/qr.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang qr example outputs differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[qr] C/XLang parity: PASS'

        $core = Join-Path $work 'qr_xl_core.exe'
        $null = Run 'XLang core build' $xl @('build',
            'extlibs/qr/test/aot_core.xl', '-o', $core)
        $null = Run 'XLang core AOT' $core @()
        if (-not $SkipAot) {
            $target = Join-Path $work 'qr_xl_boundaries.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/qr/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            Push-Location $root
            try {
                $target = Join-Path $work 'qr_xl_central.exe'
                $null = Run 'Central XLang build' $xl @('build',
                    'test/cases/extlib_qr.xl', '-o', $target)
                $null = Run 'Central XLang AOT' $target @()
            } finally {
                Pop-Location
            }
            $target = Join-Path $work 'qr_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'examples/extlibs/qr.xl', '-o', $target)
            $aotOutput = Run 'XLang example AOT' $target @()
            if ($aotOutput -cne $cOutput) {
                throw 'C/XLang AOT qr example outputs differ'
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
        throw "qr temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
