param([string]$Root = '', [switch]$SkipAot)
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl = Join-Path $Root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$workRoot = Join-Path $Root 'build/test/sha3_guard'
[IO.Directory]::CreateDirectory($workRoot) | Out-Null
$runId = [Guid]::NewGuid().ToString('N')
$work = Join-Path $workRoot ('run_' + $runId)
[IO.Directory]::CreateDirectory($work) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$manifest = Get-Content (Join-Path $Root 'release/extlibs/sha3/xlang.package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object { "release/extlibs/sha3/$_" })
$cases = @('extlib_sha3','extlib_sha3_progress')
function Inputs {
    @('.gitattributes','release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
      'release/include/xrt_decl.h','release/include/xruntime_decl.h','test/support/extlib_codec_runtime.c',
      'tools/check_extlib_sha3.ps1','tools/check_extlib_sha3.sh','tools/extlib_smoke_process.ps1',
      'tools/xlir_native_process.ps1','tools/run_extlib_c.ps1','test/featured_extlibs.json','test/suites/11_featured_extlibs_rc.bat',
      'release/examples/extlibs/sha3.xl','release/examples/extlibs/sha3_basic.xl','release/examples/extlibs/sha3_basic.expected.txt','release/examples/extlibs/sha3.expected.txt') +
      @($cases | ForEach-Object { "test/cases/$_.xl" }) +
      @(Get-ChildItem (Join-Path $Root 'release/extlibs/sha3') -Recurse -File |
        ForEach-Object { $_.FullName.Substring($Root.Length + 1).Replace('\','/') }) | Sort-Object -Unique
}
foreach ($path in @(Inputs)) { $hashes[$path] = (Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash }
function Check([string]$Name, [string]$Program, [string[]]$Arguments, [int]$Seconds = 120) {
    $r = Invoke-ExtlibSmokeProcess $Program $Arguments $Root $Seconds
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if ($r.status -ne 'pass') { throw "$Name failed: $($r.stderr)`n$($r.stdout)" }
    Write-Host "[sha3] ${Name}: PASS"
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
    $pkg = Join-Path $Root 'release/extlibs/sha3'
    $null = Check 'runtime' $gcc ($baseFlags + @('-DXRT_MODULE_THREAD','-DXRT_MODULE_ATOMIC','-DXRT_MODULE_CODEC_HEX','-DXRT_MODULE_BUFFER','-DXRT_MODULE_DIR_TEMP','-DXRT_MODULE_PATH','-DXRT_MODULE_FILE_WHOLE','-c','test/support/extlib_codec_runtime.c','-o',$runtime))
    for ($i=0; $i -lt $sources.Count; ++$i) {
        $obj = Join-Path $work "source-$i.o"
        $null = Check "native-$i" $gcc ($flags + @('-c',$sources[$i],'-o',$obj))
        $objects += $obj
    }
    $links = @($runtime,'-lWs2_32','-lAdvapi32','-lm')
    $standalone = Join-Path $work 'standalone.exe'
    $null = Check 'standalone-build' $gcc ($baseFlags + @('release/extlibs/sha3/test/test_sha3_standalone.c',$objects[0],$objects[1],'-lWs2_32','-lAdvapi32','-lm','-o',$standalone))
    $null = Check 'standalone' $standalone @()
    foreach ($name in @('test_sha3','test_sha3_native')) {
        $selected = if ($name -eq 'test_sha3') { @() } else { $objects }
        $binary = Join-Path $work "$name.exe"
        $null = Check "$name-build" $gcc ($flags + @("release/extlibs/sha3/test/$name.c") + $selected + $links + @('-o',$binary))
        $null = Check $name $binary @()
    }
    $core = 'release/extlibs/sha3/test/test_sha3.c'
    $null = Check 'core-c-jit' $xl @('run',$core,'--inc-path','lib/xrt')
    $null = Check 'core-c-package-runner' (Get-Process -Id $PID).Path @('-NoProfile','-ExecutionPolicy','Bypass','-File','tools/run_extlib_c.ps1','-Package','sha3','-Source',$core)
    $case = 'release/extlibs/sha3/test/private_update_negative.xl'
    $result = Invoke-ExtlibSmokeProcess $xl @('check',$case) $Root 30
    $rejected = $result.status -eq 'fail' -and
        ($result.stdout + $result.stderr) -match 'no matching callable class member'
    $checks.Add([ordered]@{name='private-update-negative';program=$xl;arguments=@('check',$case);status=$(if ($rejected) { 'pass' } else { 'fail' });exitCode=$result.exitCode;stdout=$result.stdout;stderr=$result.stderr})
    if (!$rejected) { throw 'SHA3 native pointer/length method remains callable from importer' }
    Write-Host '[sha3] private-update-negative: PASS'
    foreach ($name in $cases) {
        $case = "test/cases/$name.xl"; $binary = Join-Path $work "$name.exe"; $module = Join-Path $work "$name.c"
        $null = Check "$name-jit" $xl @('run',$case)
        if (!$SkipAot) {
            $null = Check "$name-build" $xl @('build',$case,'-o',$binary)
            $null = Check "$name-aot" $binary @()
        }
        $null = Check "$name-cgen" $xl @('cgen',$case,'--emit',$module)
        $decls += @(Get-Content $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bx(?:sha3|shake)_[a-z0-9_]+\(' })
    }
    $decls = @($decls | Sort-Object -Unique)
    $source = Get-Content (Join-Path $pkg 'src/sha3.xl') -Raw -Encoding UTF8
    $expected = @([regex]::Matches($source,'extern "(x(?:sha3|shake)_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bx(?:sha3|shake)_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if (@(Compare-Object $expected $actual).Count -or $decls.Count -ne $expected.Count) { throw 'Generated ABI coverage incomplete' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include <stdint.h>`n#include `"xruntime.h`"`n#include `"sha3_native.h`"`n"+($decls -join "`n"),$utf8)
    $null = Check 'generated-prototypes' $gcc ($flags + @('-Irelease/extlibs/sha3/include','-fsyntax-only',$abi))

    foreach ($name in @('sha3','sha3_basic')) {
        $golden = [IO.File]::ReadAllText((Join-Path $Root "release/examples/extlibs/$name.expected.txt")).Replace("`r`n","`n").TrimEnd("`n")
        $case = "release/examples/extlibs/$name.xl"; $binary = Join-Path $work "$name-example.exe"
        $output = Check "$name-example-jit" $xl @('run',$case)
        if (!$SkipAot) {
            $null = Check "$name-example-build" $xl @('build',$case,'-o',$binary)
            $aot = Check "$name-example-aot" $binary @()
            if ($output -cne $aot) { throw 'Language example JIT/AOT output mismatch' }
        }
        if ($output -cne $golden) { throw 'Language example output mismatch' }
    }
    foreach ($name in @('basic','files')) {
        $stem = if ($name -eq 'basic') { 'sha3_basic' } else { 'sha3' }
        $golden = [IO.File]::ReadAllText((Join-Path $Root "release/examples/extlibs/$stem.expected.txt")).Replace("`r`n","`n").TrimEnd("`n")
        $example = "release/extlibs/sha3/examples/$name.c"; $binary = Join-Path $work "c-$name.exe"
        $null = Check "c-$name-build" $gcc ($flags + @('-DNDEBUG',$example) + $objects + $links + @('-o',$binary))
        $c = Check "c-$name" $binary @()
        $jit = Check "c-$name-jit" $xl (@('run',$example) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime'))
        $binary = Join-Path $work "c-$name-tcc.exe"
        $null = Check "c-$name-aot-build" $xl (@('build',$example) + $sources + @('--inc-path','lib/xrt','--inc-path','lib/xruntime','-o',$binary))
        $aot = Check "c-$name-aot" $binary @()
        if ($c -cne $golden -or $jit -cne $golden -or $aot -cne $golden) { throw 'C/XL golden output mismatch' }
    }
    if (@(Compare-Object @($hashes.Keys) @(Inputs)).Count) { throw 'Input file set changed' }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Root $path)).Hash -ne $hashes[$path]) { throw "Input changed: $path" }
    }
    $outcome = 'passed'
} finally {
    $scope = 'SHA3/SHAKE: 546 OpenSSL boundary vectors x 8 alignments, 12 known answers, splits, strict algorithms/lengths, UTF-8/Buffer, state/readers/callbacks, exact generated ABI, four independent threads, 176 ordinary/temp allocation failure positions, paired C/XL JIT/GCC examples'
    if (!$SkipAot) { $scope += ', XL AOT' } else { $scope += '; XL AOT skipped' }
    $report = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;scope=$scope;generatedPrototypes=$decls.Count;inputs=$hashes;checks=@($checks.ToArray())}
    try {
        [IO.File]::WriteAllText((Join-Path $workRoot ('results_' + $runId + '.json')),
            ($report | ConvertTo-Json -Depth 8),$utf8)
    } finally {
        $parentPath = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
        $workPath = [IO.Path]::GetFullPath($work)
        if (!$workPath.StartsWith($parentPath, [StringComparison]::OrdinalIgnoreCase)) {
            throw "SHA3 temporary work path escaped build/test: $workPath"
        }
        Remove-Item -LiteralPath $workPath -Recurse -Force
    }
}
