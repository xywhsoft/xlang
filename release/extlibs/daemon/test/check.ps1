param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/daemon/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = [IO.Path]::GetFullPath((Join-Path $testRoot ('daemon_guard_' + [guid]::NewGuid().ToString('N'))))

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($exitCode -ne 0) {
        throw "${name}: exit $exitCode`n$($output -join "`n")"
    }
    Write-Host "[daemon] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

function RunC([string]$name, [string]$source) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $runner -Package daemon -Source $source 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($exitCode -ne 0) {
        throw "${name}: exit $exitCode`n$($output -join "`n")"
    }
    Write-Host "[daemon] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'manifest macro compile' $gcc (@('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/daemon/include') +
        $moduleFlags + @('release/extlibs/daemon/daemon_native.c'))
    $null = RunC 'C boundary test' 'release/extlibs/daemon/test/test_daemon.c'
    $null = Run 'XLang core' $xl @('run', 'test/cases/extlib_daemon.xl')
    $null = Run 'XLang FFI boundaries' $xl @('run',
        'release/extlibs/daemon/test/ffi_boundaries.xl')
    $null = Run 'XLang top-level lifecycle' $xl @('run',
        'release/extlibs/daemon/test/top_level.xl')

    foreach ($negative in @('private_native_negative.xl',
        'private_status_negative.xl', 'private_getter_negative.xl',
        'unexported_constructor_negative.xl')) {
        $source = "release/extlibs/daemon/test/$negative"
        $previousAction = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $output = & $xl check $source 2>&1
            $status = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousAction
        }
        if ($status -eq 0 -or
            ($output -join "`n") -notmatch '\[(sem|import)\]') {
            throw "${negative}: expected access diagnostic`n$($output -join "`n")"
        }
        Write-Host "[daemon] ${negative}: PASS"
    }

    $cOutput = RunC 'C example' 'release/examples/extlibs/daemon_c.c'
    $directOutput = Run 'C example direct' $xl @('run', 'release/examples/extlibs/daemon_c.c')
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/daemon.xl')
    $cOutput = $cOutput -replace 'pid=\d+', 'pid=<pid>'
    $directOutput = $directOutput -replace 'pid=\d+', 'pid=<pid>'
    $xlOutput = $xlOutput -replace 'pid=\d+', 'pid=<pid>'
    $expectedPath = Join-Path $root 'release/extlibs/daemon/test/daemon.expected.txt'
    $expected = [IO.File]::ReadAllText($expectedPath).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $xlOutput -or $directOutput -cne $cOutput) {
        throw "daemon output differs:`nC:`n$cOutput`nDirect:`n$directOutput`nXL:`n$xlOutput"
    }
    if ($cOutput -cne $expected) { throw 'daemon output differs from fixed expectation' }
    Write-Host '[daemon] C/XLang parity: PASS'
    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory($work) | Out-Null
        foreach ($source in @('test/cases/extlib_daemon.xl',
            'release/extlibs/daemon/test/ffi_boundaries.xl')) {
            $name = [IO.Path]::GetFileNameWithoutExtension($source)
            $target = Join-Path $work "$name.exe"
            $null = Run "$name build" $xl @('build', $source, '-o', $target)
            $null = Run "$name AOT" $target @()
        }
        $aot = Join-Path $work 'daemon_example.exe'
        $null = Run 'XLang example build' $xl @('build',
            'release/examples/extlibs/daemon.xl', '-o', $aot)
        $aotOutput = Run 'XLang example AOT' $aot @()
        $aotOutput = $aotOutput -replace 'pid=\d+', 'pid=<pid>'
        if ($aotOutput -cne $cOutput) {
            throw "AOT/C daemon output differs:`nC:`n$cOutput`nAOT:`n$aotOutput"
        }
        Write-Host '[daemon] AOT/C parity: PASS'

        $cAot = Join-Path $work 'daemon_c_example.exe'
        $null = Run 'C example build' $xl @('build',
            'release/examples/extlibs/daemon_c.c', '-o', $cAot)
        $cAotOutput = Run 'C example AOT' $cAot @()
        $cAotOutput = $cAotOutput -replace 'pid=\d+', 'pid=<pid>'
        if ($cAotOutput -cne $expected) {
            throw 'C AOT daemon output differs from fixed expectation'
        }

        $topLevel = Join-Path $work 'top_level.exe'
        $null = Run 'top-level build' $xl @('build',
            'release/extlibs/daemon/test/top_level.xl', '-o', $topLevel)
        $null = Run 'top-level AOT' $topLevel @()
    }
} finally {
    Pop-Location
    if (-not $work.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe daemon cleanup path: $work"
    }
    if (Test-Path -LiteralPath $work) {
        Remove-Item -LiteralPath $work -Recurse -Force
    }
}
