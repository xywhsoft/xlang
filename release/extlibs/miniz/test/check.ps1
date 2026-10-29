param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-DXRT_MODULE_DEFLATE', '-DXRT_MODULE_INFLATE', '-DXRT_MODULE_DIR_TEMP',
    '-Ilib/xrt', '-Ilib/xruntime', '-Irelease/extlibs/miniz/include')
$links = @('-lws2_32', '-ladvapi32', '-liphlpapi', '-lm')
$workParent = Join-Path $root 'build/test/miniz_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$inputs = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h',
    'release/include/xruntime_decl.h', 'release/include/xl_extlib_abi.h',
    'test/support/extlib_codec_runtime.c', 'tools/run_extlib_c.ps1',
    'tools/check_extlib_native_globals.ps1', 'tools/check_extlib_native_globals.sh',
    'tools/extlib_smoke_process.ps1', 'tools/xlir_native_process.ps1',
    'test/cases/extlib_miniz.xl', 'test/cases/extlib_miniz_progress.xl',
    'release/examples/extlibs/miniz.xl', 'release/examples/extlibs/miniz_basic.xl') |
    ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/miniz') -Recurse -File |
    ForEach-Object FullName)
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousAction
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[miniz] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

function Run-C([string]$name, [string]$source) {
    $previousAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $runner -Package miniz -Source $source 2>&1 |
            ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousAction
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $runner -Package miniz -Source $source`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[miniz] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'GCC version' $gcc @('--version')
    foreach ($order in @(@(), @('-DMINIZ_CORE_FIRST'))) {
        $null = Run 'GCC native header order' $gcc ($flags + $order + @('-fsyntax-only',
            'release/extlibs/miniz/test/native_header.c'))
    }
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'GCC runtime build' $gcc ($flags + @('-c',
        'test/support/extlib_codec_runtime.c', '-o', $runtime))
    $objects = @($runtime)
    foreach ($source in @('xrt_miniz.c', 'miniz_native.c')) {
        $object = Join-Path $work ($source.Replace('.', '_') + '.o')
        $null = Run "GCC native build $source" $gcc ($flags + @('-c',
            "release/extlibs/miniz/$source", '-o', $object))
        $objects += $object
    }
    $testBinary = Join-Path $work 'test_miniz.exe'
    $null = Run 'GCC C boundaries build' $gcc ($flags + @(
        '-DNDEBUG', 'release/extlibs/miniz/test/test_miniz.c') + $objects +
        $links + @('-o', $testBinary))
    $null = Run 'GCC C boundaries' $testBinary @()
    $hex = Run 'GCC zlib sample' $testBinary @('--emit')
    $null = Run 'Python zlib interop' $python @('-I', '-c',
        'import sys,zlib; assert zlib.decompress(bytes.fromhex(sys.argv[1])) == bytes((97,98,99))',
        $hex)
    $allocBinary = Join-Path $work 'test_allocations.exe'
    $null = Run 'GCC allocation build' $gcc ($flags + @(
        'release/extlibs/miniz/test/test_allocations.c') + $objects +
        $links + @('-o', $allocBinary))
    $null = Run 'GCC allocation failures' $allocBinary @()
    $ioBinary = Join-Path $work 'test_io_errors.exe'
    $null = Run 'GCC IO error build' $gcc ($flags + @(
        'release/extlibs/miniz/test/test_io_errors.c') + $objects + $links + @('-o', $ioBinary))
    $null = Run 'GCC IO error provenance' $ioBinary @()
    $null = Run 'TCC IO error provenance' $xl @('run', 'release/extlibs/miniz/test/test_io_errors.c')
    $optionsBinary = Join-Path $work 'test_native_options.exe'
    $null = Run 'GCC native options build' $gcc ($flags + @(
        'release/extlibs/miniz/test/test_native_options.c') + $objects + $links + @('-o', $optionsBinary))
    $null = Run 'GCC native options' $optionsBinary @()
    $null = Run-C 'TCC native options' 'release/extlibs/miniz/test/test_native_options.c'

    foreach ($kind in @('compressor', 'decompressor')) {
        $source = "release/extlibs/miniz/test/private_${kind}_negative.xl"
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
        Write-Host "[miniz] private ${kind}: PASS"
    }

    $null = Run-C 'TCC C boundaries' 'release/extlibs/miniz/test/test_miniz.c'
    foreach ($source in @(
        'test/cases/extlib_miniz.xl',
        'test/cases/extlib_miniz_progress.xl',
        'release/extlibs/miniz/test/file_ops.xl',
        'release/extlibs/miniz/test/allocations.xl',
        'release/extlibs/miniz/test/text_transfer.xl',
        'release/extlibs/miniz/test/native_options.xl',
        'release/examples/extlibs/miniz.xl'
    )) {
        $null = Run "XLang $source" $xl @('run', $source)
    }

    $cOutput = Run-C 'TCC C example' 'release/extlibs/miniz/examples/basic.c'
    $directSource = 'release/extlibs/miniz/examples/basic.c'
    $directOutput = Run 'TCC C single-file example' $xl @('run', $directSource)
    $directBinary = Join-Path $work 'miniz_c_single_file.exe'
    $null = Run 'TCC C single-file build' $xl @('build', $directSource, '-o', $directBinary)
    $directAotOutput = Run 'TCC C single-file executable' $directBinary @()
    $exampleBinary = Join-Path $work 'miniz_c_example.exe'
    $null = Run 'GCC C example build' $gcc ($flags + @(
        'release/extlibs/miniz/examples/basic.c') + $objects +
        $links + @('-o', $exampleBinary))
    $gccOutput = Run 'GCC C example' $exampleBinary @()
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/miniz_basic.xl')
    if ($cOutput -cne $xlOutput -or $cOutput -cne $gccOutput -or
        $cOutput -cne $directOutput -or $cOutput -cne $directAotOutput -or
        $cOutput -cne 'hello miniz') {
        throw "C/XLang example output differs:`nTCC: $cOutput`nGCC: $gccOutput`nXL: $xlOutput"
    }
    Write-Host '[miniz] C/XLang example parity: PASS'
    $streaming = Join-Path $work 'streaming_c.exe'
    $null = Run 'GCC streaming example build' $gcc ($flags + @(
        'release/extlibs/miniz/examples/streaming_c.c') + $objects + $links + @('-o', $streaming))
    $streamingOutput = Run 'GCC streaming example' $streaming @()
    $streamingC = Run 'TCC streaming example' $xl @('run', 'release/extlibs/miniz/examples/streaming_c.c')
    $streamingXl = Run 'XLang streaming example' $xl @('run', 'release/extlibs/miniz/examples/streaming.xl')
    if ($streamingC -cne $streamingOutput -or $streamingXl -cne $streamingOutput -or
        $streamingOutput -cne "hello stream`n12 / 12") { throw 'miniz streaming examples differ' }

    if (-not $SkipAot) {
        $textTarget = Join-Path $work 'miniz_text_transfer.exe'
        $null = Run 'XLang text transfer AOT build' $xl @('build',
            'release/extlibs/miniz/test/text_transfer.xl', '-o', $textTarget)
        $null = Run 'XLang text transfer AOT' $textTarget @()
        $target = Join-Path $work 'miniz_basic.exe'
        $null = Run 'XLang AOT build' $xl @('build',
            'release/examples/extlibs/miniz_basic.xl', '-o', $target)
        $aotOutput = Run 'XLang AOT example' $target @()
        if ($aotOutput -cne $cOutput) {
            throw 'XLang AOT output differs from C example'
        }
        foreach ($source in @('test/cases/extlib_miniz.xl', 'test/cases/extlib_miniz_progress.xl',
            'release/extlibs/miniz/test/file_ops.xl', 'release/extlibs/miniz/test/allocations.xl',
            'release/extlibs/miniz/test/native_options.xl',
            'release/examples/extlibs/miniz.xl')) {
            $target = Join-Path $work (([IO.Path]::GetFileNameWithoutExtension($source)) + '.exe')
            $null = Run "AOT build $source" $xl @('build', $source, '-o', $target)
            $null = Run "AOT $source" $target @()
        }
        foreach ($source in @('streaming.xl', 'streaming_c.c')) {
            $target = Join-Path $work ($source.Replace('.', '_') + '.exe')
            $null = Run "AOT $source build" $xl @('build', "release/extlibs/miniz/examples/$source", '-o', $target)
            $output = Run "AOT $source" $target @()
            if ($output -cne $streamingOutput) { throw "miniz $source AOT differs from GCC" }
        }
    }
    $lifecycle = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $root 'tools/check_extlib_native_globals.ps1'), '-Root', $root, '-Package', 'miniz')
    if ($SkipAot) { $lifecycle += '-SkipAot' }
    $null = Run 'top-level lifecycle' (Get-Process -Id $PID).Path $lifecycle
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) {
            throw "miniz input changed during gate: $path"
        }
    }
    $passed = $true
    Write-Host 'miniz Windows package gate: PASS'
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "miniz temporary work path escaped build/test: $workPath"
    }
    if ($passed) {
        Get-ChildItem -LiteralPath $workPath -File |
            ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
        Remove-Item -LiteralPath $workPath
    } else {
        Write-Warning "miniz failure artifacts: $workPath"
    }
}
