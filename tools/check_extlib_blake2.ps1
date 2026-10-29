param([string]$Root = '', [switch]$SkipAot)

$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $Root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$workRoot = Join-Path $Root 'build/test/blake2_guard'
$runId = [guid]::NewGuid().ToString('N')
$work = Join-Path $workRoot ('run_' + $runId)
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$manifest = Get-Content (Join-Path $Root 'release/extlibs/blake2/xlang.package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/blake2/$_" })
$paths = @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
    'release/include/xrt_decl.h','release/include/xruntime_decl.h',
    'test/support/extlib_codec_runtime.c','tools/check_extlib_blake2.ps1',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
    'test/featured_extlibs.json','test/suites/11_featured_extlibs_rc.bat','tools/check_extlib_blake2.sh',
    'test/cases/extlib_blake2.xl','test/cases/extlib_blake2_boundaries.xl','test/cases/extlib_blake2_progress.xl','release/examples/extlibs/blake2.xl',
    'release/examples/extlibs/blake2_basic.xl','release/examples/extlibs/blake2_basic.expected.txt')
$paths += Get-ChildItem (Join-Path $Root 'release/extlibs/blake2') -Recurse -File |
    Where-Object Extension -in @('.c','.h','.xl','.json') |
    ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }
foreach ($path in @($paths | Sort-Object -Unique)) {
    $hashes[$path] = (Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash
}

function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 120) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds
    $checks.Add([ordered]@{name=$Name;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[blake2] ${Name}: PASS"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Ilib/xruntime')
$packageFlags = $flags + @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$objects = @()
$runtime = Join-Path $work 'runtime.o'
$outcome = 'failed'
try {
    $null = Check 'runtime' $gcc ($flags + @('-DXRT_MODULE_CODEC_HEX','-DXRT_MODULE_THREAD','-c','test/support/extlib_codec_runtime.c','-o',$runtime))
    $index = 0
    foreach ($source in $sources) {
        $object = Join-Path $work "source-$index.o"
        $null = Check "native-$index" $gcc ($packageFlags + @('-c',$source,'-o',$object))
        $objects += $object
        ++$index
    }
    $links = @($runtime,'-lWs2_32','-lAdvapi32','-lm')
    $test = 'release/extlibs/blake2/test/test_blake2.c'
    $exe = Join-Path $work 'blake2-c.exe'
    $null = Check 'c-build' $gcc ($packageFlags + @('-DNDEBUG',$test) + $objects + $links + @('-o',$exe))
    $null = Check 'c-boundaries' $exe @()
    $allocation = Join-Path $work 'blake2-alloc.exe'
    $null = Check 'allocation-build' $gcc ($packageFlags + @('-DNDEBUG','release/extlibs/blake2/test/test_blake2_alloc.c') + $objects + $links + @('-o',$allocation))
    $null = Check 'allocation' $allocation @()
    $native = Join-Path $work 'blake2-native.exe'
    $null = Check 'native-boundaries-build' $gcc ($packageFlags + @('-DNDEBUG','release/extlibs/blake2/test/test_blake2_native.c') + $objects + $links + @('-o',$native))
    $null = Check 'native-boundaries' $native @()
    $close = Join-Path $work 'blake2-close.exe'
    $null = Check 'close-build' $gcc ($packageFlags + @('-DNDEBUG','release/extlibs/blake2/test/test_blake2_close.c') + $objects[0..1] + $links + @('-o',$close))
    $null = Check 'close-boundaries' $close @()
    $example = 'release/extlibs/blake2/examples/basic.c'
    $releaseExample = Join-Path $work 'blake2-c-example-ndebug.exe'
    $null = Check 'c-example-ndebug-build' $gcc ($packageFlags + @('-DNDEBUG',$example) + $objects + $links + @('-o',$releaseExample))
    $releaseOutput = Check 'c-example-ndebug' $releaseExample @()
    $releaseGolden = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/blake2_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($releaseOutput -cne $releaseGolden) { throw 'BLAKE2 -DNDEBUG C example output differs from golden' }
    $directOutput = Check 'c-example-direct-jit' $xl @('run',$example)
    if ($directOutput -cne $releaseGolden) { throw 'Direct C example output differs from golden' }
    $null = Check 'c-jit' $xl (@('run',$test) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime'))
    foreach ($name in @('native_b_visibility_negative','native_b_keyed_visibility_negative',
        'native_s_visibility_negative','native_s_keyed_visibility_negative')) {
        $case = "release/extlibs/blake2/test/$name.xl"
        $result = Invoke-ExtlibSmokeProcess $xl @('check',$case) $Root 30
        $rejected = $result.status -eq 'fail' -and
            ($result.stdout + $result.stderr) -match 'no matching callable class member'
        $checks.Add([ordered]@{name=$name;status=$(if ($rejected) { 'pass' } else { 'fail' });exitCode=$result.exitCode;stdout=$result.stdout;stderr=$result.stderr})
        if (!$rejected) { throw "BLAKE2 native method visibility gate failed: $name" }
        Write-Host "[blake2] ${name}: PASS"
    }
    foreach ($name in @('keyed_b_pointer_length_negative','keyed_s_pointer_length_negative')) {
        $case = "release/extlibs/blake2/test/$name.xl"
        $result = Invoke-ExtlibSmokeProcess $xl @('check',$case) $Root 30
        $rejected = $result.status -eq 'fail' -and
            ($result.stdout + $result.stderr) -match '\[sem\].*create'
        $checks.Add([ordered]@{name=$name;status=$(if ($rejected) { 'pass' } else { 'fail' });exitCode=$result.exitCode;stdout=$result.stdout;stderr=$result.stderr})
        if (!$rejected) { throw "BLAKE2 raw keyed constructor remains callable: $name" }
        Write-Host "[blake2] ${name}: PASS"
    }
    foreach ($name in @('extlib_blake2','extlib_blake2_boundaries','extlib_blake2_progress')) {
        $case = "test/cases/$name.xl"
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-jit" $xl @('run',$case)
        if (!$SkipAot) {
            $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
            $null = Check "$name-aot" $binary @()
        }
    }
    $null = Check 'known-vectors-xlang' $xl @('run','release/extlibs/blake2/test/vectors.xl')
    $decls = @()
    foreach ($name in @('extlib_blake2','extlib_blake2_boundaries','extlib_blake2_progress')) {
        $module = Join-Path $work "$name.c"
        $null = Check "$name-cgen" $xl @('cgen',"test/cases/$name.xl",'--emit',$module)
        $decls += @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxblake2[bs]?_[a-z_]+\(' })
    }
    $decls = @($decls | Sort-Object -Unique)
    $source = Get-Content (Join-Path $Root 'release/extlibs/blake2/src/blake2.xl') -Raw -Encoding UTF8
    $expected = @([regex]::Matches($source,'extern "(xblake2[bs]?_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxblake2[bs]?_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -ne 0 -or $decls.Count -ne $expected.Count) {
        throw "BLAKE2 generated ABI coverage incomplete: expected $($expected.Count), got $($decls.Count)"
    }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include <stdint.h>`n#include `"blake2_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Check 'generated-prototypes' $gcc ($packageFlags + @('-Irelease/extlibs/blake2/include','-fsyntax-only',$abi))
    foreach ($name in @('blake2','blake2_basic')) {
        $case = "release/examples/extlibs/$name.xl"
        $text = Check "$name-example-jit" $xl @('run',$case)
        if ($name -eq 'blake2_basic') { $xlangOutput = $text }
        if (!$SkipAot) {
            $binary = Join-Path $work "$name-example.exe"
            $null = Check "$name-example-build" $xl @('build',$case,'-o',$binary)
            $aot = Check "$name-example-aot" $binary @()
            if ($aot -cne $text) { throw "$name JIT/AOT example output differs" }
        }
    }
    $example = 'release/extlibs/blake2/examples/basic.c'
    $binary = Join-Path $work 'blake2-c-example.exe'
    $null = Check 'c-example-build' $gcc ($packageFlags + @($example) + $objects + $links + @('-o',$binary))
    $cOutput = Check 'c-example' $binary @()
    $jitOutput = Check 'c-example-jit' $xl @('run',$example)
    if (!$SkipAot) {
        $binary = Join-Path $work 'blake2-c-example-tcc.exe'
        $null = Check 'c-example-aot-build' $xl @('build',$example,'-o',$binary)
        $cAot = Check 'c-example-aot' $binary @()
    }
    $helperOutput = Check 'c-example-package-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','blake2','-Source',$example)
    $expectedOutput = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/blake2_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($cOutput -cne $expectedOutput -or $jitOutput -cne $expectedOutput -or $xlangOutput -cne $expectedOutput -or
        (!$SkipAot -and $cAot -cne $expectedOutput) -or $helperOutput -cne $expectedOutput) {
        throw 'C/xlang examples differ from the common golden output'
    }
    # 导出类的私有句柄构造器不能被包外代码调用；只编译，不执行。
    $privateCase = 'release/extlibs/blake2/test/private_adopt_constructor_negative.xl'
    $privateResult = Invoke-ExtlibSmokeProcess $xl @('check',$privateCase) $Root 30
    $privateRejected = $privateResult.status -eq 'fail' -and
        ($privateResult.stdout + $privateResult.stderr) -match 'access|private'
    $checks.Add([ordered]@{name='private-adopt-constructor';status=$(if ($privateRejected) { 'pass' } else { 'fail' });
        exitCode=$privateResult.exitCode;stdout=$privateResult.stdout;stderr=$privateResult.stderr})
    if (!$privateRejected) {
        throw 'private native-handle constructor is callable outside blake2; see dev/extlib_blockers/blake2_private_adopt_constructor.md'
    }
    Write-Host '[blake2] private-adopt-constructor: PASS'
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash -ne $hashes[$path]) { throw "Input changed: $path" }
    }
    $outcome = 'passed'
} finally {
    $scope = 'Windows C boundaries, 216 known answers, allocation failures, xlang/C JIT, callbacks and readers, generated ABI and paired examples'
    if (!$SkipAot) { $scope += ', Windows AOT' } else { $scope += '; AOT skipped' }
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;scope=$scope;generatedPrototypes=$decls.Count;inputs=$hashes;checks=@($checks.ToArray())}
    try {
        [IO.File]::WriteAllText((Join-Path $workRoot ('results_' + $runId + '.json')),
            ($report | ConvertTo-Json -Depth 8),$utf8)
    } finally {
        $parentPath = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
        $workPath = [IO.Path]::GetFullPath($work)
        if (-not $workPath.StartsWith($parentPath, [StringComparison]::OrdinalIgnoreCase)) {
            throw "BLAKE2 temporary work path escaped build/test: $workPath"
        }
        Remove-Item -LiteralPath $workPath -Recurse -Force
    }
}
