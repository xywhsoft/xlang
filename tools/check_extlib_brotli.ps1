param([string]$Root = '', [string]$XrtInclude = '', [string]$WorkDirectory = '', [string]$RuntimeDirectory = '', [switch]$SkipAot)

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'release' }
$RuntimeDirectory = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$xl = Join-Path $RuntimeDirectory 'xl.exe'
$environment = @{XLANG_PACKAGE_PATH = Join-Path $Root 'release/extlibs'}
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$work = if (!$WorkDirectory) { Join-Path $Root ('build/test/brotli_guard/run_' + [Guid]::NewGuid().ToString('N')) } elseif ([IO.Path]::IsPathRooted($WorkDirectory)) { [IO.Path]::GetFullPath($WorkDirectory) } else { [IO.Path]::GetFullPath((Join-Path $Root $WorkDirectory)) }
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$manifest = Get-Content (Join-Path $Root 'release/extlibs/brotli/xlang.package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/brotli/$_" })
$paths = @('lib/xrt/xrt.h','lib/xruntime/xruntime.h',
    'release/include/xrt_decl.h','release/include/xruntime_decl.h',
    'test/support/extlib_codec_runtime.c','test/support/extlib_codec_file_rollback.h',
    'test/cases/extlib_codec_file_allocation.c','tools/check_extlib_brotli.ps1',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
    'test/featured_extlibs.json','test/suites/11_featured_extlibs_rc.bat','tools/check_extlib_brotli.sh',
    'test/cases/extlib_brotli.xl','test/cases/extlib_brotli_progress.xl','release/examples/extlibs/brotli.xl',
    'release/examples/extlibs/brotli_basic.xl','release/examples/extlibs/brotli_basic.expected.txt',
    'release/extlibs/brotli/README.md','release/extlibs/brotli/docs/site.html')
$paths += Get-ChildItem (Join-Path $Root 'release/extlibs/brotli') -Recurse -File |
    ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }
$paths += Get-ChildItem (Join-Path $Root 'release/include') -Recurse -File |
    ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }
$paths = @($paths | ForEach-Object { Join-Path $Root $_ })
$paths += @('xl.exe','xl.dll','xlc.dll') | ForEach-Object { Join-Path $RuntimeDirectory $_ }
$paths += @($gcc,(Get-Process -Id $PID).Path)
$includes = @('-Ilib/xrt','-Ilib/xruntime')
if ($XrtInclude) {
    $XrtInclude = (Resolve-Path -LiteralPath $XrtInclude).Path
    $includes = @("-I$XrtInclude")
    $paths += Join-Path $XrtInclude 'xrt.h'
    $paths += Join-Path $XrtInclude 'xruntime.h'
}
foreach ($path in @($paths | Sort-Object -Unique)) {
    $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash
}

function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 120) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds $environment
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[brotli] ${Name}: PASS"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror',
    '-Irelease/extlibs/brotli/third_party/include') + $includes
$packageFlags = $flags + @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$objects = @()
$coreObjects = @()
$runtime = Join-Path $work 'runtime.o'
$outcome = 'failed'
$failure = ''
try {
    foreach ($order in @('ALONE','CORE_FIRST','NATIVE_FIRST','XRT_FIRST')) {
        $null = Check "native-header-$order" $gcc ($flags + @("-DBROTLI_$order",'-fsyntax-only',
            'release/extlibs/brotli/test/native_header.c'))
    }
    foreach ($kind in @('compressor','decompressor')) {
        $name = "private_${kind}_negative"
        $args = @('check',"release/extlibs/brotli/test/$name.xl")
        $result = Invoke-ExtlibSmokeProcess $xl $args $Root 90 $environment
        $checks.Add([ordered]@{name=$name;program=$xl;arguments=$args;status=$result.status;exitCode=$result.exitCode;elapsedMs=$result.elapsedMs;stdout=$result.stdout;stderr=$result.stderr})
        if ($result.status -ne 'fail' -or
            !$result.stderr.Contains('updateNative') -or
            !$result.stderr.Contains('access')) {
            throw "$name did not report private access"
        }
        Write-Host "[brotli] ${name}: rejected as expected"
    }
    $null = Check 'runtime' $gcc ($flags + @('-DXRT_MODULE_DIR','-DXRT_MODULE_FILE_WHOLE','-c','test/support/extlib_codec_runtime.c','-o',$runtime))
    $index = 0
    foreach ($source in $sources) {
        $object = Join-Path $work "source-$index.o"
        $null = Check "native-$index" $gcc ($packageFlags + @('-c',$source,'-o',$object))
        $objects += $object
        if ($source -ne 'release/extlibs/brotli/brotli_native.c') { $coreObjects += $object }
        ++$index
    }
    $links = @($runtime,'-lWs2_32','-lAdvapi32','-lm')
    $pureRuntime = Join-Path $work 'xrt-file-runtime.o'
    $null = Check 'pure-c-runtime' $gcc ($flags + @('-c','release/extlibs/brotli/test/xrt_file_runtime.c','-o',$pureRuntime))
    $pureLinks = @($pureRuntime,'-lWs2_32','-lAdvapi32','-lm')
    foreach ($name in @('files_c','file_rollback_open')) {
        $binary = Join-Path $work ($name+'.exe')
        $null = Check "$name-build" $gcc ($flags + @('-DNDEBUG',"release/extlibs/brotli/test/$name.c") + $coreObjects + $pureLinks + @('-o',$binary))
        $null = Check $name $binary @()
    }
    $test = 'release/extlibs/brotli/test/test_brotli.c'
    $exe = Join-Path $work 'brotli-c.exe'
    $null = Check 'c-build' $gcc ($packageFlags + @('-DNDEBUG',$test) + $objects + $links + @('-o',$exe))
    $null = Check 'c-boundaries' $exe @()
    $allocation = Join-Path $work 'brotli-alloc.exe'
    $null = Check 'allocation-build' $gcc ($packageFlags + @('-DNDEBUG','release/extlibs/brotli/test/test_brotli_alloc.c') + $objects + $links + @('-o',$allocation))
    $null = Check 'allocation' $allocation @()
    $example = 'release/extlibs/brotli/examples/basic.c'
    $releaseExample = Join-Path $work 'brotli-c-example-ndebug.exe'
    $null = Check 'c-example-ndebug-build' $gcc ($packageFlags + @('-DNDEBUG',$example) + $objects + $links + @('-o',$releaseExample))
    $releaseOutput = Check 'c-example-ndebug' $releaseExample @()
    $releaseGolden = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/brotli_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($releaseOutput -cne $releaseGolden) { throw 'Brotli -DNDEBUG C example output differs from golden' }
    $null = Check 'c-jit' $xl (@('run',$test) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime'))
    foreach ($name in @('extlib_brotli','extlib_brotli_progress')) {
        $case = "test/cases/$name.xl"
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-jit" $xl @('run',$case)
        if (!$SkipAot) {
            $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
            $null = Check "$name-aot" $binary @()
        }
    }
    $errorCase = 'release/extlibs/brotli/test/error_dispatch.xl'
    $null = Check 'error-dispatch-jit' $xl @('run',$errorCase)
    if (!$SkipAot) {
        $errorBinary = Join-Path $work 'error-dispatch.exe'
        $null = Check 'error-dispatch-build' $xl @('build',$errorCase,'-o',$errorBinary)
        $null = Check 'error-dispatch-aot' $errorBinary @()
    }
    $null = Check 'external-wire-fixture-xlang' $xl @('run','release/extlibs/brotli/test/wire_fixture.xl')
    $decls = @()
    foreach ($name in @('extlib_brotli','extlib_brotli_progress')) {
        $module = Join-Path $work "$name.c"
        $null = Check "$name-cgen" $xl @('cgen',"test/cases/$name.xl",'--emit',$module)
        $decls += @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxbrotli_[a-z_]+\(' })
    }
    $decls = @($decls | Sort-Object -Unique)
    $source = Get-Content (Join-Path $Root 'release/extlibs/brotli/src/brotli.xl') -Raw -Encoding UTF8
    $expected = @([regex]::Matches($source,'extern "(xbrotli_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxbrotli_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -ne 0 -or $decls.Count -ne $expected.Count) {
        throw "Brotli generated ABI coverage incomplete: expected $($expected.Count), got $($decls.Count)"
    }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include <stdint.h>`n#include `"brotli_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Check 'generated-prototypes' $gcc ($packageFlags + @('-Irelease/extlibs/brotli/include','-fsyntax-only',$abi))
    foreach ($name in @('brotli','brotli_basic')) {
        $case = "release/examples/extlibs/$name.xl"
        $text = Check "$name-example-jit" $xl @('run',$case)
        if ($name -eq 'brotli_basic') { $xlangOutput = $text }
        $binary = Join-Path $work "$name-example.exe"
        if (!$SkipAot) {
            $null = Check "$name-example-build" $xl @('build',$case,'-o',$binary)
            $aot = Check "$name-example-aot" $binary @()
            if ($aot -cne $text) { throw "$name JIT/AOT example output differs" }
        }
    }
    $example = 'release/extlibs/brotli/examples/basic.c'
    $binary = Join-Path $work 'brotli-c-example.exe'
    $null = Check 'c-example-build' $gcc ($packageFlags + @($example) + $objects + $links + @('-o',$binary))
    $cOutput = Check 'c-example' $binary @()
    $jitOutput = Check 'c-example-jit' $xl (@('run',$example) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime'))
    $binary = Join-Path $work 'brotli-c-example-tcc.exe'
    if (!$SkipAot) {
        $null = Check 'c-example-aot-build' $xl (@('build',$example) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','-o',$binary))
        $cAot = Check 'c-example-aot' $binary @()
    }
    $helperOutput = Check 'c-example-package-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','brotli','-Source',$example,'-RuntimeDirectory',$RuntimeDirectory)
    $expectedOutput = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/brotli_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($cOutput -cne $expectedOutput -or $jitOutput -cne $expectedOutput -or $xlangOutput -cne $expectedOutput -or
        (!$SkipAot -and $cAot -cne $expectedOutput) -or $helperOutput -cne $expectedOutput) {
        throw 'C/xlang examples differ from the common golden output'
    }
    $binary = Join-Path $work 'file-allocation.exe'
    $null = Check 'file-allocation-build' $gcc ($packageFlags + @('-DNDEBUG','release/extlibs/brotli/test/file_allocation.c') + $objects + $links + @('-o',$binary))
    $null = Check 'file-allocation' $binary @((Join-Path $work ('files-'+[Guid]::NewGuid().ToString('N'))))
    $null = Check 'file-allocation-jit' $xl (@('run','release/extlibs/brotli/test/file_allocation.c') + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','--',(Join-Path $work ('jit-files-'+[Guid]::NewGuid().ToString('N')))))
    $binary = Join-Path $work 'file-allocation-aot.exe'
    if (!$SkipAot) {
        $null = Check 'file-allocation-aot-build' $xl (@('build','release/extlibs/brotli/test/file_allocation.c') + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','-o',$binary))
        $null = Check 'file-allocation-aot' $binary @((Join-Path $work ('aot-files-'+[Guid]::NewGuid().ToString('N'))))
    }
    $golden = [IO.File]::ReadAllText((Join-Path $Root 'release/extlibs/brotli/examples/files.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $filesExample = 'release/extlibs/brotli/examples/files_c.c'
    $binary = Join-Path $work 'files-example.exe'
    $null = Check 'files-example-c-build' $gcc ($flags + @('-DNDEBUG',$filesExample) + $coreObjects + $pureLinks + @('-o',$binary))
    $cText = Check 'files-example-c' $binary @()
    $direct = Check 'files-example-c-jit' $xl (@('run',$filesExample) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime'))
    $xlText = Check 'files-example-xl' $xl @('run','release/extlibs/brotli/examples/files.xl')
    if ($cText -cne $golden -or $direct -cne $golden -or $xlText -cne $golden) { throw 'Brotli file example golden output differs' }
    $fileTest = 'release/extlibs/brotli/test/files_c.c'
    $null = Check 'files-c-jit' $xl (@('run',$fileTest) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime'))
    if (!$SkipAot) {
        foreach ($source in @($filesExample,$fileTest)) {
            $binary = Join-Path $work (([IO.Path]::GetFileNameWithoutExtension($source))+'-aot.exe')
            if ($source -eq $fileTest) { $binary = Join-Path $work 'files-test-aot.exe' }
            $null = Check "$source-build" $xl (@('build',$source) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','-o',$binary))
            $text = Check "$source-aot" $binary @()
            if ($source -eq $filesExample -and $text -cne $golden) { throw 'Brotli C file AOT example differs' }
        }
        $binary = Join-Path $work 'files-xl-aot.exe'
        $null = Check 'files-example-xl-build' $xl @('build','release/extlibs/brotli/examples/files.xl','-o',$binary)
        if ((Check 'files-example-xl-aot' $binary @()) -cne $golden) { throw 'Brotli XL file AOT example differs' }
    }
    $binary = Join-Path $work 'rollback-closed.exe'
    $null = Check 'rollback-closed-build' $gcc ($flags + @('-DNDEBUG','release/extlibs/brotli/test/file_rollback_closed.c') + $coreObjects + $pureLinks + @('-o',$binary))
    $closedFailure = $null
    try { $null = Check 'rollback-closed' $binary @((Join-Path $work 'closed-files')) }
    catch { $closedFailure = $_; Write-Warning 'Brotli closed-transaction rollback failed; continue to audit global lifecycle' }
    # 顶层真实对象不能通过搬进 main 掩盖退出期的原生析构边界。
    $topLevel = 'release/extlibs/brotli/test/top_level.xl'
    $null = Check 'top-level-jit' $xl @('run',$topLevel)
    if (!$SkipAot) {
        $topBinary = Join-Path $work 'top-level.exe'
        $null = Check 'top-level-build' $xl @('build',$topLevel,'-o',$topBinary)
        $null = Check 'top-level-aot' $topBinary @()
    }
    if ($closedFailure) { throw $closedFailure }
    $outcome = 'passed'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $hashes[$_] })
    $changed += @(Get-ChildItem (Join-Path $Root 'release/extlibs/brotli') -Recurse -File | Where-Object { !$hashes.Contains($_.FullName) } | ForEach-Object FullName)
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;scope='Windows C interop, allocation failures, xlang/C JIT/AOT, callbacks and transactional files, generated prototypes, tutorials and paired golden output';cRuntimeOverride=$XrtInclude;runtimeDirectory=$RuntimeDirectory;environment=$environment;runtimeNote='XrtInclude overrides independent GCC C only. XL JIT/AOT use RuntimeDirectory and its embedded runtime.';inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($report | ConvertTo-Json -Depth 8),$utf8)
    if ($changed.Count) { throw 'Inputs changed during brotli gate' }
}
