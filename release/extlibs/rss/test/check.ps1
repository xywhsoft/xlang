param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$safeRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $safeRoot ('rss_guard_' + [guid]::NewGuid().ToString('N'))
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$shell = (Get-Process -Id $PID).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$passed = $false
$inputs = @('rss','xml.dom','xml.sax','_expat') | ForEach-Object {
    Get-ChildItem (Join-Path $root "release/extlibs/$_") -Recurse -File | ForEach-Object FullName
}
$inputs += @(Get-ChildItem (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName)
$inputs += @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h','tools/run_extlib_c.ps1','tools/extlib_smoke_process.ps1',
    'tools/xlir_native_process.ps1','test/cases/extlib_rss.xl',
    'release/examples/extlibs/rss_c.c','release/examples/extlibs/rss_pair.xl',
    'release/examples/extlibs/rss.xl') | ForEach-Object {Join-Path $root $_}
$hashes = @{}
foreach ($path in @($inputs) + @($gcc,$shell)) {$hashes[$path] = (Get-FileHash -LiteralPath $path).Hash}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Ilib/xrt',
    '-Irelease/extlibs/rss/include','-Irelease/extlibs/xml.dom','-Irelease/extlibs/xml.sax',
    '-Irelease/extlibs/_expat/vendor/expat/lib','-Irelease/extlibs/_expat/vendor/expat')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$sources = @('release/extlibs/_expat/vendor/expat/lib/xmlparse.c',
    'release/extlibs/_expat/vendor/expat/lib/xmlrole.c','release/extlibs/_expat/vendor/expat/lib/xmltok.c',
    'release/extlibs/_expat/expat_random.c','release/extlibs/xml.sax/xrt_xml_common.c',
    'release/extlibs/xml.sax/xrt_xml_sax.c','release/extlibs/xml.sax/xml_sax_native.c',
    'release/extlibs/xml.dom/xrt_xml_dom_tree.c','release/extlibs/xml.dom/xrt_xml_dom_parser.c',
    'release/extlibs/xml.dom/xrt_xml_dom_writer.c','release/extlibs/xml.dom/xml_dom_native.c',
    'release/extlibs/rss/rss_parse.c','release/extlibs/rss/rss_build.c')
$cArgs = @($sources)
foreach ($include in @('lib/xrt','release/extlibs/rss/include','release/extlibs/xml.dom',
    'release/extlibs/xml.sax','release/extlibs/_expat/vendor/expat/lib','release/extlibs/_expat/vendor/expat')) {
    $cArgs += @('--inc-path',$include)
}

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
    Write-Host "[rss] ${name}: PASS"
    return ($result.stdout + $result.stderr).Replace("`r`n","`n").TrimEnd("`n")
}

function VerifyInputs {
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {throw "rss input changed: $path"}
    }
}

[void][IO.Directory]::CreateDirectory($work)
Push-Location $root
try {
    $null = Run 'GCC version' $gcc @('--version')
    $cc1 = Run 'GCC cc1 path' $gcc @('-print-prog-name=cc1')
    $hashes[$cc1] = (Get-FileHash -LiteralPath $cc1).Hash
    $hashes | ConvertTo-Json | Set-Content (Join-Path $work 'inputs.before.json') -Encoding UTF8
    $null = Run 'strict manifest syntax' $gcc ($flags + @('-Werror','-fsyntax-only',
        '-DXRT_MODULE_BUFFER','-DXRT_MODULE_UNICODE','-DXRT_MODULE_VALUE_CONTAINER',
        'release/extlibs/rss/rss_parse.c','release/extlibs/rss/rss_build.c'))
    $null = Run 'strict fixture syntax' $gcc ($flags + @('-Werror','-fsyntax-only',
        'release/extlibs/rss/test/test_rss.c','release/extlibs/rss/test/test_link_relations.c',
        'release/examples/extlibs/rss_c.c','release/extlibs/rss/examples/atom_author_c.c',
        'release/extlibs/rss/examples/atom_links_c.c'))
    $objects = @()
    $allSources = @('release/extlibs/rss/test/runtime.c') + $sources
    for ($i = 0; $i -lt $allSources.Count; ++$i) {
        $object = Join-Path $work ("unit_$i.o")
        $null = Run "C compile $($allSources[$i])" $gcc ($flags + @('-c',$allSources[$i],'-o',$object))
        $objects += $object
    }
    foreach ($name in @('test_rss','test_link_relations')) {
        $target = Join-Path $work ($name + '.exe')
        $null = Run "GCC $name build" $gcc ($flags + @('-Werror',"release/extlibs/rss/test/$name.c") + $objects + $links + @('-o',$target))
        $null = Run "GCC $name" $target @()
        $null = Run "TCC $name JIT" $runner @('-Package','rss','-Source',"release/extlibs/rss/test/$name.c")
    }
    $null = Run 'XLang relations JIT' $xl @('run','release/extlibs/rss/test/link_relations.xl')
    $null = Run 'XLang implicit alternate JIT' $xl @('run','release/extlibs/rss/test/implicit_alternate.xl')
    $null = Run 'XLang input limit JIT' $xl @('run','release/extlibs/rss/test/input_limit.xl')

    $generated = Join-Path $work 'generated.c'
    $null = Run 'actual ABI cgen' $xl @('cgen','test/cases/extlib_rss.xl','--emit',$generated)
    $prototypes = [regex]::Matches([IO.File]::ReadAllText($generated),'(?m)^extern [^\r\n;]*\bxrss_(?:detect|parse|parse_rss|parse_atom|build_rss|build_atom)\([^;\r\n]*\);')
    if ($prototypes.Count -ne 6) {throw 'All six actual rss C prototypes must be checked'}
    $abi = Join-Path $work 'abi.c'
    $abiText = '#include "rss_native.h"' + "`n" + (($prototypes | ForEach-Object Value) -join "`n")
    [IO.File]::WriteAllText($abi,$abiText,[Text.UTF8Encoding]::new($false))
    $null = Run 'six actual prototypes' $gcc ($flags + @('-Werror','-fsyntax-only',$abi))
    $wrong = $abiText -replace '(?m)^extern [^\r\n;]*\bxrss_parse\(', 'extern int xrss_parse('
    if ($wrong -ceq $abiText) {throw 'Wrong ABI control did not change declaration'}
    [IO.File]::WriteAllText($abi,$wrong,[Text.UTF8Encoding]::new($false))
    $null = Run 'wrong parse return rejected' $gcc ($flags + @('-Werror','-fsyntax-only',$abi)) 1
    if ($checks[-1].result.stderr -notmatch 'conflicting types') {throw 'Wrong ABI failed for an unrelated reason'}

    $exampleSources = @('release/examples/extlibs/rss_c.c','release/extlibs/rss/examples/atom_author_c.c',
        'release/extlibs/rss/examples/atom_links_c.c')
    $exampleXl = @('release/examples/extlibs/rss_pair.xl','release/extlibs/rss/examples/atom_author.xl',
        'release/extlibs/rss/examples/atom_links.xl')
    $goldens = @("type: rss`nitems: 1","Editor`nSource",
        [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/rss/examples/atom_links.expected.txt')).Replace("`r`n","`n").TrimEnd("`n"))
    for ($i = 0; $i -lt 3; ++$i) {
        $target = Join-Path $work ("example_$i.exe")
        $null = Run "GCC example $i build" $gcc ($flags + @('-Werror',$exampleSources[$i]) + $objects + $links + @('-o',$target))
        $output = Run "GCC example $i" $target @()
        if ($output -cne $goldens[$i]) {throw "GCC example $i golden mismatch"}
        $output = Run "C example $i JIT" $runner @('-Package','rss','-Source',$exampleSources[$i])
        if ($output -cne $goldens[$i]) {throw "C example $i JIT golden mismatch"}
        # 第一组语言字典构造仍须在完整门禁通过；解析范例先独立验证。
        if ($i -gt 0) {
            $output = Run "XLang example $i JIT" $xl @('run',$exampleXl[$i])
            if ($output -cne $goldens[$i]) {throw "XLang example $i JIT golden mismatch"}
        }
        if (!$SkipAot) {
            $target = Join-Path $work ("example_c_aot_$i.exe")
            $null = Run "C example $i AOT build" $xl (@('build',$exampleSources[$i]) + $cArgs + @('-o',$target))
            $output = Run "C example $i AOT" $target @()
            if ($output -cne $goldens[$i]) {throw "C example $i AOT golden mismatch"}
            if ($i -gt 0) {
                $target = Join-Path $work ("example_xl_aot_$i.exe")
                $null = Run "XLang example $i AOT build" $xl @('build',$exampleXl[$i],'-o',$target)
                $output = Run "XLang example $i AOT" $target @()
                if ($output -cne $goldens[$i]) {throw "XLang example $i AOT golden mismatch"}
            }
        }
    }
    if (!$SkipAot) {
        $target = Join-Path $work 'relations_c.exe'
        $null = Run 'C relations AOT build' $xl (@('build','release/extlibs/rss/test/test_link_relations.c') + $cArgs + @('-o',$target))
        $null = Run 'C relations AOT' $target @()
        $target = Join-Path $work 'relations_xl.exe'
        $null = Run 'XLang relations AOT build' $xl @('build','release/extlibs/rss/test/link_relations.xl','-o',$target)
        $null = Run 'XLang relations AOT' $target @()
    }
    VerifyInputs
    # 原正例必须保持成功预期；共享 M1 不得改为预期失败。
    foreach ($source in @('test/cases/extlib_rss.xl','release/extlibs/rss/test/author_inheritance.xl')) {
        $null = Run $source $xl @('run',$source)
    }
    $output = Run 'RSS XLang example' $xl @('run',$exampleXl[0])
    if ($output -cne $goldens[0]) {throw 'RSS XLang golden mismatch'}
    $null = Run 'extended XLang example' $xl @('run','release/examples/extlibs/rss.xl')
    if (!$SkipAot) {
        $target = Join-Path $work 'core.exe'
        $null = Run 'original core AOT build' $xl @('build','release/extlibs/rss/test/aot_core.xl','-o',$target)
        $null = Run 'original core AOT' $target @()
    }
    $passed = $true
    if ($SkipAot) {Write-Output 'rss Windows C/XLang (AOT skipped) gate: PASS'}
    else {Write-Output 'rss Windows full C/XLang/AOT gate: PASS'}
} catch {
    [IO.File]::WriteAllText((Join-Path $work 'failure.txt'),$_.Exception.Message,[Text.UTF8Encoding]::new($false))
    throw
} finally {
    Pop-Location
    VerifyInputs
    $resolved = [IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($safeRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        !([IO.Path]::GetFileName($resolved)).StartsWith('rss_guard_')) {throw 'Unsafe rss cleanup path'}
    [ordered]@{passed=$passed;inputs=$hashes;checks=$checks} | ConvertTo-Json -Depth 10 |
        Set-Content ($work + '.report.json') -Encoding UTF8
    if ($passed) {Remove-Item -LiteralPath $resolved -Recurse -Force}
    else {Write-Warning "rss failure artifacts retained: $resolved"}
}
