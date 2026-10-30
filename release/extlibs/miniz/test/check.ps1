param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$shell = (Get-Process -Id $PID).Path
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
    'test/support/extlib_codec_runtime.c', 'test/support/extlib_native_codec_file_rollback.h',
    'tools/run_extlib_c.ps1',
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
$checks = [Collections.Generic.List[object]]::new()
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
foreach ($path in @($gcc, $python, $shell)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$expectedExit = 0) {
    if ($null -eq $arguments) { $arguments = @() }
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 240 @{LC_ALL='C'}
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;expectedExit=$expectedExit;result=$result})
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expectedExit) {
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[miniz] ${name}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n", "`n").TrimEnd("`n")
}

function Run-C([string]$name, [string]$source) {
    return Run $name $shell @('-NoProfile','-ExecutionPolicy','Bypass','-File',
        $runner,'-Package','miniz','-Source',$source)
}

Push-Location $root
try {
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $null = Run 'Python version' $python @('--version')
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json') -Encoding UTF8
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
    $interop = Join-Path $work 'interop.exe'
    $null = Run 'GCC interop build' $gcc ($flags + @('release/extlibs/miniz/test/interop.c') +
        $objects + $links + @('-o', $interop))
    $null = Run 'Python zlib interop' $python @('-I','-O','release/extlibs/miniz/test/interop.py',$interop)
    $rollback = Join-Path $work 'file_rollback.exe'
    $null = Run 'GCC rollback build' $gcc ($flags + @('release/extlibs/miniz/test/file_rollback_c.c') +
        $objects + $links + @('-o', $rollback))
    $null = Run 'GCC open rollback' $rollback @()
    $null = Run-C 'TCC open rollback' 'release/extlibs/miniz/test/file_rollback_c.c'
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
        $diagnostic = Run "private $kind must reject" $xl @('check',$source) -expectedExit 1
        if ($diagnostic -notmatch 'updateNative' -or $diagnostic -notmatch 'access') {
            throw "private member was not rejected: $source`n$diagnostic"
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
    $golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/miniz/examples/file_transaction.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $transaction = Join-Path $work 'transaction.exe'
    $null = Run 'GCC transaction example build' $gcc ($flags + @('release/extlibs/miniz/examples/file_transaction_c.c') +
        $objects + $links + @('-o',$transaction))
    $output = Run 'GCC transaction example' $transaction @()
    if ($output -cne $golden) { throw 'C transaction example differs from golden' }
    foreach ($source in @('file_transaction.xl','file_transaction_c.c')) {
        $output = Run "$source JIT" $xl @('run',"release/extlibs/miniz/examples/$source")
        if ($output -cne $golden) { throw "$source differs from golden" }
        if (-not $SkipAot) {
            $exe = Join-Path $work ($source + '.exe')
            $null = Run "$source AOT build" $xl @('build',"release/extlibs/miniz/examples/$source",'-o',$exe)
            $output = Run "$source AOT" $exe @()
            if ($output -cne $golden) { throw "$source AOT differs from golden" }
        }
    }

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
        $exe = Join-Path $work 'rollback_aot.exe'
        $null = Run 'C rollback AOT build' $xl @('build','release/extlibs/miniz/test/file_rollback_c.c',
            'release/extlibs/miniz/xrt_miniz.c','release/extlibs/miniz/miniz_native.c',
            '--inc-path','lib/xrt','--inc-path','lib/xruntime','-o',$exe)
        $null = Run 'C rollback AOT' $exe @()
    }
    $lifecycle = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $root 'tools/check_extlib_native_globals.ps1'), '-Root', $root, '-Package', 'miniz')
    if ($SkipAot) { $lifecycle += '-SkipAot' }
    # 独立执行，不能用关闭态回滚失败掩盖 M2，也不能设成预期失败。
    $boundaries = [Collections.Generic.List[string]]::new()
    try { $null = Run 'closed transaction rollback' $rollback @('--closed') }
    catch { $boundaries.Add($_.Exception.Message) }
    try { $null = Run 'top-level lifecycle' $shell $lifecycle }
    catch { $boundaries.Add($_.Exception.Message) }
    if ($boundaries.Count) { throw ($boundaries -join "`n") }
    $passed = $true
    Write-Host 'miniz Windows package gate: PASS'
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    $unchanged = $true
    foreach ($path in $hashes.Keys) {
        if (-not (Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            $unchanged = $false
            Write-Warning "miniz input changed during gate: $path"
        }
    }
    $passed = $passed -and $unchanged
    [ordered]@{passed=$passed;unchanged=$unchanged;inputs=$hashes;checks=$checks} |
        ConvertTo-Json -Depth 9 | Set-Content ($work + '.report.json') -Encoding UTF8
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
    if (-not $unchanged) { throw 'miniz validation inputs changed' }
}
