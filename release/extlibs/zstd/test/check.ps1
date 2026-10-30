param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$xl = Join-Path $root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$workParent = Join-Path $root 'build/test/zstd_guard'
$work = Join-Path $workParent ('run_' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$passed = $false
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$shell = (Get-Process -Id $PID).Path
$checks = [Collections.Generic.List[object]]::new()
$hashes = @{}
$inputs = @('release/xl.exe','release/xl.dll','release/xlc.dll',
    'lib/xrt/xrt.h','lib/xruntime/xruntime.h','test/support/extlib_codec_runtime.c',
    'test/support/extlib_codec_io_errors.h','test/support/extlib_native_codec_file_rollback.h',
    'test/cases/extlib_zstd.xl','test/cases/extlib_zstd_progress.xl','release/examples/extlibs/zstd.xl',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
    'tools/check_extlib_native_globals.ps1','tools/check_extlib_native_globals.sh') | ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/zstd') -File -Recurse | ForEach-Object FullName)
$inputs += @(Get-ChildItem (Join-Path $root 'release/include') -File -Recurse | ForEach-Object FullName)
$inputs += @($gcc,$shell)

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$expectedExit = 0) {
    if ($null -eq $arguments) { $arguments = @() }
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 180
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;
        directory=(Get-Location).Path;expectedExit=$expectedExit;result=$result})
    $checks | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $work 'report.json') -Encoding UTF8
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "[$name] $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expectedExit) {
        throw "[$name] $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[zstd] ${name}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash $path).Hash }
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    if (!(Test-Path -LiteralPath $cc1 -PathType Leaf)) { throw "Missing GCC cc1: $cc1" }
    $hashes[(Resolve-Path $cc1).Path] = (Get-FileHash $cc1).Hash
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.before.json') -Encoding UTF8
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-DXRT_MODULE_DIR_TEMP',
        '-Ilib/xrt', '-Ilib/xruntime', '-Irelease/extlibs/zstd/include')
    $links = @('-lws2_32', '-ladvapi32', '-liphlpapi', '-lm')
    $objects = @()
    foreach ($source in @('test/support/extlib_codec_runtime.c',
        'release/extlibs/zstd/xrt_zstd.c', 'release/extlibs/zstd/zstd_native.c')) {
        $object = Join-Path $work ('source-' + $objects.Count + '.o')
        $null = Run "GCC $source" $gcc ($flags + @('-c', $source, '-o', $object))
        $objects += $object
    }
    $vendor = Join-Path $work 'vendor.o'
    $null = Run 'GCC zstd vendor' $gcc @('-std=c11', '-O2', '-c',
        'release/extlibs/zstd/third_party/zstd.c', '-o', $vendor)
    $objects += $vendor
    $null = Run 'native missing prototypes' $gcc ($flags + @('-Wmissing-prototypes','-fsyntax-only','release/extlibs/zstd/zstd_native.c'))
    foreach ($kind in @('compressor','decompressor')) {
        $diagnostic = Run "private $kind must reject" $xl @('check',"release/extlibs/zstd/test/private_${kind}_negative.xl") -expectedExit 1
        if ($diagnostic -notmatch 'updateNative' -or $diagnostic -notmatch 'access') {throw 'Unrelated private access diagnostic'}
    }

    foreach ($name in @('test_zstd', 'test_file', 'test_allocations','io_errors_c','file_rollback_c','text_views_c')) {
        $source = "release/extlibs/zstd/test/$name.c"
        $exe = Join-Path $work "$name.exe"
        $null = Run "GCC build $name" $gcc ($flags + @($source) + $objects + $links + @('-o', $exe))
        $null = Run "GCC $name" $exe @()
    }
    $null = Run 'TCC IO errors' $xl @('run','release/extlibs/zstd/test/io_errors_c.c')
    $cArgs = @('release/extlibs/zstd/xrt_zstd.c','release/extlibs/zstd/zstd_native.c',
        'release/extlibs/zstd/third_party/zstd.c','--inc-path','lib/xrt','--inc-path','lib/xruntime')
    $null = Run 'TCC open rollback' $xl (@('run','release/extlibs/zstd/test/file_rollback_c.c') + $cArgs)
    $null = Run 'TCC C text views' $xl (@('run','release/extlibs/zstd/test/text_views_c.c') + $cArgs)
    # 此夹具只替换本编译单元的构造器，不能再链接默认 core/bridge 的重复定义。
    $backend = Join-Path $work 'backend-oom.exe'
    $null = Run 'GCC backend OOM build' $gcc ($flags + @('release/extlibs/zstd/test/test_backend_oom.c',
        $objects[0],$vendor) + $links + @('-o',$backend))
    $null = Run 'GCC backend OOM' $backend @()
    $null = Run 'TCC backend OOM' $xl @('run','release/extlibs/zstd/test/test_backend_oom.c')
    $golden = @('source bytes: 7','completed frames: 1','binary round trip: true',
        'output limit rejected: true','zstd bounded stream: OK') -join "`n"
    $bounded = Join-Path $work 'bounded-gcc.exe'
    $null = Run 'GCC bounded example build' $gcc ($flags + @('release/extlibs/zstd/examples/bounded_stream_c.c') +
        $objects + $links + @('-o',$bounded))
    if ((Run 'GCC bounded example' $bounded @()) -cne $golden) { throw 'GCC bounded example differs' }
    foreach ($source in @('bounded_stream.xl','bounded_stream_c.c')) {
        if ((Run "JIT $source" $xl @('run',"release/extlibs/zstd/examples/$source")) -cne $golden) {
            throw "Bounded example differs: $source"
        }
    }
    $transactionGolden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/zstd/examples/file_transaction.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $transaction = Join-Path $work 'transaction.exe'
    $null = Run 'GCC transaction example build' $gcc ($flags + @('release/extlibs/zstd/examples/file_transaction_c.c') + $objects + $links + @('-o',$transaction))
    if ((Run 'GCC transaction example' $transaction @()) -cne $transactionGolden) {throw 'C transaction example differs'}
    foreach ($source in @('file_transaction.xl','file_transaction_c.c')) {
        if ((Run "$source JIT" $xl @('run',"release/extlibs/zstd/examples/$source")) -cne $transactionGolden) {throw "$source output differs"}
        if (!$SkipAot) {
            $exe = Join-Path $work ($source + '.exe')
            $null = Run "$source AOT build" $xl @('build',"release/extlibs/zstd/examples/$source",'-o',$exe)
            if ((Run "$source AOT" $exe @()) -cne $transactionGolden) {throw "$source AOT differs"}
        }
    }
    $source = 'release/extlibs/zstd/examples/basic_c.c'
    $cExe = Join-Path $work 'basic-gcc.exe'
    $null = Run 'GCC example build' $gcc ($flags + @($source) + $objects + $links + @('-o', $cExe))
    $gccOutput = Run 'GCC example' $cExe @()
    $directOutput = Run 'TCC single-file example' $xl @('run', $source)
    $directExe = Join-Path $work 'basic-tcc.exe'
    $null = Run 'TCC single-file build' $xl @('build', $source, '-o', $directExe)
    $builtOutput = Run 'TCC single-file executable' $directExe @()
    Set-Location $release
    $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/zstd/examples/basic.xl')
    foreach ($case in @('../test/cases/extlib_zstd_progress.xl',
        '../test/cases/extlib_zstd.xl',
        'extlibs/zstd/test/wire_fixture.xl',
        'extlibs/zstd/test/file_ops.xl',
        'extlibs/zstd/test/concat_boundary.xl',
        'extlibs/zstd/test/error_dispatch.xl')) {
        $null = Run "XLang $case" $xl @('run', $case)
    }
    $teachingOutput = Run 'XLang teaching example' $xl @('run',
        'examples/extlibs/zstd.xl')
    if (-not $teachingOutput.Contains('zstd example: PASS')) {
        throw 'zstd teaching example did not reach completion'
    }
    if ($gccOutput -cne 'zstd round trip: hello zstd' -or
        $gccOutput -cne $directOutput -or $gccOutput -cne $builtOutput -or
        $gccOutput -cne $xlOutput) { throw 'C/XLang example output mismatch' }
    if (-not $SkipAot) {
        foreach ($name in @('io_errors_c','file_rollback_c','text_views_c')) {
            $exe = Join-Path $work ($name + '.aot.exe')
            [string[]]$extra = @()
            if ($name -ne 'io_errors_c') {
                $extra = @($cArgs | ForEach-Object {if ($_ -match '^release/|^lib/') {Join-Path $root $_} else {$_}})
            }
            $null = Run "$name AOT build" $xl (@('build',(Join-Path $root "release/extlibs/zstd/test/$name.c")) +
                $extra + @('-o',$exe))
            $null = Run "$name AOT" $exe @()
        }
        $cases = @(
            @{name='backend_oom';source='test/test_backend_oom.c';golden=$null},
            @{name='bounded_xl';source='examples/bounded_stream.xl';golden=$golden},
            @{name='bounded_c';source='examples/bounded_stream_c.c';golden=$golden}
        )
        foreach ($case in $cases) {
            $target = Join-Path $work ($case.name + '.exe')
            $null = Run "AOT $($case.name) build" $xl @('build',
                (Join-Path $root ("release/extlibs/zstd/" + $case.source)),'-o',$target)
            $output = Run "AOT $($case.name)" $target @()
            if ($null -ne $case.golden -and $output -cne $case.golden) { throw "AOT golden differs: $($case.name)" }
        }
        $aot = Join-Path $work 'basic-xl.exe'
        $null = Run 'XLang AOT build' $xl @('build', 'extlibs/zstd/examples/basic.xl', '-o', $aot)
        $aotOutput = Run 'XLang AOT example' $aot @()
        if ($aotOutput -cne $gccOutput) { throw 'XLang AOT output mismatch' }
        foreach ($case in @(
            @('wire_fixture', 'extlibs/zstd/test/wire_fixture.xl', ''),
            @('concat_boundary', 'extlibs/zstd/test/concat_boundary.xl', ''),
            @('error_dispatch', 'extlibs/zstd/test/error_dispatch.xl', ''),
            @('file_ops', 'extlibs/zstd/test/file_ops.xl', 'zstd file operations: PASS'),
            @('progress', '../test/cases/extlib_zstd_progress.xl', ''),
            @('central', '../test/cases/extlib_zstd.xl', '')
        )) {
            $caseExe = Join-Path $work ("zstd_$($case[0]).exe")
            $null = Run "XLang $($case[0]) AOT build" $xl @('build', $case[1], '-o', $caseExe)
            $caseOutput = Run "XLang $($case[0]) AOT" $caseExe @()
            if ($caseOutput -cne $case[2]) {
                throw "unexpected XLang $($case[0]) AOT output: $caseOutput"
            }
        }
        $teachingExe = Join-Path $work 'zstd_teaching.exe'
        $null = Run 'XLang teaching AOT build' $xl @('build',
            'examples/extlibs/zstd.xl', '-o', $teachingExe)
        $teachingAot = Run 'XLang teaching AOT' $teachingExe @()
        if (-not $teachingAot.Contains('zstd example: PASS')) {
            throw 'zstd teaching AOT did not reach completion'
        }
    }
    $lifecycle = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $root 'tools/check_extlib_native_globals.ps1'), '-Root', $root, '-Package', 'zstd')
    if ($SkipAot) { $lifecycle += '-SkipAot' }
    $boundaries = [Collections.Generic.List[string]]::new()
    try {$null = Run 'closed transaction rollback' (Join-Path $work 'file_rollback_c.exe') @('--closed')}
    catch {$boundaries.Add($_.Exception.Message)}
    try {$null = Run 'top-level lifecycle' $shell $lifecycle}
    catch {$boundaries.Add($_.Exception.Message)}
    if ($boundaries.Count) {throw ($boundaries -join "`n")}
    $passed = $true
    Write-Host 'zstd Windows package gate: PASS'
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    $changed = @()
    foreach ($path in $hashes.Keys) {
        if (-not (Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) { $changed += $path }
    }
    if ($changed.Count) {$passed = $false}
    [ordered]@{passed=$passed;changed=$changed;inputs=$hashes;checks=$checks} |
        ConvertTo-Json -Depth 10 | Set-Content ($work + '.report.json') -Encoding UTF8
    if ($changed.Count) {throw "zstd inputs changed: $($changed -join ', ')"}
    $tempRoot = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($workPath).StartsWith('run_')) {
        throw "zstd temporary work path escaped build/test/zstd_guard: $workPath"
    }
    if ($passed) { Remove-Item -LiteralPath $workPath -Recurse -Force }
    else { Write-Warning "zstd failure artifacts: $workPath" }
}
