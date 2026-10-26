param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[imagehash] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'strict GCC compile' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_ONCE',
        '-DXRT_MODULE_THREAD_KEY', '-Ilib/xrt',
        '-Irelease/extlibs/imagehash', '-Irelease/extlibs/imagehash/include',
        '-fsyntax-only', 'release/extlibs/imagehash/imagehash_native.c')
    foreach ($name in @('test_imagehash', 'test_error_threads')) {
        $null = Run $name 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
            '-File', $runner, '-Package', 'imagehash', '-Source',
            "release/extlibs/imagehash/test/$name.c")
    }
    $null = Run 'XLang central' $xl @('run', 'test/cases/extlib_imagehash.xl')
    $null = Run 'XLang allocations' $xl @('run',
        'release/extlibs/imagehash/test/allocations.xl')

    $cOutput = Run 'C example' 'powershell' @('-NoProfile', '-ExecutionPolicy',
        'Bypass', '-File', $runner, '-Package', 'imagehash', '-Source',
        'release/extlibs/imagehash/examples/imagehash.c')
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/imagehash.xl')
    if ($cOutput -cne $xlOutput) {
        throw "C/XLang examples differ:`nC:`n$cOutput`nXL:`n$xlOutput"
    }
    $expected = (Get-Content -LiteralPath 'release/extlibs/imagehash/examples/imagehash.expected.txt' -Raw).
        Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $expected) {
        throw "Imagehash example differs from fixed result:`nActual:`n$cOutput`nExpected:`n$expected"
    }
    Write-Host '[imagehash] C/XLang parity: PASS'

    $directSource = 'release/extlibs/imagehash/examples/imagehash.c'
    $directOutput = Run 'C example direct JIT' $xl @('run', $directSource)
    if ($directOutput -cne $expected) {
        throw "Direct C JIT example differs from fixed result:`n$directOutput"
    }
    $work = Join-Path $root 'build/test'
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $directExe = Join-Path $work ("imagehash_direct_{0}.exe" -f [guid]::NewGuid().ToString('N'))
    try {
        $null = Run 'C example direct AOT build' $xl @('build', $directSource, '-o', $directExe)
        $builtOutput = Run 'C example direct AOT run' $directExe @()
        if ($builtOutput -cne $expected) {
            throw "Direct C AOT example differs from fixed result:`n$builtOutput"
        }
    } finally {
        if (Test-Path -LiteralPath $directExe -PathType Leaf) {
            Remove-Item -LiteralPath $directExe -Force
        }
    }

    if (-not $SkipAot) {
        $aot = Join-Path $work ("imagehash_core_{0}.exe" -f [guid]::NewGuid().ToString('N'))
        try {
            $null = Run 'core AOT build' $xl @('build',
                'release/extlibs/imagehash/test/aot_core.xl', '-o', $aot)
            $null = Run 'core AOT run' $aot @()
        } finally {
            if (Test-Path -LiteralPath $aot -PathType Leaf) {
                Remove-Item -LiteralPath $aot -Force
            }
        }
        $allocationExe = Join-Path $work ("imagehash_allocations_{0}.exe" -f [guid]::NewGuid().ToString('N'))
        try {
            $null = Run 'allocations AOT build' $xl @('build',
                'release/extlibs/imagehash/test/allocations.xl', '-o', $allocationExe)
            $null = Run 'allocations AOT run' $allocationExe @()
        } finally {
            if (Test-Path -LiteralPath $allocationExe -PathType Leaf) {
                Remove-Item -LiteralPath $allocationExe -Force
            }
        }
        $exampleExe = Join-Path $work ("imagehash_example_{0}.exe" -f [guid]::NewGuid().ToString('N'))
        try {
            $null = Run 'XLang example AOT build' $xl @('build',
                'release/examples/extlibs/imagehash.xl', '-o', $exampleExe)
            $aotOutput = Run 'XLang example AOT run' $exampleExe @()
            if ($aotOutput -cne $expected) {
                throw "XLang AOT example differs from fixed result:`n$aotOutput"
            }
        } finally {
            if (Test-Path -LiteralPath $exampleExe -PathType Leaf) {
                Remove-Item -LiteralPath $exampleExe -Force
            }
        }
    }
} finally {
    Pop-Location
}
