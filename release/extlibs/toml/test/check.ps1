param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('toml_guard_' + [guid]::NewGuid().ToString('N'))

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
    Write-Host "[toml] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $null = Run 'strict C syntax' $gcc @(
        '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
        '-fsyntax-only', 'release/extlibs/toml/xrt_toml_parser.c',
        'release/extlibs/toml/xrt_toml_writer.c',
        'release/extlibs/toml/test/abi_contract.c')

    Push-Location $release
    try {
        $nestedFile = Join-Path $work 'nested.toml'
        $null = Run 'C boundaries' $xl @(
            'run', 'extlibs/toml/test/test_toml.c', '--', $nestedFile)
        $python = Get-Command python -CommandType Application -ErrorAction Stop |
            Select-Object -First 1 -ExpandProperty Source
        $null = Run 'Python nested tables' $python @(
            'extlibs/toml/test/check_external.py', '--nested', $nestedFile)
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/toml/test/boundaries.xl')
        $null = Run 'XLang allocation failures' $xl @('run', 'extlibs/toml/test/allocations.xl')
        $xlangOutput = Run 'XLang example' $xl @('run', 'examples/extlibs/toml.xl')
        $cOutput = Run 'C example' $xl @('run', 'examples/extlibs/toml_c.c')
        if ($xlangOutput -cne $cOutput) {
            throw "C/XLang TOML examples differ:`nC:`n$cOutput`nXL:`n$xlangOutput"
        }
        Write-Host '[toml] C/XLang parity: PASS'

        $xlFile = Join-Path $work 'xlang.toml'
        $cFile = Join-Path $work 'c.toml'
        $xlFileOutput = Run 'XLang file example' $xl @(
            'run', 'examples/extlibs/toml.xl', '--', $xlFile)
        $cFileOutput = Run 'C file example' $xl @(
            'run', 'examples/extlibs/toml_c.c', '--', $cFile)
        if ($xlFileOutput -cne $cOutput -or $cFileOutput -cne $cOutput -or
            (Get-FileHash -LiteralPath $xlFile -Algorithm SHA256).Hash -cne
            (Get-FileHash -LiteralPath $cFile -Algorithm SHA256).Hash) {
            throw 'C/XLang TOML file examples differ'
        }
        $null = Run 'Python tomllib availability' $python @('-c', 'import tomllib')
        $null = Run 'Python TOML file interop' $python @(
            'extlibs/toml/test/check_external.py', $cFile, $xlFile)

        if (-not $SkipAot) {
            $target = Join-Path $work 'aot_core.exe'
            $null = Run 'core AOT build' $xl @('build',
                'extlibs/toml/test/aot_core.xl', '-o', $target)
            $null = Run 'core AOT' $target @()
            $exampleTarget = Join-Path $work 'aot_example.exe'
            $null = Run 'example AOT build' $xl @('build',
                'examples/extlibs/toml.xl', '-o', $exampleTarget)
            $aotOutput = Run 'example AOT' $exampleTarget @()
            if ($aotOutput -cne $cOutput) {
                throw "C/AOT TOML examples differ:`nC:`n$cOutput`nAOT:`n$aotOutput"
            }
            Write-Host '[toml] C/AOT parity: PASS'
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
