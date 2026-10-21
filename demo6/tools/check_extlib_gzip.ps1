param([string]$Root = '', [string]$XrtInclude = '', [string]$WorkDirectory = '', [string]$RuntimeDirectory = '')

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'release' }
$RuntimeDirectory = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$xl = Join-Path $RuntimeDirectory 'xl.exe'
$environment = @{XLANG_PACKAGE_PATH = Join-Path $Root 'release/extlibs'}
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$work = if (!$WorkDirectory) { Join-Path $Root 'build/test/gzip_guard' } elseif ([IO.Path]::IsPathRooted($WorkDirectory)) { [IO.Path]::GetFullPath($WorkDirectory) } else { [IO.Path]::GetFullPath((Join-Path $Root $WorkDirectory)) }
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$manifest = Get-Content (Join-Path $Root 'release/extlibs/gzip/xlang.package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/gzip/$_" })
$paths = @('lib/xrt/xrt.h','lib/xruntime/xruntime.h',
    'release/include/xrt_decl.h','release/include/xruntime_decl.h',
    'test/support/extlib_codec_runtime.c','test/cases/extlib_codec_file_allocation.c','tools/check_extlib_gzip.ps1','tools/check_extlib_gzip_interop.py',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
    'test/phase1_extlibs.json','test/suites/12_phase1_extlibs.bat','tools/check_extlib_gzip.sh',
    'test/cases/extlib_gzip.xl','test/cases/extlib_gzip_progress.xl','release/examples/extlibs/gzip.xl',
    'release/examples/extlibs/gzip_basic.xl','release/examples/extlibs/gzip_basic.expected.txt',
    'release/examples/extlibs/gzip_c.c',
    'release/extlibs/gzip/README.md','release/extlibs/gzip/docs/site.html')
$paths += Get-ChildItem (Join-Path $Root 'release/extlibs/gzip') -Recurse -File |
    Where-Object Extension -in @('.c','.h','.xl','.json') |
    ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }
$paths = @($paths | ForEach-Object { Join-Path $Root $_ })
$paths += @('xl.exe','xl.dll','xlc.dll') | ForEach-Object { Join-Path $RuntimeDirectory $_ }
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
    Write-Host "[gzip] ${Name}: PASS"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror') + $includes
$packageFlags = $flags + @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$objects = @()
$runtime = Join-Path $work 'runtime.o'
$outcome = 'failed'
$failure = ''
try {
    foreach ($kind in @('compressor','decompressor')) {
        $name = "private_${kind}_negative"
        $args = @('check',"release/extlibs/gzip/test/$name.xl")
        $result = Invoke-ExtlibSmokeProcess $xl $args $Root 90 $environment
        $checks.Add([ordered]@{name=$name;program=$xl;arguments=$args;status=$result.status;exitCode=$result.exitCode;elapsedMs=$result.elapsedMs;stdout=$result.stdout;stderr=$result.stderr})
        if ($result.status -ne 'fail' -or
            !$result.stderr.Contains('updateNative') -or
            !$result.stderr.Contains('access')) {
            throw "$name did not report private access"
        }
        Write-Host "[gzip] ${name}: rejected as expected"
    }
    $null = Check 'runtime' $gcc ($flags + @('-DXRT_MODULE_DEFLATE','-DXRT_MODULE_INFLATE','-DXRT_MODULE_DIR','-DXRT_MODULE_FILE_WHOLE','-c','test/support/extlib_codec_runtime.c','-o',$runtime))
    $index = 0
    foreach ($source in $sources) {
        $object = Join-Path $work "source-$index.o"
        $null = Check "native-$index" $gcc ($packageFlags + @('-c',$source,'-o',$object))
        $objects += $object
        ++$index
    }
    $links = @($runtime,'-lWs2_32','-lAdvapi32','-lm')
    $test = 'release/extlibs/gzip/test/test_gzip.c'
    $exe = Join-Path $work 'gzip-c.exe'
    $null = Check 'c-build' $gcc ($packageFlags + @('-DNDEBUG',$test) + $objects + $links + @('-o',$exe))
    $null = Check 'c-boundaries' $exe @()
    $stream = Join-Path $work 'gzip-stream.exe'
    $streamTest = 'release/extlibs/gzip/test/test_gzip_stream.c'
    $null = Check 'stream-build' $gcc ($packageFlags + @('-DNDEBUG',$streamTest) + $objects + $links + @('-o',$stream))
    $null = Check 'stream-boundaries' $stream @()
    $null = Check 'stream-jit' $xl (@('run',$streamTest) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime'))
    $interop = Join-Path $work 'gzip-interop.exe'
    $null = Check 'interop-build' $gcc ($packageFlags + @('release/extlibs/gzip/test/gzip_interop.c') + $objects + $links + @('-o',$interop))
    $python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    $null = Check 'python-zlib-interop' $python @('tools/check_extlib_gzip_interop.py',$interop,(Join-Path $work 'interop'))
    $allocation = Join-Path $work 'gzip-alloc.exe'
    $null = Check 'allocation-build' $gcc ($packageFlags + @('-DNDEBUG','release/extlibs/gzip/test/test_gzip_alloc.c') + $objects + $links + @('-o',$allocation))
    $null = Check 'allocation' $allocation @()
    $null = Check 'c-jit' $xl (@('run',$test) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime'))
    $fileTest = Join-Path $work 'file-allocation.exe'
    $null = Check 'file-allocation-build' $gcc ($packageFlags + @('-DNDEBUG','-Itest/cases','release/extlibs/gzip/test/file_allocation_dll.c') + $objects + $links + @('-o',$fileTest))
    $null = Check 'file-allocation' $fileTest @((Join-Path $work ('files-'+[Guid]::NewGuid().ToString('N'))))
    $probe = Join-Path $work 'file-allocation.c'
    $fixture = (Join-Path $Root 'test/cases/extlib_codec_file_allocation.c').Replace('\','/')
    [IO.File]::WriteAllText($probe,"#define TEST_CODEC_GZIP`n#include `"$fixture`"`n",$utf8)
    $null = Check 'file-allocation-jit' $xl (@('run',$probe) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','--',(Join-Path $work ('jit-files-'+[Guid]::NewGuid().ToString('N')))))
    $null = Check 'file-ops-jit' $xl @('run','release/extlibs/gzip/test/file_ops.xl')
    $fileAot = Join-Path $work 'file-allocation-aot.exe'
    $null = Check 'file-allocation-aot-build' $xl (@('build',$probe) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','-o',$fileAot))
    $null = Check 'file-allocation-aot' $fileAot @((Join-Path $work ('aot-files-'+[Guid]::NewGuid().ToString('N'))))
    $central = 'release/examples/extlibs/gzip_c.c'
    $centralOutput = Check 'central-c-jit' $xl @('run',$central)
    $binary = Join-Path $work 'gzip-central-c.exe'
    $null = Check 'central-c-build' $xl @('build',$central,'-o',$binary)
    $centralAot = Check 'central-c-aot' $binary @()
    if ($centralOutput -cne 'gzip C example: PASS' -or $centralAot -cne $centralOutput) {
        throw 'central gzip C example output mismatch'
    }
    foreach ($name in @('extlib_gzip','extlib_gzip_progress')) {
        $case = "test/cases/$name.xl"
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-jit" $xl @('run',$case)
        $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
        $null = Check "$name-aot" $binary @()
    }
    $decls = @()
    foreach ($name in @('extlib_gzip','extlib_gzip_progress')) {
        $module = Join-Path $work "$name.c"
        $null = Check "$name-cgen" $xl @('cgen',"test/cases/$name.xl",'--emit',$module)
        $decls += @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxgzip_[a-z_]+\(' })
    }
    $decls = @($decls | Sort-Object -Unique)
    $source = Get-Content (Join-Path $Root 'release/extlibs/gzip/src/gzip.xl') -Raw -Encoding UTF8
    $expected = @([regex]::Matches($source,'extern "(xgzip_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxgzip_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -ne 0 -or $decls.Count -ne $expected.Count) {
        throw "Gzip generated ABI coverage incomplete: expected $($expected.Count), got $($decls.Count)"
    }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include <stdint.h>`n#include `"gzip_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Check 'generated-prototypes' $gcc ($packageFlags + @('-Irelease/extlibs/gzip/include','-fsyntax-only',$abi))
    foreach ($name in @('gzip','gzip_basic')) {
        $case = "release/examples/extlibs/$name.xl"
        $text = Check "$name-example-jit" $xl @('run',$case)
        if ($name -eq 'gzip_basic') { $xlangOutput = $text }
        $binary = Join-Path $work "$name-example.exe"
        $null = Check "$name-example-build" $xl @('build',$case,'-o',$binary)
        $aot = Check "$name-example-aot" $binary @()
        if ($aot -cne $text) { throw "$name JIT/AOT example output differs" }
    }
    $example = 'release/extlibs/gzip/examples/basic.c'
    $binary = Join-Path $work 'gzip-c-example.exe'
    $null = Check 'c-example-build' $gcc ($packageFlags + @($example) + $objects + $links + @('-o',$binary))
    $cOutput = Check 'c-example' $binary @()
    $jitOutput = Check 'c-example-jit' $xl @('run',$example)
    $binary = Join-Path $work 'gzip-c-example-tcc.exe'
    $null = Check 'c-example-aot-build' $xl @('build',$example,'-o',$binary)
    $cAot = Check 'c-example-aot' $binary @()
    $helperOutput = Check 'c-example-package-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','gzip','-Source',$example,'-RuntimeDirectory',$RuntimeDirectory)
    $expectedOutput = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/gzip_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($cOutput -cne $expectedOutput -or $jitOutput -cne $expectedOutput -or $xlangOutput -cne $expectedOutput -or
        $cAot -cne $expectedOutput -or $helperOutput -cne $expectedOutput) {
        throw 'C/xlang examples differ from the common golden output'
    }
    $outcome = 'passed'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $hashes[$_] })
    $changed += @(Get-ChildItem (Join-Path $Root 'release/extlibs/gzip') -Recurse -File | Where-Object { $_.Extension -in @('.c','.h','.xl','.json') -and !$hashes.Contains($_.FullName) } | ForEach-Object FullName)
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;scope='Windows C/zlib interop, allocation failures, xlang/C JIT/AOT, callbacks and transactional files, generated prototype coverage, tutorials and paired golden output';cRuntimeOverride=$XrtInclude;runtimeDirectory=$RuntimeDirectory;environment=$environment;runtimeNote='XrtInclude overrides independent GCC C only. XL JIT/AOT use RuntimeDirectory and its embedded runtime.';inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($report | ConvertTo-Json -Depth 8),$utf8)
    if ($changed.Count) { throw 'Inputs changed during gzip gate' }
}
