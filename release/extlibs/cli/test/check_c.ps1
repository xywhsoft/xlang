$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$buildRoot = Join-Path $root 'build/test/cli'
$work = Join-Path $buildRoot ('guard_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[cli] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/cli/test/xrt_runtime.c', '-o', $runtime))
    $source = 'release/extlibs/cli/cli_native.c'
    $test = Join-Path $work 'test_cli.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/cli/test/test_cli.c', $source, $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @()
    $example = Join-Path $work 'example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/cli/examples/basic_c.c', $source, $runtime) +
        $links + @('-o', $example))
    $actual = Run 'C example' $example @()
    $goldenPath = Join-Path $root 'release/extlibs/cli/examples/basic.expected.txt'
    $golden = [IO.File]::ReadAllText($goldenPath).Replace("`r`n", "`n").TrimEnd("`n")
    if ($actual -cne $golden) { throw 'C example output differs from golden file' }
    $xl = Join-Path $root 'release/xl.exe'
    $direct = Run 'C single-file run' $xl @('run',
        'release/extlibs/cli/examples/basic_c.c')
    $directExe = Join-Path $work 'single_file.exe'
    $null = Run 'C single-file build' $xl @('build',
        'release/extlibs/cli/examples/basic_c.c', '-o', $directExe)
    $directAot = Run 'C single-file executable' $directExe @()
    if ($direct -cne $golden -or $directAot -cne $golden) {
        throw 'C single-file output differs from golden file'
    }
    $null = Run 'XLang example check' $xl @(
        'check', 'release/extlibs/cli/examples/basic.xl')
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = (Resolve-Path -LiteralPath $buildRoot).Path.TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw 'cli test directory escaped the build root'
        }
        Remove-Item -LiteralPath $work -Recurse -Force
    }
}
