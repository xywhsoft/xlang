param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $Root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$workParent = Join-Path $Root 'build/test/sqlite_guard'
$runId = [guid]::NewGuid().ToString('N')
$work = Join-Path $workParent ("run_$runId")
$reportPath = Join-Path $workParent ("results_$runId.json")
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$pkg = 'release/extlibs/sqlite'
$sources = @("$pkg/sqlite_native.c", "$pkg/third_party/sqlite3.c")
$cases = @('test/cases/extlib_sqlite.xl', "$pkg/test/boundaries.xl", "$pkg/test/file.xl")
function InputPaths {
    @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
      'release/include/xrt_decl.h','release/include/xruntime_decl.h','release/include/xl_extlib_abi.h',
      'test/support/extlib_sqlite_runtime.c','tools/check_extlib_sqlite.ps1','tools/check_extlib_sqlite.sh',
      'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
      'test/phase1_extlibs.json','test/suites/12_phase1_extlibs.bat','test/cases/extlib_sqlite.xl',
      'release/examples/extlibs/sqlite.xl','release/examples/extlibs/sqlite_basic.xl',
      'release/examples/extlibs/sqlite_basic.expected.txt')
    Get-ChildItem (Join-Path $Root $pkg) -Recurse -File | Where-Object Extension -in @('.c','.h','.xl','.json') |
        ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash }
function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 120) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds
    $checks.Add([ordered]@{name=$Name;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[sqlite] ${Name}: PASS"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt')
$runtime = Join-Path $work 'runtime.o'
$native = Join-Path $work 'native.o'
$vendor = Join-Path $work 'sqlite3.o'
$objects = @($native,$vendor,$runtime,'-lAdvapi32','-lWs2_32','-lm')
$outcome = 'failed'
$decls = @()
$constants = @()
try {
    $null = Check 'runtime' $gcc ($flags + @('-c','test/support/extlib_sqlite_runtime.c','-o',$runtime))
    $null = Check 'bridge' $gcc ($flags + @('-c',"$pkg/sqlite_native.c",'-o',$native))
    $null = Check 'sqlite-vendor' $gcc @('-std=c11','-O2','-c',"$pkg/third_party/sqlite3.c",'-o',$vendor) 240
    foreach ($name in @('test_sqlite','test_sqlite_alloc','test_sqlite_threads')) {
        $binary = Join-Path $work "$name.exe"
        $testFlags = if ($name -eq 'test_sqlite_threads') { @('-DNDEBUG') } else { @() }
        $null = Check "$name-build" $gcc ($flags + $testFlags + @("$pkg/test/$name.c") + $objects + @('-o',$binary))
        $null = Check $name $binary @()
    }
    foreach ($name in @('test_sqlite','test_sqlite_threads')) {
        $null = Check "$name-jit" $xl (@('run',"$pkg/test/$name.c") + $sources + @('--inc-path','lib/xrt'))
    }
    foreach ($case in $cases) {
        $name = [IO.Path]::GetFileNameWithoutExtension($case)
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-jit" $xl @('run',$case)
        $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
        $null = Check "$name-aot" $binary @()
        $module = Join-Path $work "$name.c"
        $null = Check "$name-cgen" $xl @('cgen',$case,'--emit',$module)
        $decls += @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxsqlite\w*\(' })
    }
    $source = Get-Content (Join-Path $Root "$pkg/src/sqlite.xl") -Raw -Encoding UTF8
    $expected = @([regex]::Matches($source,'extern "(xsqlite\w+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $decls = @($decls | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxsqlite\w*(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -ne 0 -or $decls.Count -ne $expected.Count) { throw 'Incomplete generated ABI coverage' }
    $constants = @([regex]::Matches($source,'export const int (SQLITE_\w+) = (\w+)') | ForEach-Object {
        '_Static_assert({0} == {1}, "{0}");' -f $_.Groups[1].Value,$_.Groups[2].Value
    })
    if ($constants.Count -ne 45) { throw 'Incomplete constant verification' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"sqlite_native.h`"`n#include `"sqlite3.h`"`n" + (($decls + $constants) -join "`n"),$utf8)
    $null = Check 'generated-prototypes-and-constants' $gcc ($flags + @("-I$pkg/include","-I$pkg/third_party",'-fsyntax-only',$abi))
    foreach ($name in @('sqlite','sqlite_basic')) {
        $case = "release/examples/extlibs/$name.xl"
        $jit = Check "$name-example-jit" $xl @('run',$case)
        $binary = Join-Path $work "$name-example.exe"
        $null = Check "$name-example-build" $xl @('build',$case,'-o',$binary)
        $aot = Check "$name-example-aot" $binary @()
        if ($jit -cne $aot) { throw "$name JIT/AOT output mismatch" }
        if ($name -eq 'sqlite_basic') { $xlangOutput = $jit }
    }
    $example = "$pkg/examples/basic.c"
    $binary = Join-Path $work 'c-example.exe'
    $null = Check 'c-example-build' $gcc ($flags + @('-DNDEBUG',$example) + $objects + @('-o',$binary))
    $cOutput = Check 'c-example' $binary @()
    $jitOutput = Check 'c-example-jit' $xl @('run',$example)
    $binary = Join-Path $work 'c-example-tcc.exe'
    $null = Check 'c-example-aot-build' $xl @('build',$example,'-o',$binary)
    $aotOutput = Check 'c-example-aot' $binary @()
    $helperOutput = Check 'c-example-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','sqlite','-Source',$example)
    $golden = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/sqlite_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    foreach ($text in @($xlangOutput,$cOutput,$jitOutput,$aotOutput,$helperOutput)) { if ($text -cne $golden) { throw 'C/xlang example golden mismatch' } }
    $bad = Join-Path $work 'const-write.xl'
    [IO.File]::WriteAllText($bad,"import sqlite`nsqlite.SQLITE_OK = 1`n",$utf8)
    $r = Invoke-ExtlibSmokeProcess $xl @('check',$bad) $Root 30
    if ($r.exitCode -ne 1 -or $r.stderr -notmatch '(?i)const') { throw "Constant assignment was not rejected as expected: $($r.stderr)" }
    $checks.Add([ordered]@{name='constant-write-rejected';status='pass';exitCode=$r.exitCode;stderr=$r.stderr})
    foreach ($path in $hashes.Keys) { if ((Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash -ne $hashes[$path]) { throw "Input changed: $path" } }
    if (@(InputPaths | Where-Object { !$hashes.Contains($_) }).Count -ne 0) { throw 'Inputs added during gate' }
    $outcome = 'passed'
} finally {
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;scope='SQLite wrapper ABI/constants, C/state/allocator/thread tests, xlang JIT/AOT/file locks and paired examples';generatedPrototypes=$decls.Count;constants=$constants.Count;inputs=$hashes;checks=@($checks.ToArray())}
    [IO.File]::WriteAllText($reportPath,($report | ConvertTo-Json -Depth 8),$utf8)
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\', '/') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "SQLite temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
    Write-Host "[sqlite] report: $reportPath"
}
