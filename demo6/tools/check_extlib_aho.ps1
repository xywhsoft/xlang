param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $Root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$work = Join-Path $Root 'build/test/aho_guard'
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$pkg = 'release/extlibs/aho_corasick'
$sources = @("$pkg/xrt_aho_corasick.c", "$pkg/aho_corasick_native.c")
$cases = @('test/cases/extlib_aho_corasick.xl', "$pkg/test/boundaries.xl")
function InputPaths {
    @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
      'release/include/xrt_decl.h','release/include/xruntime_decl.h','release/include/xl_extlib_abi.h',
      'test/support/extlib_aho_runtime.c','tools/check_extlib_aho.ps1','tools/check_extlib_aho.sh',
      'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
      'test/featured_extlibs.json','test/suites/11_featured_extlibs_rc.bat','test/cases/extlib_aho_corasick.xl',
      'release/examples/extlibs/aho_corasick.xl','release/examples/extlibs/aho_corasick_c.c',
      'release/examples/extlibs/aho_corasick_basic.xl','release/examples/extlibs/aho_corasick_basic.expected.txt')
    Get-ChildItem (Join-Path $Root $pkg) -Recurse -File | Where-Object Extension -in @('.c','.h','.xl','.json') |
        ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash }
function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 120) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[aho] ${Name}: PASS"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt')
$runtime = Join-Path $work 'runtime.o'
$core = Join-Path $work 'core.o'
$native = Join-Path $work 'native.o'
$objects = @($core,$native,$runtime,'-lAdvapi32','-lWs2_32')
$outcome = 'failed'
$decls = @()
try {
    $null = Check 'runtime' $gcc ($flags + @('-c','test/support/extlib_aho_runtime.c','-o',$runtime))
    $null = Check 'core' $gcc ($flags + @('-c',$sources[0],'-o',$core))
    $null = Check 'bridge' $gcc ($flags + @('-c',$sources[1],'-o',$native))
    foreach ($name in @('test_aho_corasick','test_aho_native','test_aho_alloc','test_aho_threads')) {
        $binary = Join-Path $work "$name.exe"
        # 核心白盒测试自身包含实现，其余测试使用正常独立编译单元。
        $links = if ($name -eq 'test_aho_corasick') { @($runtime,'-lAdvapi32','-lWs2_32') } else { $objects }
        $null = Check "$name-build" $gcc ($flags + @("$pkg/test/$name.c") + $links + @('-o',$binary))
        $null = Check $name $binary @()
    }
    $null = Check 'core-c-jit' $xl @('run',"$pkg/test/test_aho_corasick.c",'--inc-path','lib/xrt')
    foreach ($name in @('test_aho_native','test_aho_threads')) {
        $null = Check "$name-jit" $xl (@('run',"$pkg/test/$name.c") + $sources + @('--inc-path','lib/xrt'))
    }
    foreach ($name in @('private_add_pattern_native','private_feed_native')) {
        $case = "$pkg/test/$name.xl"
        $r = Invoke-ExtlibSmokeProcess $xl @('check',$case) $Root 30
        if ($r.exitCode -ne 1 -or $r.stderr -notmatch 'access') {
            throw "$name must be rejected by member access: $($r.stderr)"
        }
        $checks.Add([ordered]@{name=$name;status='pass';exitCode=$r.exitCode;stderr=$r.stderr})
    }
    foreach ($case in $cases) {
        $name = [IO.Path]::GetFileNameWithoutExtension($case)
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-jit" $xl @('run',$case)
        $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
        $null = Check "$name-aot" $binary @()
        $module = Join-Path $work "$name-generated.c"
        $null = Check "$name-cgen" $xl @('cgen',$case,'--emit',$module)
        $decls += @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxac_\w*\(' })
    }
    $source = Get-Content (Join-Path $Root "$pkg/src/aho_corasick.xl") -Raw -Encoding UTF8
    $expected = @([regex]::Matches($source,'extern "(xac_\w+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $decls = @($decls | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxac_\w*(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    $header = Get-Content (Join-Path $Root "$pkg/include/aho_corasick_native.h") -Raw -Encoding UTF8
    $declared = @([regex]::Matches($header,'\bxac_\w*(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -ne 0 -or @(Compare-Object $expected $declared).Count -ne 0 -or $decls.Count -ne $expected.Count) {
        throw 'Incomplete generated/native ABI coverage'
    }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"aho_corasick_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Check 'all-generated-prototypes' $gcc ($flags + @("-I$pkg/include",'-fsyntax-only',$abi))

    # 工厂只接收字符串模式数组，不隐式把数字或整段字符串当成模式集合。
    foreach ($expression in @('123','"abc"','[123]')) {
        $bad = Join-Path $work 'invalid-patterns.xl'
        [IO.File]::WriteAllText($bad,"import aho_corasick`nauto ac = aho_corasick.create($expression)`n",$utf8)
        $r = Invoke-ExtlibSmokeProcess $xl @('check',$bad) $Root 30
        if ($r.exitCode -ne 1 -or $r.stderr -notmatch '\[sem\]') { throw "Invalid patterns accepted or wrong failure: $expression $($r.stderr)" }
        $checks.Add([ordered]@{name="reject-patterns-$expression";status='pass';exitCode=$r.exitCode;stderr=$r.stderr})
    }

    foreach ($name in @('aho_corasick','aho_corasick_basic')) {
        $case = "release/examples/extlibs/$name.xl"
        $jit = Check "$name-example-jit" $xl @('run',$case)
        $binary = Join-Path $work "$name-example.exe"
        $null = Check "$name-example-build" $xl @('build',$case,'-o',$binary)
        $aot = Check "$name-example-aot" $binary @()
        if ($jit -cne $aot) { throw "$name JIT/AOT output mismatch" }
        if ($name -eq 'aho_corasick_basic') { $xlangOutput = $jit }
    }
    $golden = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/aho_corasick_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    foreach ($example in @('release/examples/extlibs/aho_corasick_c.c',"$pkg/examples/basic.c")) {
        $name = [IO.Path]::GetFileNameWithoutExtension($example)
        $binary = Join-Path $work "$name-gcc.exe"
        $null = Check "$name-c-build" $gcc ($flags + @($example) + $objects + @('-o',$binary))
        $c = Check "$name-c-run" $binary @()
        $jit = Check "$name-c-jit" $xl (@('run',$example) + $sources + @('--inc-path','lib/xrt'))
        $binary = Join-Path $work "$name-tcc.exe"
        $null = Check "$name-c-aot-build" $xl (@('build',$example) + $sources + @('--inc-path','lib/xrt','-o',$binary))
        $aot = Check "$name-c-aot" $binary @()
        $helper = Check "$name-c-runner" (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','aho_corasick','-Source',$example)
        foreach ($text in @($jit,$aot,$helper)) { if ($text -cne $c) { throw "$name C output mismatch" } }
        if ($name -eq 'basic' -and ($c -cne $golden -or $xlangOutput -cne $golden)) { throw 'C/xlang paired example differs from golden' }
    }
    foreach ($path in $hashes.Keys) { if ((Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash -ne $hashes[$path]) { throw "Input changed: $path" } }
    if (@(InputPaths | Where-Object { !$hashes.Contains($_) }).Count -ne 0) { throw 'Inputs added during gate' }
    $outcome = 'passed'
} finally {
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;scope='Aho core/native boundaries, differential chunks, OOM, shared immutable automaton threads, generated ABI, xlang/C JIT/AOT and paired examples; not simultaneous use of one scanner or performance certification';generatedPrototypes=$decls.Count;inputs=$hashes;checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($report | ConvertTo-Json -Depth 8),$utf8)
}
