param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$safeRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $safeRoot ('sitemap_guard_' + [guid]::NewGuid().ToString('N'))
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$shell = (Get-Process -Id $PID).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$passed = $false
$inputs = @('sitemap','xml.sax','_expat') | ForEach-Object {
    Get-ChildItem (Join-Path $root "release/extlibs/$_") -Recurse -File | ForEach-Object FullName
}
$inputs += @(Get-ChildItem (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName)
$inputs += @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h','tools/run_extlib_c.ps1','tools/extlib_smoke_process.ps1',
    'tools/xlir_native_process.ps1','test/cases/extlib_sitemap.xl',
    'release/examples/extlibs/sitemap_pair.xl','release/examples/extlibs/sitemap.xl') |
    ForEach-Object {Join-Path $root $_}
$hashes = @{}
foreach ($path in @($inputs) + @($gcc,$python,$shell)) {$hashes[$path] = (Get-FileHash -LiteralPath $path).Hash}
$flags = @('-std=c11','-O1','-Wall','-Wextra','-Ilib/xrt',
    '-Irelease/extlibs/sitemap/include','-Irelease/extlibs/xml.sax',
    '-Irelease/extlibs/_expat/vendor/expat/lib','-Irelease/extlibs/_expat/vendor/expat')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$sources = @('release/extlibs/_expat/vendor/expat/lib/xmlparse.c',
    'release/extlibs/_expat/vendor/expat/lib/xmlrole.c','release/extlibs/_expat/vendor/expat/lib/xmltok.c',
    'release/extlibs/_expat/expat_random.c','release/extlibs/xml.sax/xrt_xml_common.c',
    'release/extlibs/xml.sax/xrt_xml_sax.c','release/extlibs/xml.sax/xml_sax_native.c',
    'release/extlibs/sitemap/sitemap_xml.c','release/extlibs/sitemap/sitemap_robots.c')
$cArgs = @($sources)
foreach ($dir in @('lib/xrt','release/extlibs/sitemap/include','release/extlibs/xml.sax',
    'release/extlibs/_expat/vendor/expat/lib','release/extlibs/_expat/vendor/expat')) {$cArgs += @('--inc-path',$dir)}

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$expected = 0) {
    if ($null -eq $arguments) {$arguments = @()}
    if ($program.EndsWith('.ps1')) {
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$program) + $arguments
        $program = $shell
    }
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 180 @{LC_ALL='C'}
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;expectedExit=$expected;result=$result})
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nstatus=$($result.status) exit=$($result.exitCode)`nstdout:`n$($result.stdout)`nstderr:`n$($result.stderr)`n")
    if ($result.status -notin @('pass','fail') -or $result.exitCode -ne $expected) {
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stdout)`n$($result.stderr)"
    }
    Write-Host "[sitemap] ${name}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n","`n").TrimEnd("`n")
}

function VerifyInputs {
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {throw "sitemap input changed: $path"}
    }
}

[void][IO.Directory]::CreateDirectory($work)
Push-Location $root
try {
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $null = Run 'Python version' $python @('--version')
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.before.json') -Encoding UTF8
    # 强制公共头进入实际实现编译，核对包括 C 便利入口在内的十个原型。
    $null = Run 'strict manifest and ten C declarations' $gcc ($flags + @('-Werror','-fsyntax-only',
        '-include','sitemap_native.h','-DXRT_MODULE_IO','-DXRT_MODULE_BUFFER','-DXRT_MODULE_STRING',
        '-DXRT_MODULE_UNICODE','-DXRT_MODULE_VALUE_CONTAINER',
        'release/extlibs/sitemap/sitemap_xml.c','release/extlibs/sitemap/sitemap_robots.c'))
    $null = Run 'strict fixtures' $gcc ($flags + @('-Werror','-fsyntax-only',
        'release/extlibs/sitemap/test/test_sitemap.c','release/extlibs/sitemap/test/test_robots_newlines.c',
        'release/extlibs/sitemap/examples/sitemap_c.c','release/extlibs/sitemap/examples/robots_policy_c.c'))
    $objects = @()
    $allSources = @('release/extlibs/sitemap/test/xrt_runtime.c') + $sources
    for ($i = 0; $i -lt $allSources.Count; ++$i) {
        $object = Join-Path $work ("unit_$i.o")
        $null = Run "C compile $($allSources[$i])" $gcc ($flags + @('-c',$allSources[$i],'-o',$object))
        $objects += $object
    }
    foreach ($name in @('test_sitemap','test_robots_newlines')) {
        $target = Join-Path $work ($name + '.exe')
        $null = Run "GCC $name build" $gcc ($flags + @('-Werror',"release/extlibs/sitemap/test/$name.c") + $objects + $links + @('-o',$target))
        $null = Run "GCC $name" $target @()
        $null = Run "TCC $name JIT" $runner @('-Package','sitemap','-Source',"release/extlibs/sitemap/test/$name.c")
    }
    $null = Run 'XLang robots newlines JIT' $xl @('run','release/extlibs/sitemap/test/robots_newlines.xl')
    $generated = Join-Path $work 'generated.c'
    $null = Run 'actual ABI cgen' $xl @('cgen','test/cases/extlib_sitemap.xl','--emit',$generated)
    $prototypes = [regex]::Matches([IO.File]::ReadAllText($generated),'(?m)^extern [^\r\n;]*\bxsm_(?:build|build_index|parse_n|parse_index_n|robots_parse_n|robots_allows_n)\([^;\r\n]*\);')
    if ($prototypes.Count -ne 6) {throw 'All six actual sitemap foreign prototypes must be checked'}
    $abi = Join-Path $work 'abi.c'
    $abiText = '#include "sitemap_native.h"' + "`n" + (($prototypes | ForEach-Object Value) -join "`n")
    [IO.File]::WriteAllText($abi,$abiText,[Text.UTF8Encoding]::new($false))
    $null = Run 'six actual generated prototypes' $gcc ($flags + @('-Werror','-fsyntax-only',$abi))
    $wrong = $abiText -replace '(?m)^extern [^\r\n;]*\bxsm_parse_n\(', 'extern int xsm_parse_n('
    if ($wrong -ceq $abiText) {throw 'Wrong ABI control did not change declaration'}
    [IO.File]::WriteAllText($abi,$wrong,[Text.UTF8Encoding]::new($false))
    $null = Run 'wrong parse return rejected' $gcc ($flags + @('-Werror','-fsyntax-only',$abi)) 1
    if ($checks[-1].result.stderr -notmatch 'conflicting types') {throw 'Wrong ABI failed for an unrelated reason'}

    $golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/sitemap/examples/robots_policy.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    foreach ($example in @('sitemap','robots_policy')) {
        $source = "release/extlibs/sitemap/examples/${example}_c.c"
        $target = Join-Path $work ($example + '_gcc.exe')
        $null = Run "GCC $example build" $gcc ($flags + @('-Werror',$source) + $objects + $links + @('-o',$target))
        $gccOutput = Run "GCC $example" $target @()
        $jit = Run "C $example JIT" $runner @('-Package','sitemap','-Source',$source)
        if ($gccOutput -cne $jit) {throw "$example C JIT/GCC mismatch"}
        if ($example -eq 'robots_policy' -and $gccOutput -cne $golden) {throw 'C policy golden mismatch'}
        $cFile = Join-Path $work ($example + '_gcc.txt')
        [IO.File]::WriteAllText($cFile,$gccOutput + "`n",[Text.UTF8Encoding]::new($false))
        if ($example -eq 'sitemap') {
            $null = Run 'GCC/JIT independent XML oracle' $python @('-I','release/extlibs/sitemap/test/check_external.py',$cFile,$cFile)
            # 双份相同坏输出也必须拒绝，不能只比较两个生成器是否一致。
            $bad = Join-Path $work 'bad.xml.txt'
            $badText = $gccOutput.Replace('https://example.test/?a=1&amp;b=2','https://example.test/wrong')
            if ($badText -ceq $gccOutput) {throw 'XML oracle mutation did not change input'}
            [IO.File]::WriteAllText($bad,$badText + "`n",[Text.UTF8Encoding]::new($false))
            $null = Run 'corrupt XML golden rejected' $python @('-I','release/extlibs/sitemap/test/check_external.py',$bad,$bad) 1
            if ($checks[-1].result.stderr -notmatch 'URL or XML escaping differs') {throw 'Oracle failed for an unrelated reason'}
        } else {
            $output = Run 'XLang policy JIT' $xl @('run','release/extlibs/sitemap/examples/robots_policy.xl')
            if ($output -cne $golden) {throw 'XLang policy golden mismatch'}
        }
        if (!$SkipAot) {
            $target = Join-Path $work ($example + '_caot.exe')
            $null = Run "C $example AOT build" $xl (@('build',$source) + $cArgs + @('-o',$target))
            $output = Run "C $example AOT" $target @()
            if ($output -cne $gccOutput) {throw "$example C AOT/GCC mismatch"}
            if ($example -eq 'robots_policy') {
                $target = Join-Path $work 'policy_xlaot.exe'
                $null = Run 'XLang policy AOT build' $xl @('build','release/extlibs/sitemap/examples/robots_policy.xl','-o',$target)
                $output = Run 'XLang policy AOT' $target @()
                if ($output -cne $golden) {throw 'XLang policy AOT golden mismatch'}
            }
        }
    }
    if (!$SkipAot) {
        $target = Join-Path $work 'newlines_c.exe'
        $null = Run 'C newlines AOT build' $xl (@('build','release/extlibs/sitemap/test/test_robots_newlines.c') + $cArgs + @('-o',$target))
        $null = Run 'C newlines AOT' $target @()
        $target = Join-Path $work 'newlines_xl.exe'
        $null = Run 'XLang newlines AOT build' $xl @('build','release/extlibs/sitemap/test/robots_newlines.xl','-o',$target)
        $null = Run 'XLang newlines AOT' $target @()
    }
    VerifyInputs
    # 原语言数组生成例依然必须成功；M1 不改成预期失败。
    foreach ($source in @('test/cases/extlib_sitemap.xl','release/extlibs/sitemap/test/boundaries.xl',
        'release/extlibs/sitemap/test/allocations.xl','release/extlibs/sitemap/test/protocol_limits.xl')) {
        $null = Run $source $xl @('run',$source)
    }
    $output = Run 'original XLang XML example' $xl @('run','release/examples/extlibs/sitemap_pair.xl')
    $xlFile = Join-Path $work 'original_xl.txt'
    [IO.File]::WriteAllText($xlFile,$output + "`n",[Text.UTF8Encoding]::new($false))
    $null = Run 'C/XLang XML oracle' $python @('-I','release/extlibs/sitemap/test/check_external.py',(Join-Path $work 'sitemap_gcc.txt'),$xlFile)
    if (!$SkipAot) {
        $target = Join-Path $work 'core.exe'
        $null = Run 'original core AOT build' $xl @('build','release/extlibs/sitemap/test/core_aot.xl','-o',$target)
        $output = Run 'original core AOT' $target @()
        if ($output -cne '') {throw 'Original core AOT must have no output'}
        $target = Join-Path $work 'original_example.exe'
        $null = Run 'original XLang XML AOT build' $xl @('build','release/examples/extlibs/sitemap_pair.xl','-o',$target)
        $output = Run 'original XLang XML AOT' $target @()
        [IO.File]::WriteAllText($xlFile,$output + "`n",[Text.UTF8Encoding]::new($false))
        $null = Run 'C/XLang AOT XML oracle' $python @('-I','release/extlibs/sitemap/test/check_external.py',(Join-Path $work 'sitemap_gcc.txt'),$xlFile)
    }
    $passed = $true
    if ($SkipAot) {Write-Output 'sitemap Windows C/XLang (AOT skipped) gate: PASS'}
    else {Write-Output 'sitemap Windows full C/XLang/AOT gate: PASS'}
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    VerifyInputs
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($safeRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        !([IO.Path]::GetFileName($resolved)).StartsWith('sitemap_guard_')) {throw 'Unsafe sitemap cleanup path'}
    [ordered]@{passed=$passed;inputs=$hashes;checks=$checks} | ConvertTo-Json -Depth 10 |
        Set-Content ($work + '.report.json') -Encoding UTF8
    if ($passed) {Remove-Item -LiteralPath $resolved -Recurse -Force}
    else {Write-Warning "sitemap failure artifacts retained: $resolved"}
}
