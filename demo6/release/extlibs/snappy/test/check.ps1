param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Ilib/xruntime', '-Irelease/extlibs/snappy/include')
$links = @('-lws2_32', '-ladvapi32', '-liphlpapi', '-lm')
$workParent = Join-Path $root 'build/test/snappy_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousAction
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[snappy] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

function Run-C([string]$name, [string]$source) {
    $previousAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $runner -Package snappy -Source $source 2>&1 |
            ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousAction
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[snappy] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'GCC runtime build' $gcc ($flags + @('-c',
        'test/support/extlib_codec_runtime.c', '-o', $runtime))
    $objects = @($runtime)
    foreach ($source in @(
        'xrt_snappy.c', 'snappy_native.c', 'third_party/xrt_snappy.c'
    )) {
        $object = Join-Path $work (($source -replace '[/\\.]', '_') + '.o')
        $null = Run "GCC native build $source" $gcc ($flags + @('-c',
            "release/extlibs/snappy/$source", '-o', $object))
        $objects += $object
    }
    $testBinary = Join-Path $work 'test_snappy.exe'
    $null = Run 'GCC C boundaries build' $gcc ($flags + @(
        '-DNDEBUG', '-DSNAPPY_MEMORY_GUARD',
        'release/extlibs/snappy/test/test_snappy.c') + $objects +
        $links + @('-o', $testBinary))
    $null = Run 'GCC C boundaries' $testBinary @()
    $allocBinary = Join-Path $work 'test_allocations.exe'
    $null = Run 'GCC allocation build' $gcc ($flags + @(
        'release/extlibs/snappy/test/test_allocations.c') + $objects +
        $links + @('-o', $allocBinary))
    $null = Run 'GCC allocation failures' $allocBinary @()

    foreach ($kind in @('compressor', 'decompressor')) {
        $source = "release/extlibs/snappy/test/private_${kind}_negative.xl"
        $previousAction = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $diagnostic = & $xl check $source 2>&1
            $diagnosticExit = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousAction
        }
        if ($diagnosticExit -eq 0 -or
            ($diagnostic -join "`n") -notmatch 'updateNative' -or
            ($diagnostic -join "`n") -notmatch 'access') {
            throw "private member was not rejected: $source`n$($diagnostic -join "`n")"
        }
        Write-Host "[snappy] private ${kind}: PASS"
    }

    $null = Run-C 'C boundaries' 'release/extlibs/snappy/test/test_snappy.c'
    Set-Location (Join-Path $root 'release')
    foreach ($source in @(
        '../test/cases/extlib_snappy.xl',
        '../test/cases/extlib_snappy_progress.xl',
        'extlibs/snappy/test/file_ops.xl',
        'extlibs/snappy/test/allocations.xl',
        'extlibs/snappy/test/text_transfer.xl',
        'examples/extlibs/snappy.xl'
    )) {
        $null = Run "XLang $source" $xl @('run', $source)
    }

    Set-Location $root
    $cOutput = Run-C 'C example' 'release/extlibs/snappy/examples/basic.c'
    $exampleBinary = Join-Path $work 'snappy_c_example.exe'
    $null = Run 'GCC C example build' $gcc ($flags + @(
        'release/extlibs/snappy/examples/basic.c') + $objects +
        $links + @('-o', $exampleBinary))
    $gccOutput = Run 'GCC C example' $exampleBinary @()
    Set-Location (Join-Path $root 'release')
    $directOutput = Run 'C example xl run' $xl @('run',
        'extlibs/snappy/examples/basic.c')
    $xlOutput = Run 'XLang example' $xl @('run',
        'examples/extlibs/snappy_basic.xl')
    if ($cOutput -cne $xlOutput -or $cOutput -cne $gccOutput -or
        $cOutput -cne $directOutput -or
        $cOutput -cne 'hello snappy') {
        throw "C/XLang example output differs:`nTCC: $cOutput`nGCC: $gccOutput`nDirect: $directOutput`nXL: $xlOutput"
    }
    Write-Host '[snappy] C/XLang example parity: PASS'

    if (-not $SkipAot) {
        $textTarget = Join-Path $work 'snappy_text_transfer.exe'
        $null = Run 'XLang text transfer AOT build' $xl @('build',
            'extlibs/snappy/test/text_transfer.xl', '-o', $textTarget)
        $null = Run 'XLang text transfer AOT' $textTarget @()
        $target = Join-Path $work 'snappy_basic.exe'
        $null = Run 'XLang AOT build' $xl @('build',
            'examples/extlibs/snappy_basic.xl', '-o', $target)
        $aotOutput = Run 'XLang AOT example' $target @()
        if ($aotOutput -cne $cOutput) {
            throw 'XLang AOT output differs from C example'
        }
    }
    Write-Host 'snappy Windows package gate: PASS'
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "snappy temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
