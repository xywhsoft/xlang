param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $Root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$work = Join-Path $Root 'build/test/crc32_guard'
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$manifest = Get-Content (Join-Path $Root 'release/extlibs/crc32/xlang.package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/crc32/$_" })
$cases = @('extlib_crc32','extlib_crc32_progress')
function Inputs {
    @('.gitattributes','release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
      'release/include/xrt_decl.h','release/include/xruntime_decl.h','test/support/extlib_codec_runtime.c',
      'tools/check_extlib_crc32.ps1','tools/check_extlib_crc32.sh','tools/extlib_smoke_process.ps1',
      'tools/xlir_native_process.ps1','tools/run_extlib_c.ps1','test/featured_extlibs.json','test/suites/11_featured_extlibs_rc.bat',
      'release/examples/extlibs/crc32.xl','release/examples/extlibs/crc32_basic.xl','release/examples/extlibs/crc32_basic.expected.txt') +
      @($cases | ForEach-Object { "test/cases/$_.xl" }) +
      @(Get-ChildItem (Join-Path $Root 'release/extlibs/crc32') -Recurse -File |
        ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }) | Sort-Object -Unique
}
foreach ($path in @(Inputs)) { $hashes[$path] = (Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash }
function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 120) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[crc32] ${Name}: PASS"
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
    $pkg = Join-Path $Root 'release/extlibs/crc32'
    $null = Check 'runtime' $gcc ($baseFlags + @('-DXRT_MODULE_THREAD','-DXRT_MODULE_ATOMIC','-DXRT_MODULE_ONCE','-c','test/support/extlib_codec_runtime.c','-o',$runtime))
    for ($i=0; $i -lt $sources.Count; ++$i) {
        $obj = Join-Path $work "source-$i.o"
        $null = Check "native-$i" $gcc ($flags + @('-c',$sources[$i],'-o',$obj))
        $objects += $obj
    }
    $links = @($runtime,'-lWs2_32','-lAdvapi32','-lm')
    $standalone = Join-Path $work 'standalone.exe'
    $null = Check 'standalone-build' $gcc ($baseFlags + @('release/extlibs/crc32/test/test_crc32_standalone.c',$objects[0],'-lWs2_32','-lAdvapi32','-lm','-o',$standalone))
    $null = Check 'standalone' $standalone @()
    foreach ($name in @('test_crc32','test_crc32_native')) {
        $selected = if ($name -eq 'test_crc32') { @() } else { $objects }
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-build" $gcc ($flags + @("release/extlibs/crc32/test/$name.c") + $selected + $links + @('-o',$binary))
        $null = Check $name $binary @()
    }
    $core = 'release/extlibs/crc32/test/test_crc32.c'
    $null = Check 'core-c-jit' $xl @('run',$core,'--inc-path','lib/xrt')
    $null = Check 'core-c-package-runner' (Get-Process -Id $PID).Path @('-NoProfile','-ExecutionPolicy','Bypass','-File','tools/run_extlib_c.ps1','-Package','crc32','-Source',$core)
    foreach ($name in $cases) {
        $case = "test/cases/$name.xl"; $binary = Join-Path $work "$name.exe"; $module = Join-Path $work "$name.c"
        $null = Check "$name-jit" $xl @('run',$case)
        $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
        $null = Check "$name-aot" $binary @()
        $null = Check "$name-cgen" $xl @('cgen',$case,'--emit',$module)
        $decls += @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxcrc32_[a-z0-9_]+\(' })
    }
    $decls = @($decls | Sort-Object -Unique)
    $source = Get-Content (Join-Path $pkg 'src/crc32.xl') -Raw -Encoding UTF8
    $expected = @([regex]::Matches($source,'extern "(xcrc32_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxcrc32_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -or $decls.Count -ne $expected.Count) { throw 'Generated ABI coverage incomplete' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#define XRT_MODULE_VALUE`n#include <stdint.h>`n#include `"crc32_native.h`"`n"+($decls -join "`n"),$utf8)
    $null = Check 'generated-prototypes' $gcc ($flags + @('-Irelease/extlibs/crc32/include','-fsyntax-only',$abi))

    $golden = [IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/crc32_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    foreach ($name in @('crc32','crc32_basic')) {
        $case = "release/examples/extlibs/$name.xl"; $binary = Join-Path $work "$name-example.exe"
        $output = Check "$name-example-jit" $xl @('run',$case)
        $null = Check "$name-example-build" $xl @('build',$case,'-o',$binary)
        $aot = Check "$name-example-aot" $binary @()
        if ($output -cne $aot -or ($name -eq 'crc32_basic' -and $output -cne $golden)) { throw 'Language example output mismatch' }
    }
    $example = 'release/extlibs/crc32/examples/basic.c'; $binary = Join-Path $work 'c-example.exe'
    $null = Check 'c-example-build' $gcc ($flags + @($example) + $objects + $links + @('-o',$binary))
    $c = Check 'c-example' $binary @()
    $jit = Check 'c-example-jit' $xl (@('run',$example) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime'))
    $binary = Join-Path $work 'c-example-tcc.exe'
    $null = Check 'c-example-aot-build' $xl (@('build',$example) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','-o',$binary))
    $aot = Check 'c-example-aot' $binary @()
    if ($c -cne $golden -or $jit -cne $golden -or $aot -cne $golden) { throw 'C/XL golden output mismatch' }
    if (@(Compare-Object @($hashes.Keys) @(Inputs)).Count) { throw 'Input file set changed' }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash -ne $hashes[$path]) { throw "Input changed: $path" }
    }
    $outcome = 'passed'
} finally {
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;scope='CRC-32/ISO-HDLC: fixed vectors and 1536 bitwise differential cases, exact ABI, UTF-8/Buffer, state/readers/callbacks, concurrent first use, allocation failures, C/XL JIT/AOT/GCC examples; not Linux XL, big-endian target or performance certification';generatedPrototypes=$decls.Count;inputs=$hashes;checks=@($checks.ToArray())}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($report | ConvertTo-Json -Depth 8),$utf8)
}
