param([string]$Root = '', [switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $Root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$workRoot = Join-Path $Root 'build/test/blake3_guard'
$runId = [guid]::NewGuid().ToString('N')
$work = Join-Path $workRoot ('run_' + $runId)
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$toolHashes = [ordered]@{}
$manifest = Get-Content (Join-Path $Root 'release/extlibs/blake3/xlang.package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/blake3/$_" })
$paths = @('.gitattributes','release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
    'release/include/xrt_decl.h','release/include/xruntime_decl.h',
    'test/support/extlib_codec_runtime.c','tools/check_extlib_blake3.ps1',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
    'test/featured_extlibs.json','test/suites/11_featured_extlibs_rc.bat','tools/check_extlib_blake3.sh',
    'test/cases/extlib_blake3.xl','test/cases/extlib_blake3_boundaries.xl','test/cases/extlib_blake3_progress.xl','release/examples/extlibs/blake3.xl',
    'release/examples/extlibs/blake3_basic.xl','release/examples/extlibs/blake3_basic.expected.txt')
function Get-Blake3InputPaths {
    @($paths) + @(Get-ChildItem (Join-Path $Root 'release/extlibs/blake3') -Recurse -File |
        Where-Object { $_.Extension -in @('.c','.h','.xl','.json','.md','.html') -or $_.Name -eq 'LICENSE' } |
        ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }) |
        Sort-Object -Unique
}
foreach ($path in @(Get-Blake3InputPaths)) {
    $hashes[$path] = (Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash
}

function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 120) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[blake3] ${Name}: PASS"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}

$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Ilib/xruntime')
$packageFlags = $flags + @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$objects = @()
$runtime = Join-Path $work 'runtime.o'
$outcome = 'failed'
try {
    $toolHashes[$gcc] = (Get-FileHash $gcc).Hash
    $shell = (Get-Process -Id $PID).Path
    $toolHashes[$shell] = (Get-FileHash $shell).Hash
    $null = Check 'gcc-version' $gcc @('--version')
    $cc1 = Check 'gcc-cc1' $gcc @('-print-prog-name=cc1')
    if (!(Test-Path -LiteralPath $cc1 -PathType Leaf)) { throw "Missing GCC cc1: $cc1" }
    $toolHashes[(Resolve-Path $cc1).Path] = (Get-FileHash $cc1).Hash
    # 固定上游来源和独立向量；构建不联网，也不自动更新 vendor。
    $pkg = Join-Path $Root 'release/extlibs/blake3'
    $vendor = Get-Content (Join-Path $pkg 'third_party/manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($vendor.commit -ne '5aa53f07f7188a569cadfc5daf1522972d9a9630' -or $vendor.files.Count -ne 6) { throw 'Unexpected vendor lock' }
    foreach ($file in $vendor.files) {
        if ((Get-FileHash -LiteralPath (Join-Path $pkg $file.path)).Hash -ne $file.sha256) { throw "Vendor changed: $($file.path)" }
    }
    $vectors = Get-Content (Join-Path $pkg 'test/test_vectors.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $header = Get-Content (Join-Path $pkg 'test/vectors.h') -Raw -Encoding UTF8
    $rows = [regex]::Matches($header, '(?m)^\s*\{(\d+), \{"([0-9a-f]+)", "([0-9a-f]+)", "([0-9a-f]+)"\}\},\r?$')
    if ($rows.Count -ne $vectors.cases.Count -or $rows.Count -ne 35 -or
        !$header.Contains('static const char vector_key[] = "' + $vectors.key + '";') -or
        !$header.Contains('static const char vector_context[] = "' + $vectors.context_string + '";')) { throw 'Vector header differs from upstream JSON' }
    for ($i=0; $i -lt $rows.Count; ++$i) {
        $row=$rows[$i].Groups
        $case=$vectors.cases[$i]
        if ([int]$row[1].Value -ne $case.input_len -or $row[2].Value -cne $case.hash -or
            $row[3].Value -cne $case.keyed_hash -or $row[4].Value -cne $case.derive_key) { throw "Vector $i differs from upstream JSON" }
    }
    $golden = @(
        ('hash: ' + $vectors.cases[2].hash.Substring(0,32))
        ('clone: ' + $vectors.cases[1].hash.Substring(0,32))
        'bytes: 2'
        ('reset: ' + $vectors.cases[0].hash.Substring(0,32))
        ('keyed: ' + $vectors.cases[1].keyed_hash.Substring(0,32))
        ('kdf: ' + $vectors.cases[1].derive_key.Substring(0,32))
        ('xof/65: ' + $vectors.cases[1].hash.Substring(0,130))
    ) -join "`n"
    $expectedOutput = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/blake3_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($golden -cne $expectedOutput) { throw 'Example output differs from independent vectors' }
    $checks.Add([ordered]@{name='vendor-vector-golden';status='pass';vectors=$rows.Count;files=$vendor.files.Count})
    $null = Check 'runtime' $gcc ($flags + @('-DXRT_MODULE_CODEC_HEX','-DXRT_MODULE_THREAD','-c','test/support/extlib_codec_runtime.c','-o',$runtime))
    $index = 0
    foreach ($source in $sources) {
        $object = Join-Path $work "source-$index.o"
        $null = Check "native-$index" $gcc ($packageFlags + @('-c',$source,'-o',$object))
        $objects += $object
        ++$index
    }
    $links = @($runtime,'-lWs2_32','-lAdvapi32','-lm')
    $test = 'release/extlibs/blake3/test/test_blake3.c'
    $exe = Join-Path $work 'blake3-c.exe'
    # 核心测试直接包含 core，以白盒覆盖 uint64 计数溢出，不重复链接该对象。
    $null = Check 'c-build' $gcc ($packageFlags + @('-DNDEBUG',$test,$objects[0]) + $links + @('-o',$exe))
    $null = Check 'c-boundaries' $exe @()
    # 新用例自己包含 core/portable，只链接同一份验证运行时。
    $readerCause = 'release/extlibs/blake3/test/test_reader_cause.c'
    $readerExe = Join-Path $work 'blake3-reader-cause.exe'
    $null = Check 'reader-cause-build' $gcc ($packageFlags + @('-DNDEBUG',$readerCause) + $links + @('-o',$readerExe))
    $null = Check 'reader-cause' $readerExe @()
    $null = Check 'reader-cause-jit' $xl @('run',$readerCause)
    if (!$SkipAot) {
        $readerAot = Join-Path $work 'blake3-reader-cause-aot.exe'
        $null = Check 'reader-cause-aot-build' $xl @('build',$readerCause,'-o',$readerAot)
        $null = Check 'reader-cause-aot' $readerAot @()
    }
    $allocation = Join-Path $work 'blake3-alloc.exe'
    $null = Check 'allocation-build' $gcc ($packageFlags + @('-DNDEBUG','release/extlibs/blake3/test/test_blake3_alloc.c') + $objects + $links + @('-o',$allocation))
    $null = Check 'allocation' $allocation @()
    $native = Join-Path $work 'blake3-native.exe'
    $null = Check 'native-boundaries-build' $gcc ($packageFlags + @('-DNDEBUG','release/extlibs/blake3/test/test_blake3_native.c') + $objects + $links + @('-o',$native))
    $null = Check 'native-boundaries' $native @()
    $close = Join-Path $work 'blake3-close.exe'
    $null = Check 'close-build' $gcc ($packageFlags + @('-DNDEBUG','release/extlibs/blake3/test/test_blake3_close.c') + $objects[0..1] + $links + @('-o',$close))
    $null = Check 'close-boundaries' $close @()
    $example = 'release/extlibs/blake3/examples/basic.c'
    $releaseExample = Join-Path $work 'blake3-c-example-ndebug.exe'
    $null = Check 'c-example-ndebug-build' $gcc ($packageFlags + @('-DNDEBUG',$example) + $objects + $links + @('-o',$releaseExample))
    $releaseOutput = Check 'c-example-ndebug' $releaseExample @()
    if ($releaseOutput -cne $expectedOutput) { throw 'BLAKE3 -DNDEBUG C example output differs from official vectors' }
    $directOutput = Check 'c-example-direct-jit' $xl @('run',$example)
    if ($directOutput -cne $expectedOutput) { throw 'Direct C example output differs from official vectors' }
    $null = Check 'c-jit' $xl @('run',$test,$sources[0],'--inc-path','lib/xrt','--inc-path','lib/xruntime')
    $null = Check 'c-package-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','blake3','-Source',$test)
    foreach ($name in @('native_update_visibility_negative','native_keyed_update_visibility_negative')) {
        $case = "release/extlibs/blake3/test/$name.xl"
        $result = Invoke-ExtlibSmokeProcess $xl @('check',$case) $Root 30
        $rejected = $result.status -eq 'fail' -and
            ($result.stdout + $result.stderr) -match 'no matching callable class member'
        $checks.Add([ordered]@{name=$name;program=$xl;arguments=@('check',$case);status=$(if ($rejected) { 'pass' } else { 'fail' });exitCode=$result.exitCode;stdout=$result.stdout;stderr=$result.stderr})
        if (!$rejected) {
            throw "BLAKE3 native method visibility gate failed: $name"
        }
        Write-Host "[blake3] ${name}: PASS"
    }
    $case = 'release/extlibs/blake3/test/keyed_pointer_length_negative.xl'
    $result = Invoke-ExtlibSmokeProcess $xl @('check',$case) $Root 30
    $rejected = $result.status -eq 'fail' -and
        ($result.stdout + $result.stderr) -match '\[sem\].*create'
    $checks.Add([ordered]@{name='keyed-pointer-length-negative';program=$xl;arguments=@('check',$case);status=$(if ($rejected) { 'pass' } else { 'fail' });exitCode=$result.exitCode;stdout=$result.stdout;stderr=$result.stderr})
    if (!$rejected) { throw 'BLAKE3 raw keyed constructor remains callable' }
    Write-Host '[blake3] keyed-pointer-length-negative: PASS'
    foreach ($name in @('extlib_blake3','extlib_blake3_boundaries','extlib_blake3_progress')) {
        $case = "test/cases/$name.xl"
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-jit" $xl @('run',$case)
        if (!$SkipAot) {
            $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
            $null = Check "$name-aot" $binary @()
        }
    }
    $null = Check 'official-vectors-xlang' $xl @('run','release/extlibs/blake3/test/vectors.xl')
    if (!$SkipAot) {
        $vectorsExe = Join-Path $work 'official-vectors.exe'
        $null = Check 'official-vectors-aot-build' $xl @('build','release/extlibs/blake3/test/vectors.xl','-o',$vectorsExe)
        $null = Check 'official-vectors-aot' $vectorsExe @()
    }
    $decls = @()
    foreach ($name in @('extlib_blake3','extlib_blake3_boundaries','extlib_blake3_progress')) {
        $module = Join-Path $work "$name.c"
        $null = Check "$name-cgen" $xl @('cgen',"test/cases/$name.xl",'--emit',$module)
        $decls += @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxblake3_[a-z_]+\(' })
    }
    $decls = @($decls | Sort-Object -Unique)
    $source = Get-Content (Join-Path $Root 'release/extlibs/blake3/src/blake3.xl') -Raw -Encoding UTF8
    $expected = @([regex]::Matches($source,'extern "(xblake3_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxblake3_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -ne 0 -or $decls.Count -ne $expected.Count) {
        throw "BLAKE3 generated ABI coverage incomplete: expected $($expected.Count), got $($decls.Count)"
    }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include <stdint.h>`n#include `"blake3_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Check 'generated-prototypes' $gcc ($packageFlags + @('-Irelease/extlibs/blake3/include','-fsyntax-only',$abi))
    $ownedModule = [IO.File]::ReadAllText((Join-Path $work 'extlib_blake3.c'))
    foreach ($symbol in @(
        'xblake3_hash_hex_owned', 'xblake3_hash_str_hex_owned',
        'xblake3_hash_mac_hex_owned', 'xblake3_hash_mac_str_hex_owned',
        'xblake3_hash_kdf_hex_owned', 'xblake3_hash_kdf_str_hex_owned',
        'xblake3_file_hex_owned', 'xblake3_hasher_digest_hex_owned')) {
        if (-not $ownedModule.Contains("extern char* $symbol(") -or
            -not [regex]::IsMatch($ownedModule,
                '\.Data\s*=\s*' + [regex]::Escape($symbol) + '\s*\(')) {
            throw "BLAKE3 owned text is not directly adopted: $symbol"
        }
    }
    Write-Host '[blake3] owned text direct adoption: PASS'
    foreach ($name in @('blake3','blake3_basic')) {
        $case = "release/examples/extlibs/$name.xl"
        $text = Check "$name-example-jit" $xl @('run',$case)
        if ($name -eq 'blake3_basic') { $xlangOutput = $text }
        if (!$SkipAot) {
            $binary = Join-Path $work "$name-example.exe"
            $null = Check "$name-example-build" $xl @('build',$case,'-o',$binary)
            $aot = Check "$name-example-aot" $binary @()
            if ($aot -cne $text) { throw "$name JIT/AOT example output differs" }
        }
    }
    $example = 'release/extlibs/blake3/examples/basic.c'
    $binary = Join-Path $work 'blake3-c-example.exe'
    $null = Check 'c-example-build' $gcc ($packageFlags + @($example) + $objects + $links + @('-o',$binary))
    $cOutput = Check 'c-example' $binary @()
    $jitOutput = Check 'c-example-jit' $xl @('run',$example)
    if (!$SkipAot) {
        $binary = Join-Path $work 'blake3-c-example-tcc.exe'
        $null = Check 'c-example-aot-build' $xl @('build',$example,'-o',$binary)
        $cAot = Check 'c-example-aot' $binary @()
    }
    $helperOutput = Check 'c-example-package-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','blake3','-Source',$example)
    $expectedOutput = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/blake3_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($cOutput -cne $expectedOutput -or $jitOutput -cne $expectedOutput -or $xlangOutput -cne $expectedOutput -or
        (!$SkipAot -and $cAot -cne $expectedOutput) -or $helperOutput -cne $expectedOutput) {
        throw 'C/xlang examples differ from the common golden output'
    }
    # 新增或删除文件同样会改变验收输入，不能只核对原有文件的内容。
    if (@(Compare-Object @($hashes.Keys) @(Get-Blake3InputPaths)).Count -ne 0) {
        throw 'BLAKE3 input file set changed during the gate'
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash -ne $hashes[$path]) { throw "Input changed: $path" }
    }
    foreach ($path in $toolHashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $toolHashes[$path]) { throw "Tool changed: $path" }
    }
    $blockers = @()
    $privateCase = 'release/extlibs/blake3/test/private_adopt_constructor_negative.xl'
    $private = Invoke-ExtlibSmokeProcess $xl @('check',$privateCase) $Root 30
    $privateRejected = $private.status -eq 'fail' -and
        ($private.stdout + $private.stderr) -match '\[sem\]'
    $checks.Add([ordered]@{name='private-adopt-constructor';status=$(if ($privateRejected) { 'pass' } else { 'fail' });exitCode=$private.exitCode;stdout=$private.stdout;stderr=$private.stderr})
    if (!$privateRejected) { $blockers += 'private native handle adoption remains accessible' }
    if (!$SkipAot) {
        $globalCase = 'release/extlibs/blake3/test/aot_global_hasher_repro.xl'
        $globalJit = Check 'global-hasher-jit' $xl @('run',$globalCase)
        $globalExe = Join-Path $work 'global-hasher.exe'
        $null = Check 'global-hasher-build' $xl @('build',$globalCase,'-o',$globalExe)
        $globalAot = Invoke-ExtlibSmokeProcess $globalExe @() $Root 30
        $globalText = $globalAot.stdout.Replace("`r`n","`n").TrimEnd("`n")
        $globalPassed = $globalAot.status -eq 'pass' -and
            $globalText -ceq $globalJit
        $checks.Add([ordered]@{name='global-hasher-aot';status=$(if ($globalPassed) { 'pass' } else { 'fail' });exitCode=$globalAot.exitCode;stdout=$globalAot.stdout;stderr=$globalAot.stderr})
        if (!$globalPassed) { $blockers += 'top-level native hasher fails during standalone EXE retirement' }
    }
    if ($blockers.Count) { throw ('BLAKE3 release blockers: ' + ($blockers -join '; ')) }
    $outcome = 'passed'
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,$utf8)
    throw
} finally {
    $scope = 'Windows C boundaries, 35 upstream vectors in three modes and six lengths, allocation failures, independent threads, xlang/C JIT, callbacks and readers, generated ABI and paired examples'
    if (!$SkipAot) { $scope += ', Windows AOT' } else { $scope += '; AOT skipped' }
    $scope += '; not shared mutable hasher concurrency or benchmark certification'
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;scope=$scope;generatedPrototypes=$decls.Count;inputs=$hashes;tools=$toolHashes;checks=@($checks.ToArray())}
    try {
        [IO.File]::WriteAllText((Join-Path $workRoot ('results_' + $runId + '.json')),
            ($report | ConvertTo-Json -Depth 8),$utf8)
    } finally {
        $parentPath = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
        $workPath = [IO.Path]::GetFullPath($work)
        if (-not $workPath.StartsWith($parentPath, [StringComparison]::OrdinalIgnoreCase)) {
            throw "BLAKE3 temporary work path escaped build/test: $workPath"
        }
        if ($outcome -eq 'passed') { Remove-Item -LiteralPath $workPath -Recurse -Force }
        else { Write-Warning "BLAKE3 failure artifacts: $workPath" }
    }
}
