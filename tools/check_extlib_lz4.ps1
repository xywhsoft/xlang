param([string]$Root = '', [string]$WorkDirectory = '', [switch]$Interop)

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$work = if (!$WorkDirectory) { Join-Path $Root 'build/test/lz4_guard' } else { [IO.Path]::GetFullPath((Join-Path $Root $WorkDirectory)) }
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$xl = Join-Path $Root 'release/xl.exe'
$environment = @{XLANG_PACKAGE_PATH=(Join-Path $Root 'release/extlibs')}
$checks = [Collections.Generic.List[object]]::new()
$inputs = [ordered]@{}
$manifest = Get-Content (Join-Path $Root 'release/extlibs/lz4/xlang.package.json') -Raw | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/lz4/$_" })

function InputPaths {
    foreach ($pkg in @('lz4','xxhash')) {
        Get-ChildItem (Join-Path $Root "release/extlibs/$pkg") -Recurse -File | ForEach-Object FullName
    }
    foreach ($p in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','test/support/extlib_codec_runtime.c',
        'test/support/extlib_codec_file_rollback.h',
        'tools/check_extlib_lz4.ps1','tools/check_extlib_lz4.sh','tools/check_extlib_lz4_interop.py',
        'tools/check_extlib_native_globals.ps1','tools/check_extlib_native_globals.sh',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
        'test/featured_extlibs.json','test/suites/11_featured_extlibs_rc.bat',
        'test/cases/extlib_lz4.xl','test/cases/extlib_lz4_progress.xl',
        'release/examples/extlibs/lz4.xl','release/examples/extlibs/lz4_basic.xl',
        'release/examples/extlibs/lz4_files.xl','release/examples/extlibs/lz4_basic.expected.txt',
        'release/examples/extlibs/lz4_files.expected.txt','release/xl.exe','release/xl.dll','release/xlc.dll',
        'release/include/xrt_decl.h','release/include/xruntime_decl.h','release/include/xl_extlib_abi.h')) {
        Join-Path $Root $p
    }
    $gcc
}
foreach ($p in @(InputPaths | Sort-Object -Unique)) { $inputs[$p] = (Get-FileHash -LiteralPath $p).Hash }

function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 120) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds $environment
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;result=$r})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[lz4] ${Name}: PASS $($r.stdout.Trim())"
    $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Ilib/xruntime')
$objects = @()
$outcome = 'failed'
$failure = ''
try {
    foreach ($kind in @('compressor','decompressor')) {
        $name = "private_${kind}_negative"
        $args = @('check',"release/extlibs/lz4/test/$name.xl")
        $result = Invoke-ExtlibSmokeProcess $xl $args $Root 90 $environment
        $checks.Add([ordered]@{name=$name;program=$xl;arguments=$args;result=$result})
        if ($result.status -ne 'fail' -or
            !$result.stderr.Contains('updateNative') -or
            !$result.stderr.Contains('access')) {
            throw "$name did not report private access"
        }
        Write-Host "[lz4] ${name}: rejected as expected"
    }
    $null = Check 'compiler-version' $gcc @('--version')
    $runtime = Join-Path $work 'runtime.o'
    $null = Check 'runtime' $gcc ($flags + @('-DXRT_MODULE_DIR','-DXRT_MODULE_DIR_TEMP','-DXRT_MODULE_FILE_WHOLE','-DXRT_MODULE_PATH','-DXRT_MODULE_THREAD','-DXRT_MODULE_ATOMIC',
        '-c','test/support/extlib_codec_runtime.c','-o',$runtime))
    $objects += $runtime
    $index = 0
    foreach ($source in $sources) {
        $object = Join-Path $work "source-$index.o"
        $compile = if ($source -match '/(xrt_lz4(?:_file)?|lz4_native)\.c$') { $flags } else { @('-std=c11','-O2') }
        $null = Check "source-$index" $gcc ($compile + @('-c',$source,'-o',$object))
        $objects += $object
        ++$index
    }
    foreach ($name in @('boundaries','stream','alloc','threads','native','files','cleanup')) {
        $exe = Join-Path $work "$name.exe"
        $null = Check "$name-build" $gcc ($flags + @('-DNDEBUG',"release/extlibs/lz4/test/test_lz4_$name.c") +
            $objects + @('-lWs2_32','-lAdvapi32','-lm','-o',$exe))
        $arguments = if ($name -in @('native','files','cleanup')) { @((Join-Path $work ($name+'-'+[Guid]::NewGuid().ToString('N')))) } else { @() }
        $null = Check $name $exe $arguments
    }
    $interopExe = Join-Path $work 'interop.exe'
    $null = Check 'interop-build' $gcc ($flags + @('release/extlibs/lz4/test/lz4_interop.c') +
        $objects + @('-lWs2_32','-lAdvapi32','-lm','-o',$interopExe))
    if ($Interop) {
        $wsl = (Get-Command wsl.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
        function LinuxPath([string]$Path) {
            $pathResult = Check 'wsl-path' $wsl @('-e','wslpath','-a','-u',$Path)
            return $pathResult.Trim()
        }
        $null = Check 'system-lz4-interop' $wsl @('-e','python3',
            (LinuxPath (Join-Path $Root 'tools/check_extlib_lz4_interop.py')), (LinuxPath $interopExe),
            (LinuxPath (Join-Path $work 'interop')), '--library','/usr/lib/x86_64-linux-gnu/liblz4.so.1') 300
    }

    foreach ($name in @('basic','files')) {
        $directSource = "release/extlibs/lz4/examples/$name.c"
        $directGolden = [IO.File]::ReadAllText((Join-Path $Root "release/examples/extlibs/lz4_$name.expected.txt")).Replace("`r`n","`n").TrimEnd("`n")
        $directRun = Check "$name-c-single-file-run" $xl @('run',$directSource)
        $directExe = Join-Path $work "$name-c-single-file.exe"
        $null = Check "$name-c-single-file-build" $xl @('build',$directSource,'-o',$directExe)
        $directBuild = Check "$name-c-single-file-aot" $directExe @()
        if ($directRun -cne $directGolden -or $directBuild -cne $directGolden) {
            throw "Single-file LZ4 $name C example output mismatch"
        }
    }

    $decls = @()
    foreach ($name in @('extlib_lz4','extlib_lz4_boundaries','extlib_lz4_progress')) {
        $case = if ($name -eq 'extlib_lz4_boundaries') { 'release/extlibs/lz4/test/boundaries.xl' } else { "test/cases/$name.xl" }
        $exe = Join-Path $work "$name.exe"
        $null = Check "$name-jit" $xl @('run',$case)
        $null = Check "$name-build" $xl @('build',$case,'-o',$exe)
        $null = Check "$name-aot" $exe @()
        $cgen = Join-Path $work "$name.c"
        $null = Check "$name-cgen" $xl @('cgen',$case,'--emit',$cgen)
        $decls += @(Get-Content $cgen | Where-Object { $_ -match '^extern .*\bxlz4_[a-z_]+\(' })
    }
    $decls = @($decls | Sort-Object -Unique)
    $binding = Get-Content (Join-Path $Root 'release/extlibs/lz4/src/lz4.xl') -Raw
    $expected = @([regex]::Matches($binding,'extern "(xlz4_[a-z_]+)"') | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxlz4_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -ne 0 -or $decls.Count -ne $expected.Count) { throw 'Incomplete generated LZ4 ABI coverage' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include <stdint.h>`n#include `"lz4_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Check "generated-$($expected.Count)-prototypes" $gcc ($flags + @('-Irelease/extlibs/lz4/include','-fsyntax-only',$abi))

    foreach ($name in @('lz4','lz4_basic','lz4_files')) {
        $case = "release/examples/extlibs/$name.xl"
        $text = Check "$name-example-jit" $xl @('run',$case)
        $exe = Join-Path $work "$name-example.exe"
        $null = Check "$name-example-build" $xl @('build',$case,'-o',$exe)
        $aot = Check "$name-example-aot" $exe @()
        if ($text -cne $aot) { throw "$name JIT/AOT output mismatch" }
        if ($name -eq 'lz4') { continue }
        $golden = [IO.File]::ReadAllText((Join-Path $Root "release/examples/extlibs/$name.expected.txt")).Replace("`r`n","`n").TrimEnd("`n")
        $c = "release/extlibs/lz4/examples/$($name.Substring(4)).c"
        $exe = Join-Path $work "$name-c.exe"
        $null = Check "$name-c-build" $gcc ($flags + @($c) + $objects + @('-lWs2_32','-lAdvapi32','-lm','-o',$exe))
        $native = Check "$name-c" $exe @()
        # 单文件 C JIT/AOT 已在前面分别与黄金输出核验。
        $exe = Join-Path $work "$name-c-tcc.exe"
        $null = Check "$name-c-aot-build" $xl @('build',$c,'--inc-path','lib/xrt','--inc-path','lib/xruntime','-o',$exe)
        $caot = Check "$name-c-aot" $exe @()
        $helper = Check "$name-c-package-runner" (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','lz4','-Source',$c)
        $mismatches = @()
        foreach ($sample in @(@('xlang',$text),@('gcc',$native),@('tcc-aot',$caot),@('runner',$helper))) {
            if ($sample[1] -cne $golden) { $mismatches += "$($sample[0]): $($sample[1].Length) characters" }
        }
        if ($mismatches.Count) {
            throw "$name bilingual golden output mismatch ($($mismatches -join ', '))"
        }
    }
    $probe = 'release/extlibs/lz4/test/test_lz4_files.c'
    $null = Check 'files-jit' $xl (@('run',$probe) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','--',
        (Join-Path $work ('jit-files-'+[Guid]::NewGuid().ToString('N')))))
    $exe = Join-Path $work 'files-aot.exe'
    $null = Check 'files-aot-build' $xl (@('build',$probe) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','-o',$exe))
    $null = Check 'files-aot' $exe @((Join-Path $work ('aot-files-'+[Guid]::NewGuid().ToString('N'))))

    # C 单文件路径也验证同一文件实现，不只测试正式分离链接的对象。
    $cleanupProbe = 'release/extlibs/lz4/test/test_lz4_cleanup.c'
    $null = Check 'cleanup-jit' $xl @('run',$cleanupProbe,'--',
        (Join-Path $work ('jit-cleanup-'+[Guid]::NewGuid().ToString('N'))))
    $cleanupExe = Join-Path $work 'cleanup-aot.exe'
    $null = Check 'cleanup-aot-build' $xl @('build',$cleanupProbe,'-o',$cleanupExe)
    $null = Check 'cleanup-aot' $cleanupExe @((Join-Path $work ('aot-cleanup-'+[Guid]::NewGuid().ToString('N'))))

    # 已关闭事务的回滚必须验收；不能用仍打开时的修复掩盖此红色边界。
    $commitExe = Join-Path $work 'commit-cleanup.exe'
    $null = Check 'commit-cleanup-build' $gcc ($flags + @(
        'release/extlibs/lz4/test/test_lz4_commit_cleanup.c') + $objects +
        @('-lWs2_32','-lAdvapi32','-lm','-o',$commitExe))
    $null = Check 'commit-cleanup' $commitExe @((Join-Path $work ('commit-cleanup-'+[Guid]::NewGuid().ToString('N'))))
    $null = Check 'top-level-lifecycle' (Get-Process -Id $PID).Path @('-NoProfile','-ExecutionPolicy','Bypass',
        '-File','tools/check_extlib_native_globals.ps1','-Root',$Root,'-Package','lz4') 360
    $outcome = 'passed'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    $changed = @($inputs.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $inputs[$_] })
    $changed += @(InputPaths | Where-Object { !$inputs.Contains($_) })
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        scope='Windows strict C, allocation, IO/reentry, threads, file transactions, generated ABI, XL/C JIT/AOT and paired examples';
        independentInterop=[bool]$Interop;
        inputs=$inputs;changedInputs=$changed;checks=$checks.ToArray()}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'), ($report | ConvertTo-Json -Depth 8), $utf8)
    if ($changed.Count) { throw 'Inputs changed during lz4 gate' }
}
