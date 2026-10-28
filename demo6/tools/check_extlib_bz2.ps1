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
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$work = if (!$WorkDirectory) { Join-Path $Root 'build/test/bz2_guard' } elseif ([IO.Path]::IsPathRooted($WorkDirectory)) { [IO.Path]::GetFullPath($WorkDirectory) } else { [IO.Path]::GetFullPath((Join-Path $Root $WorkDirectory)) }
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$manifest = Get-Content (Join-Path $Root 'release/extlibs/bz2/xlang.package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$vendor = Get-Content (Join-Path $Root 'release/extlibs/_bzip2/xlang.package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/bz2/$_" })
$vendorSources = @($vendor.artifact.cSources | ForEach-Object { "release/extlibs/_bzip2/$_" })
$includes = @('-Ilib/xrt','-Ilib/xruntime')
if ($XrtInclude) {
    $XrtInclude = (Resolve-Path -LiteralPath $XrtInclude).Path
    $includes = @("-I$XrtInclude")
}

function InputPaths {
    foreach ($pkg in @('bz2','_bzip2')) {
        Get-ChildItem (Join-Path $Root "release/extlibs/$pkg") -Recurse -File | ForEach-Object FullName
    }
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h',
        'release/include/xrt_decl.h','release/include/xruntime_decl.h','release/include/xl_extlib_abi.h',
        'test/support/extlib_codec_runtime.c','tools/check_extlib_bz2.ps1','tools/check_extlib_bz2.sh',
        'tools/check_extlib_bz2_interop.py','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
        'tools/run_extlib_c.ps1','test/featured_extlibs.json','test/suites/11_featured_extlibs_rc.bat',
        'test/cases/extlib_bz2.xl','test/cases/extlib_bz2_progress.xl',
        'release/examples/extlibs/bz2.xl','release/examples/extlibs/bz2_basic.xl',
        'release/examples/extlibs/bz2_files.xl','release/examples/extlibs/bz2_basic.expected.txt',
        'release/examples/extlibs/bz2_files.expected.txt',
        'release/examples/extlibs/bz2_c.c')) { Join-Path $Root $path }
    foreach ($name in @('xl.exe','xl.dll','xlc.dll')) { Join-Path $RuntimeDirectory $name }
    if ($XrtInclude) { Join-Path $XrtInclude 'xrt.h'; Join-Path $XrtInclude 'xruntime.h' }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 120) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds $environment
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[bz2] ${Name}: PASS"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror') + $includes
$objects = @()
$outcome = 'failed'
$failure = ''
$changed = @()
try {
    foreach ($kind in @('compressor','decompressor')) {
        $name = "private_${kind}_negative"
        $args = @('check',"release/extlibs/bz2/test/$name.xl")
        $result = Invoke-ExtlibSmokeProcess $xl $args $Root 90 $environment
        $checks.Add([ordered]@{name=$name;program=$xl;arguments=$args;status=$result.status;exitCode=$result.exitCode;elapsedMs=$result.elapsedMs;stdout=$result.stdout;stderr=$result.stderr})
        if ($result.status -ne 'fail' -or
            !$result.stderr.Contains('updateNative') -or
            !$result.stderr.Contains('access')) {
            throw "$name did not report private access"
        }
        Write-Host "[bz2] ${name}: rejected as expected"
    }
    $null = Check 'compiler-version' $gcc @('--version')
    foreach ($order in @(@(), @('-DBZ2_CORE_FIRST'), @('-DBZ2_NATIVE_FIRST'), @('-DBZ2_XRT_FIRST'))) {
        $null = Check 'native-header-order' $gcc ($flags + $order + @('-fsyntax-only',
            'release/extlibs/bz2/test/native_header.c'))
    }
    $runtime = Join-Path $work 'runtime.o'
    $null = Check 'runtime' $gcc ($flags + @('-DXRT_MODULE_DIR','-DXRT_MODULE_DIR_TEMP','-DXRT_MODULE_FILE_WHOLE','-DXRT_MODULE_PATH','-c','test/support/extlib_codec_runtime.c','-o',$runtime))
    $objects += $runtime
    $index = 0
    foreach ($source in $sources + $vendorSources) {
        $object = Join-Path $work "source-$index.o"
        $ownFlags = if ($source -in $sources) { $flags } else { @('-std=c11','-O2') }
        $null = Check "source-$index" $gcc ($ownFlags + @('-c',$source,'-o',$object))
        $objects += $object
        ++$index
    }
    $links = @('-lWs2_32','-lAdvapi32','-lm')
    foreach ($name in @('test_bz2','test_bz2_boundaries','test_bz2_stream','test_bz2_alloc')) {
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-build" $gcc ($flags + @('-DNDEBUG',"release/extlibs/bz2/test/$name.c") + $objects + $links + @('-o',$binary))
        $null = Check $name $binary @()
    }
    $interop = Join-Path $work 'interop.exe'
    $null = Check 'interop-build' $gcc ($flags + @('release/extlibs/bz2/test/bz2_interop.c') + $objects + $links + @('-o',$interop))
    $null = Check 'python-bz2-interop' $python @('tools/check_extlib_bz2_interop.py',$interop,(Join-Path $work 'interop-files')) 180
    $fileTest = Join-Path $work 'files.exe'
    $null = Check 'file-boundaries-build' $gcc ($flags + @('-DNDEBUG','release/extlibs/bz2/test/test_bz2_files.c') + $objects + $links + @('-o',$fileTest))
    $null = Check 'file-boundaries' $fileTest @((Join-Path $work ('files-'+[Guid]::NewGuid().ToString('N'))))
    $fileProbe = 'release/extlibs/bz2/test/test_bz2_files.c'
    $null = Check 'file-boundaries-jit' $xl (@('run',$fileProbe) + $sources + $vendorSources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','--',(Join-Path $work ('jit-files-'+[Guid]::NewGuid().ToString('N')))))
    $null = Check 'file-ops-jit' $xl @('run','release/extlibs/bz2/test/file_ops.xl')
    $null = Check 'text-transfer-jit' $xl @('run','release/extlibs/bz2/test/text_transfer.xl')
    if (!$SkipAot) {
        $textAot = Join-Path $work 'text-transfer-aot.exe'
        $null = Check 'text-transfer-aot-build' $xl @('build','release/extlibs/bz2/test/text_transfer.xl','-o',$textAot)
        $null = Check 'text-transfer-aot' $textAot @()
        $fileAot = Join-Path $work 'file-boundaries-aot.exe'
        $null = Check 'file-boundaries-aot-build' $xl (@('build',$fileProbe) + $sources + $vendorSources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','-o',$fileAot))
        $null = Check 'file-boundaries-aot' $fileAot @((Join-Path $work ('aot-files-'+[Guid]::NewGuid().ToString('N'))))
    }
    $central = 'release/examples/extlibs/bz2_c.c'
    $centralOutput = Check 'central-c-jit' $xl @('run',$central)
    if (!$SkipAot) {
        $binary = Join-Path $work 'bz2-central-c.exe'
        $null = Check 'central-c-build' $xl @('build',$central,'-o',$binary)
        $centralAot = Check 'central-c-aot' $binary @()
    }
    if ($centralOutput -cne 'bz2 C example: PASS' -or (!$SkipAot -and $centralAot -cne $centralOutput)) {
        throw 'central bz2 C example output mismatch'
    }
    foreach ($name in @('extlib_bz2','extlib_bz2_progress')) {
        $case = "test/cases/$name.xl"
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-jit" $xl @('run',$case)
        if (!$SkipAot) {
            $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
            $null = Check "$name-aot" $binary @()
        }
    }
    $decls = @()
    foreach ($name in @('extlib_bz2','extlib_bz2_progress')) {
        $module = Join-Path $work "$name.c"
        $null = Check "$name-cgen" $xl @('cgen',"test/cases/$name.xl",'--emit',$module)
        $decls += @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxbz2_[a-z_]+\(' })
    }
    $decls = @($decls | Sort-Object -Unique)
    $source = Get-Content (Join-Path $Root 'release/extlibs/bz2/src/bz2.xl') -Raw -Encoding UTF8
    $expected = @([regex]::Matches($source,'extern "(xbz2_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxbz2_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -ne 0 -or $decls.Count -ne $expected.Count) { throw 'Incomplete generated ABI coverage' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include <stdint.h>`n#include `"bz2_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Check "generated-$($expected.Count)-prototypes" $gcc ($flags + @('-Irelease/extlibs/bz2/include','-fsyntax-only',$abi))
    foreach ($name in @('bz2','bz2_basic','bz2_files')) {
        $case = "release/examples/extlibs/$name.xl"
        $text = Check "$name-example-jit" $xl @('run',$case)
        if (!$SkipAot) {
            $binary = Join-Path $work "$name-example.exe"
            $null = Check "$name-example-build" $xl @('build',$case,'-o',$binary)
            $aot = Check "$name-example-aot" $binary @()
            if ($aot -cne $text) { throw "$name JIT/AOT output mismatch" }
        }
        if ($name -eq 'bz2') { continue }
        $short = $name.Substring(4)
        $golden = [IO.File]::ReadAllText((Join-Path $Root "release/examples/extlibs/$name.expected.txt")).Replace("`r`n","`n").TrimEnd("`n")
        if ($text -cne $golden) { throw "$name golden output mismatch" }
        $c = "release/extlibs/bz2/examples/$short.c"
        $binary = Join-Path $work "$name-c.exe"
        $null = Check "$name-c-build" $gcc ($flags + @($c) + $objects + $links + @('-o',$binary))
        $cOutput = Check "$name-c" $binary @()
        $jit = Check "$name-c-jit" $xl @('run',$c)
        if (!$SkipAot) {
            $binary = Join-Path $work "$name-c-tcc.exe"
            $null = Check "$name-c-aot-build" $xl @('build',$c,'-o',$binary)
            $cAot = Check "$name-c-aot" $binary @()
        }
        $helper = Check "$name-c-package-runner" (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','bz2','-Source',$c,'-RuntimeDirectory',$RuntimeDirectory)
        if ($cOutput -cne $golden -or $jit -cne $golden -or $helper -cne $golden -or
            (!$SkipAot -and $cAot -cne $golden)) { throw "$name C/XL output mismatch" }
    }
    $outcome = 'passed'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $hashes[$_] })
    $changed += @(InputPaths | Where-Object { !$hashes.Contains($_) })
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $scope = 'Windows strict C, independent Python bz2 interop, allocation, IO/reentry, transactions, generated ABI, XL/C JIT and paired examples'
    if ($SkipAot) { $scope += '; AOT skipped' } else { $scope += ', AOT' }
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;scope=$scope;cRuntimeOverride=$XrtInclude;runtimeDirectory=$RuntimeDirectory;environment=$environment;runtimeNote='XrtInclude overrides independent GCC C only. XL JIT/AOT use RuntimeDirectory and its embedded runtime.';inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($report | ConvertTo-Json -Depth 8),$utf8)
    if ($changed.Count) { throw 'Inputs changed during bz2 gate' }
}
