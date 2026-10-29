param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$work = Join-Path $Root 'build/test/partial_helpers'
[IO.Directory]::CreateDirectory($work) | Out-Null
$checks = [Collections.Generic.List[object]]::new()
$fixedPaths = @('release/xl.exe','release/xl.dll','release/xlc.dll',
    'lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h',
    'release/include/xruntime_decl.h','test/support/extlib_helper_runtime.c',
    'tools/check_extlib_helper_boundaries.ps1','tools/check_extlib_helper_boundaries.sh',
    'test/suites/12_phase1_extlibs.bat','tools/rc0_guard.ps1',
    'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
    'test/cases/extlib_partial_helper_boundaries.xl','test/cases/extlib_partial_helper_boundaries_c.c',
    'release/examples/extlibs/mysql.xl','test/cases/extlib_mysql.xl')
function Snapshot {
    $result = [ordered]@{}
    $paths = $fixedPaths + @(Get-ChildItem (Join-Path $Root 'release/extlibs/mysql'),(Join-Path $Root 'release/extlibs/faiss') -Recurse -File |
        ForEach-Object { $_.FullName.Substring($Root.Length+1).Replace('\','/') })
    foreach ($p in @($paths | Sort-Object -Unique)) { $result[$p] = (Get-FileHash (Join-Path $Root $p)).Hash }
    return $result
}
$hashes = Snapshot
function Check([string]$Name,[string]$Program,[string[]]$Arguments) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root 90
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;result=$r})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)" }
    Write-Host "[partial-helpers] ${Name}: PASS"
}
function CheckExample([string]$Name,[string]$Program,[string[]]$Arguments) {
    Check $Name $Program $Arguments
    $r = $checks[$checks.Count - 1].result
    $expected = [IO.File]::ReadAllText((Join-Path $Root 'release/extlibs/mysql/examples/basic.expected.txt')).Replace("`r",'')
    if ($r.stdout.Replace("`r",'') -cne $expected -or $r.stderr.Length -ne 0) { throw "$Name output mismatch" }
}
$xl = Join-Path $Root 'release/xl.exe'
$gcc = (Get-Command gcc).Source
$src = @('release/extlibs/mysql/mysql_native.c','release/extlibs/faiss/faiss_native.c')
$c = 'test/cases/extlib_partial_helper_boundaries_c.c'
$x = 'test/cases/extlib_partial_helper_boundaries.xl'
$outcome = 'failed'
try {
    $exe = Join-Path $work 'native.exe'
    Check 'gcc-build' $gcc (@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',$c) + $src + @('test/support/extlib_helper_runtime.c','-lWs2_32','-lAdvapi32','-lm','-o',$exe))
    Check 'gcc' $exe @()
    Check 'c-jit' $xl (@('run',$c) + $src + @('--inc-path','lib/xrt'))
    Check 'xl-jit' $xl @('run',$x)
    foreach ($language in @('c','xl')) {
        $exe = Join-Path $work "$language-aot.exe"
        $args = if ($language -eq 'c') { @($c) + $src + @('--inc-path','lib/xrt') } else { @($x) }
        Check "$language-build" $xl (@('build') + $args + @('-o',$exe))
        Check "$language-aot" $exe @()
    }
    Check 'mysql-regression-jit' $xl @('run','test/cases/extlib_mysql.xl')
    $exe = Join-Path $work 'mysql-regression.exe'
    Check 'mysql-regression-build' $xl @('build','test/cases/extlib_mysql.xl','-o',$exe)
    Check 'mysql-regression-aot' $exe @()
    $exampleC = 'release/extlibs/mysql/examples/basic.c'
    $exampleXL = 'release/examples/extlibs/mysql.xl'
    $mysql = 'release/extlibs/mysql/mysql_native.c'
    $exe = Join-Path $work 'mysql-example-gcc.exe'
    Check 'mysql-example-gcc-build' $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',$exampleC,$mysql,'test/support/extlib_helper_runtime.c','-lWs2_32','-lAdvapi32','-lm','-o',$exe)
    CheckExample 'mysql-example-gcc' $exe @()
    CheckExample 'mysql-example-c-jit' $xl @('run',$exampleC,$mysql,'--inc-path','lib/xrt')
    CheckExample 'mysql-example-xl-jit' $xl @('run',$exampleXL)
    foreach ($language in @('c','xl')) {
        $exe = Join-Path $work "mysql-example-$language.exe"
        $args = if ($language -eq 'c') { @($exampleC,$mysql,'--inc-path','lib/xrt') } else { @($exampleXL) }
        Check "mysql-example-$language-build" $xl (@('build') + $args + @('-o',$exe))
        CheckExample "mysql-example-$language-aot" $exe @()
    }
    $after = Snapshot
    if (@(Compare-Object @($hashes.Keys) @($after.Keys)).Count) { throw 'Input set changed' }
    foreach ($p in $hashes.Keys) { if ($after[$p] -ne $hashes[$p]) { throw "Input changed: $p" } }
    $outcome = 'passed'
} finally {
    $r = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;scope='MySQL protocol/text helpers and FAISS scalar CSV distance helpers, plus matched C/xlang MySQL examples; not complete database/vector backends';inputs=$hashes;checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($r|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
}
