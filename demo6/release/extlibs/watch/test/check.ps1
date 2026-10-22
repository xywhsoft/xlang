param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/watch_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
$case = Join-Path $work 'case'
[IO.Directory]::CreateDirectory($work) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $case 'build/test/watch_guard')) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/watch')
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
    Write-Host "[watch] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $runtime = Join-Path $work 'xrt_runtime.o'
    $runtimeSource = Join-Path $root 'release/extlibs/watch/test/xrt_runtime.c'
    $xrtHeader = Join-Path $root 'lib/xrt/xrt.h'
    if (-not (Test-Path -LiteralPath $runtime) -or
        (Get-Item -LiteralPath $runtime).LastWriteTimeUtc -lt
            (Get-Item -LiteralPath $runtimeSource).LastWriteTimeUtc -or
        (Get-Item -LiteralPath $runtime).LastWriteTimeUtc -lt
            (Get-Item -LiteralPath $xrtHeader).LastWriteTimeUtc) {
        $null = Run 'XRT compile' $gcc ($flags + @('-c',
            'release/extlibs/watch/test/xrt_runtime.c', '-o', $runtime))
    }
    $test = Join-Path $work 'watch_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/watch/watch_native.c',
        'release/extlibs/watch/test/test_watch.c', $runtime) +
        $links + @('-o', $test))
    Push-Location $case
    try {
        $null = Run 'C test' $test @()
    } finally {
        Pop-Location
    }
    $race = Join-Path $work 'watch_race_test.exe'
    $null = Run 'directory race compile' $gcc ($flags + @(
        'release/extlibs/watch/watch_native.c',
        'release/extlibs/watch/test/test_directory_race.c', $runtime,
        '-Wl,--wrap=xrtDirOpen') + $links + @('-o', $race))
    Push-Location $case
    try {
        $null = Run 'directory race' $race @()
    } finally {
        Pop-Location
    }
    $example = Join-Path $work 'watch_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/watch/examples/watch_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $expected = @(
        'count: 1',
        'exists: true',
        'path matches: true',
        'after remove: 0',
        'slot reused: true',
        'created: true',
        'modified: true',
        'deleted: true'
    ) -join "`n"
    if ($cOutput -cne $expected) {
        throw "C watch example differs from expected output:`n$cOutput"
    }
    $directC = Run 'C example xl run' $xl @('run',
        'release/extlibs/watch/examples/watch_c.c')
    if ($directC -cne $cOutput) {
        throw 'direct C watch example differs from GCC output'
    }

    $boundarySource = Join-Path $root 'release/extlibs/watch/test/boundaries.xl'
    $cwdSource = Join-Path $root 'release/extlibs/watch/test/cwd_snapshot.xl'
    $allocationSource = Join-Path $root 'release/extlibs/watch/test/allocations.xl'
    $privateSource = Join-Path $root 'release/extlibs/watch/test/private_native.xl'
    Push-Location $case
    try {
        $null = Run 'XLang boundaries' $xl @('run', $boundarySource)
        $null = Run 'XLang cwd snapshot' $xl @('run', $cwdSource)
        $null = Run 'XLang allocation failures' $xl @('run', $allocationSource)
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $privateDiagnostic = ((& $xl check $privateSource 2>&1 |
                ForEach-Object { $_.ToString() }) -join "`n")
            $privateExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousPreference
        }
        if ($privateExitCode -eq 0 -or -not $privateDiagnostic.Contains('addNative') -or
            -not $privateDiagnostic.Contains('access')) {
            throw "watch Native member access check failed: $privateDiagnostic"
        }
    } finally {
        Pop-Location
    }
    if (-not $SkipAot) {
        $target = Join-Path $work 'watch_xl_test.exe'
        $null = Run 'XLang boundaries build' $xl @('build',
            $boundarySource, '-o', $target)
        Push-Location $case
        try {
            $null = Run 'XLang boundaries AOT' $target @()
        } finally {
            Pop-Location
        }
    }

    Push-Location (Join-Path $root 'release')
    try {
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/watch/examples/watch.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang watch example outputs differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[watch] C/XLang parity: PASS'
        $null = Run 'Public XLang example' $xl @('run', 'examples/extlibs/watch.xl')
        if (-not $SkipAot) {
            $exampleAot = Join-Path $work 'watch_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/watch/examples/watch.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT watch example differs from C'
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
        throw "watch temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
