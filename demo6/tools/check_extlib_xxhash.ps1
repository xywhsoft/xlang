param([string]$Root = '', [switch]$SkipAot)
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $Root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$work = Join-Path $Root 'build/test/xxhash_guard'
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$manifest = Get-Content (Join-Path $Root 'release/extlibs/xxhash/xlang.package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/xxhash/$_" })
$cases = @('extlib_xxhash','extlib_xxhash_boundaries','extlib_xxhash_progress')
function Inputs {
    @('.gitattributes','release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
      'release/include/xrt_decl.h','release/include/xruntime_decl.h','test/support/extlib_codec_runtime.c',
      'tools/check_extlib_xxhash.ps1','tools/check_extlib_xxhash.sh','tools/extlib_smoke_process.ps1',
      'tools/xlir_native_process.ps1','tools/run_extlib_c.ps1','test/featured_extlibs.json','test/suites/11_featured_extlibs_rc.bat',
      'release/examples/extlibs/xxhash.xl','release/examples/extlibs/xxhash_basic.xl','release/examples/extlibs/xxhash_basic.expected.txt') +
      @($cases | ForEach-Object { "test/cases/$_.xl" }) +
      @(Get-ChildItem (Join-Path $Root 'release/extlibs/xxhash') -Recurse -File |
        ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }) | Sort-Object -Unique
}
foreach ($path in @(Inputs)) { $hashes[$path] = (Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash }
function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 120) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[xxhash] ${Name}: PASS"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Ilib/xruntime')
$baseFlags = $flags
$flags += @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$objects = @()
$runtime = Join-Path $work 'runtime.o'
$outcome = 'failed'
$decls = @()
try {
    $negativeCases = [ordered]@{ private_update_negative='updateNative'; private_string_length_negative='updateStrNative' }
    foreach ($name in $negativeCases.Keys) {
        $args = @('check',"release/extlibs/xxhash/test/$name.xl")
        $result = Invoke-ExtlibSmokeProcess $xl $args $Root 30
        $checks.Add([ordered]@{name=$name;program=$xl;arguments=$args;status=$result.status;exitCode=$result.exitCode;elapsedMs=$result.elapsedMs;stdout=$result.stdout;stderr=$result.stderr})
        if ($result.status -ne 'fail' -or
            !$result.stderr.Contains($negativeCases[$name]) -or
            !$result.stderr.Contains('access')) {
            throw "$name did not report private access"
        }
        Write-Host "[xxhash] ${name}: rejected as expected"
    }
    $pkg = Join-Path $Root 'release/extlibs/xxhash'
    $lock = Get-Content (Join-Path $pkg 'third_party/manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($lock.commit -ne 'e626a72bc2321cd320e953a0ccf1584cad60f363' -or !$lock.headerUnmodified) { throw 'Invalid xxHash vendor lock' }
    foreach ($file in $lock.files) {
        if ((Get-FileHash -LiteralPath (Join-Path $pkg $file.path)).Hash -ne $file.sha256) { throw "Locked file changed: $($file.path)" }
    }
    $vectors = Get-Content (Join-Path $pkg 'test/vectors.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $header = Get-Content (Join-Path $pkg 'test/vectors.h') -Raw -Encoding UTF8
    $rows = [regex]::Matches($header,'\{(32|64), (\d+), UINT64_C\((0x[0-9A-F]+)\), UINT64_C\((0x[0-9A-F]+)\)\}')
    if ($rows.Count -ne 124 -or $rows.Count -ne $vectors.vectors.Count) { throw 'Incomplete vector table' }
    for ($i=0; $i -lt $rows.Count; ++$i) {
        $v=$vectors.vectors[$i]; $r=$rows[$i].Groups
        if ([int]$r[1].Value -ne $v.bits -or [int]$r[2].Value -ne $v.length -or $r[3].Value -cne $v.seed -or $r[4].Value -cne $v.hash) { throw "Vector $i mismatch" }
    }
    $checks.Add([ordered]@{name='vendor-vectors';status='pass';vectors=124})
    $null = Check 'runtime' $gcc ($baseFlags + @('-DXRT_MODULE_THREAD','-c','test/support/extlib_codec_runtime.c','-o',$runtime))
    for ($i=0; $i -lt $sources.Count; ++$i) {
        $obj = Join-Path $work "source-$i.o"
        $null = Check "native-$i" $gcc ($flags + @('-c',$sources[$i],'-o',$obj))
        $objects += $obj
    }
    $links = @($runtime,'-lWs2_32','-lAdvapi32','-lm')
    foreach ($name in @('test_xxhash','test_xxhash_native')) {
        $selected = if ($name -eq 'test_xxhash') { @($objects[2]) } else { $objects }
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-build" $gcc ($flags + @('-DNDEBUG',"release/extlibs/xxhash/test/$name.c") + $selected + $links + @('-o',$binary))
        $null = Check $name $binary @()
    }
    $example = 'release/extlibs/xxhash/examples/basic.c'
    $releaseExample = Join-Path $work 'xxhash-c-example-ndebug.exe'
    $null = Check 'c-example-ndebug-build' $gcc ($flags + @('-DNDEBUG',$example) + $objects + $links + @('-o',$releaseExample))
    $releaseOutput = Check 'c-example-ndebug' $releaseExample @()
    $releaseGolden = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/xxhash_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($releaseOutput -cne $releaseGolden) { throw 'xxHash -DNDEBUG C example output differs from golden' }
    $directOutput = Check 'c-example-direct-jit' $xl @('run',$example)
    if ($directOutput -cne $releaseGolden) { throw 'Direct C example output differs from golden' }
    $core = 'release/extlibs/xxhash/test/test_xxhash.c'
    $null = Check 'core-c-jit' $xl @('run',$core,$sources[2],'--inc-path','lib/xrt')
    $null = Check 'core-c-package-runner' (Get-Process -Id $PID).Path @('-NoProfile','-ExecutionPolicy','Bypass','-File','tools/run_extlib_c.ps1','-Package','xxhash','-Source',$core)
    $null = Check 'seeded-boundaries-jit' $xl @('run','release/extlibs/xxhash/test/seeded_boundaries.xl')
    $null = Check 'stream-limit-jit' $xl @('run','release/extlibs/xxhash/test/stream_limit.xl')
    foreach ($name in $cases) {
        $case = "test/cases/$name.xl"; $binary = Join-Path $work "$name.exe"; $module = Join-Path $work "$name.c"
        $null = Check "$name-jit" $xl @('run',$case)
        if (!$SkipAot) {
            $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
            $null = Check "$name-aot" $binary @()
        }
        $null = Check "$name-cgen" $xl @('cgen',$case,'--emit',$module)
        $decls += @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxxxhash_[a-z0-9_]+\(' })
    }
    $decls = @($decls | Sort-Object -Unique)
    $source = Get-Content (Join-Path $pkg 'src/xxhash.xl') -Raw -Encoding UTF8
    $expected = @([regex]::Matches($source,'extern "(xxxhash_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxxxhash_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -or $decls.Count -ne $expected.Count) { throw 'Generated ABI coverage incomplete' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#define XRT_MODULE_VALUE`n#include <stdint.h>`n#include `"xxhash_native.h`"`n"+($decls -join "`n"),$utf8)
    $null = Check 'generated-prototypes' $gcc ($flags + @('-Irelease/extlibs/xxhash/include','-fsyntax-only',$abi))

    # 负数或超宽种子必须在语义阶段拒绝，而不是依赖 C 截断。
    foreach ($name in @('invalid_seed_negative','invalid_seed32_overflow')) {
        $case = "release/extlibs/xxhash/test/$name.xl"
        $r = Invoke-ExtlibSmokeProcess $xl @('check',$case) $Root 30
        $ok = $r.status -eq 'fail' -and $r.exitCode -eq 1 -and $r.stderr -match '\[sem\]'
        $checks.Add([ordered]@{name=$name;status=$(if ($ok) {'pass'} else {'fail'});program=$xl;arguments=@('check',$case);exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
        if (!$ok) { throw "Seed range rejection failed: $name $($r.stderr)" }
        Write-Host "[xxhash] ${name}: PASS"
    }
    $golden = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/xxhash_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    foreach ($name in @('xxhash','xxhash_basic')) {
        $case = "release/examples/extlibs/$name.xl"; $binary = Join-Path $work "$name-example.exe"
        $output = Check "$name-example-jit" $xl @('run',$case)
        if (!$SkipAot) {
            $null = Check "$name-example-build" $xl @('build',$case,'-o',$binary)
            $aot = Check "$name-example-aot" $binary @()
            if ($output -cne $aot) { throw 'Language example JIT/AOT output mismatch' }
        }
        if ($name -eq 'xxhash_basic' -and $output -cne $golden) { throw 'Language example output mismatch' }
    }
    $example = 'release/extlibs/xxhash/examples/basic.c'; $binary = Join-Path $work 'c-example.exe'
    $null = Check 'c-example-build' $gcc ($flags + @($example) + $objects + $links + @('-o',$binary))
    $c = Check 'c-example' $binary @()
    $jit = Check 'c-example-jit' $xl @('run',$example)
    if (!$SkipAot) {
        $binary = Join-Path $work 'c-example-tcc.exe'
        $null = Check 'c-example-aot-build' $xl @('build',$example,'-o',$binary)
        $aot = Check 'c-example-aot' $binary @()
        if ($aot -cne $golden) { throw 'C example AOT output mismatch' }
    }
    if ($c -cne $golden -or $jit -cne $golden) { throw 'C/XL golden output mismatch' }
    if (@(Compare-Object @($hashes.Keys) @(Inputs)).Count) { throw 'Input file set changed' }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash -ne $hashes[$path]) { throw "Input changed: $path" }
    }
    $outcome = 'passed'
} finally {
    $scope = 'XXH32/XXH64: 124 independent vectors, native ABI, UTF-8, state/readers/callbacks, allocation failures, independent threads, C/XL JIT/GCC examples'
    if (!$SkipAot) { $scope += ', Windows AOT' } else { $scope += '; AOT skipped' }
    $scope += '; not XXH3 language API, Linux XL AOT, 32-bit/ARM or performance certification'
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;scope=$scope;generatedPrototypes=$decls.Count;inputs=$hashes;checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($report | ConvertTo-Json -Depth 8),$utf8)
}
