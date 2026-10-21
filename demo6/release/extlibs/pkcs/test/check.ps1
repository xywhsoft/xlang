param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('pkcs_guard_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[pkcs] ${name}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'public header' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-x', 'c', '-Ilib/xrt',
        'release/extlibs/pkcs/include/pkcs_native.h')
    $null = Run 'public header with XRT value' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-x', 'c', '-DXRT_MODULE_VALUE', '-Ilib/xrt',
        'release/extlibs/pkcs/include/pkcs_native.h')
    $null = Run 'strict GCC syntax' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', 'release/extlibs/pkcs/pkcs_native.c')
    $null = Run 'C boundaries' $runner @('pkcs',
        'release/extlibs/pkcs/test/test_pkcs.c')
    $cOutput = Run 'C example' $runner @('pkcs',
        'release/extlibs/pkcs/examples/pkcs_c.c')
    $directOutput = Run 'direct C example' $xl @('run',
        'release/extlibs/pkcs/examples/pkcs_c.c')
    if ($directOutput -cne $cOutput) { throw 'direct C example output differs' }
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/pkcs.xl')
    if ($cOutput -cne $xlOutput) { throw 'C/XLang example output differs' }
    $null = Run 'XLang boundaries' $xl @('run',
        'release/extlibs/pkcs/test/boundaries.xl')
    $null = Run 'XLang existing test' $xl @('run', 'test/cases/extlib_pkcs.xl')

    if (-not $SkipAot) {
        $boundary = Join-Path $work 'boundaries.exe'
        $null = Run 'XLang boundaries build' $xl @('build',
            'release/extlibs/pkcs/test/boundaries.xl', '-o', $boundary)
        $null = Run 'XLang boundaries AOT' $boundary @()
        $example = Join-Path $work 'example.exe'
        $null = Run 'XLang example build' $xl @('build',
            'release/examples/extlibs/pkcs.xl', '-o', $example)
        $aotOutput = Run 'XLang example AOT' $example @()
        if ($aotOutput -cne $cOutput) { throw 'C/XLang AOT output differs' }
    }
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw 'pkcs test directory escaped the build root'
        }
        Remove-Item -LiteralPath $work -Recurse -Force
    }
}
