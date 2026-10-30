param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg = Join-Path $root 'release/extlibs/netrc'
$workRoot = Join-Path $root 'build/test/netrc_guard'
$work = Join-Path $workRoot ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$compiler = if ($env:CC) { $env:CC } else { 'gcc' }
$command = Get-Command $compiler -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$gcc = if ($command) { $command.Source } else { $compiler }
$interpreter = if ($env:PYTHON) { $env:PYTHON } else { 'python' }
$command = Get-Command $interpreter -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$python = if ($command) { $command.Source } else { $interpreter }
$xl = Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$environment = @{XLANG_PACKAGE_PATH=Join-Path $root 'release/extlibs'; PYTHONDONTWRITEBYTECODE='1'}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/netrc')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$outcome = 'failed'
$failure = ''
function Normalize([string]$value) { $value.Replace("`r`n","`n").TrimEnd("`n") }
function InputPaths {
    Get-ChildItem -LiteralPath $pkg,(Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','test/cases/extlib_netrc.xl',
        'release/examples/extlibs/netrc.xl','release/examples/extlibs/netrc_c.c')) { Join-Path $root $path }
    foreach ($path in @($gcc,$python)) { if (Test-Path -LiteralPath $path -PathType Leaf) { $path } }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path]=(Get-FileHash -LiteralPath $path).Hash }
function Run([string]$name,[string]$program,[string[]]$arguments,[int]$seconds=120,[string]$directory=$root) {
    $result=Invoke-ExtlibSmokeProcess $program $arguments $directory $seconds $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;directory=$directory;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[netrc] ${name}: PASS"
    return Normalize $result.stdout
}
Push-Location $root
try {
    $null=Run 'compiler version' $gcc @('--version')
    $null=Run 'Python version' $python @('--version')
    $null=Run 'public header' $gcc ($flags+@('-x','c','-fsyntax-only','release/extlibs/netrc/include/xrt_netrc.h'))
    $manifest=Get-Content -LiteralPath (Join-Path $pkg 'xlang.package.json') -Raw|ConvertFrom-Json
    $modules=@($manifest.artifact.xrtModules|ForEach-Object{"-D$_"})
    foreach ($source in $manifest.artifact.cSources) {
        $path='release/extlibs/netrc/'+$source
        $null=Run "manifest syntax $source" $gcc ($flags+$modules+@('-fsyntax-only',$path))
    }
    # 独立 GCC 测试与范例共用一份运行库对象，不重复编译 XRT。
    $runtime=Join-Path $work 'runtime.o'
    $null=Run 'XRT compile' $gcc ($flags+@('-c','release/extlibs/netrc/test/xrt_runtime.c','-o',$runtime)) 180
    $expectedChecks=@{resource_limits_c='netrc resource budgets: 285 checks, 0 failures';
        stream_errors_c='netrc stream first cause: 126 checks, 0 failures'}
    foreach ($name in @('test_netrc','test_netrc_boundaries','test_native_tls','resource_limits_c','stream_errors_c')) {
        $source="release/extlibs/netrc/test/$name.c"
        $binary=Join-Path $work ($name+'.exe')
        $null=Run "$name compile" $gcc ($flags+@($source,$runtime)+$links+@('-o',$binary))
        $output=Run $name $binary @() 120 $work
        if ($expectedChecks.ContainsKey($name) -and $output -cne $expectedChecks[$name]) { throw 'Incomplete netrc regression checks' }
    }
    foreach ($name in @('resource_limits_c','stream_errors_c')) {
        $source="release/extlibs/netrc/test/$name.c"
        $output=Run "$name direct C" $xl @('run',$source)
        if ($output -cne $expectedChecks[$name]) { throw 'TCC regression output differs' }
    }
    $examples=@(@('release/extlibs/netrc/examples/basic.c','release/examples/extlibs/netrc.xl','basic'),
        @('release/extlibs/netrc/examples/bounded_credentials_c.c','release/extlibs/netrc/examples/bounded_credentials.xl','bounded_credentials'))
    $goldens=@{}
    foreach ($pair in $examples) {
        $expected=Normalize ([IO.File]::ReadAllText((Join-Path $pkg ("examples/$($pair[2]).expected.txt"))))
        $goldens[$pair[0]]=$expected; $goldens[$pair[1]]=$expected
        $binary=Join-Path $work ($pair[2]+'_c.exe')
        $null=Run "$($pair[2]) C compile" $gcc ($flags+@($pair[0],$runtime)+$links+@('-o',$binary))
        $c=Run "$($pair[2]) C" $binary @()
        $direct=Run "$($pair[2]) direct C" $xl @('run',$pair[0])
        $language=Run "$($pair[2]) XLang" $xl @('run',$pair[1])
        if ($c -cne $expected -or $direct -cne $expected -or $language -cne $expected) { throw 'Bilingual golden output differs' }
    }
    $central='release/examples/extlibs/netrc_c.c'
    $goldens[$central]=$goldens[$examples[0][0]]
    if ((Run 'central C example' $xl @('run',$central)) -cne $goldens[$central]) { throw 'Central C golden differs' }
    $cPath=Join-Path $work 'c.netrc'
    $xlPath=Join-Path $work '凭据.netrc'
    $c=Run 'C file example' (Join-Path $work 'basic_c.exe') @($cPath)
    $language=Run 'XLang UTF-8 file example' $xl @('run','release/examples/extlibs/netrc.xl','--',$xlPath)
    if ($c -cne $goldens[$central] -or $language -cne $c -or
        (Get-FileHash -LiteralPath $cPath).Hash -cne (Get-FileHash -LiteralPath $xlPath).Hash) { throw 'Bilingual credential files differ' }
    $null=Run 'Python netrc interop' $python @('release/extlibs/netrc/test/interop.py',$cPath,$xlPath)
    $owned='release/extlibs/netrc/test/owned_text.xl'
    $null=Run 'owned text JIT' $xl @('run',$owned)
    $probe=Join-Path $work 'probe.c'
    $null=Run 'complete API cgen' $xl @('cgen','release/examples/extlibs/netrc.xl','--emit',$probe)
    $text=[IO.File]::ReadAllText($probe)
    if ($text -cnotmatch 'extern char\* xnetrc_error_message\(' -or
        $text -cnotmatch '\bv\d+\.Data\s*=\s*xnetrc_error_message\s*\(' -or
        $text -cnotmatch 'xl6_xrt_take_cstr\(') { throw 'Netrc owned result adoption changed' }
    $decls=@($text -split "`r?`n"|Where-Object{$_ -match '^extern .*\bxnetrc_[a-z_]+\('}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxnetrc_[a-z_]+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    $binding=[IO.File]::ReadAllText((Join-Path $pkg 'src/netrc.xl'))
    $expected=@([regex]::Matches($binding,'extern "(xnetrc_[a-z_]+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    if ($decls.Count -ne 9 -or $expected.Count -ne 9 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete generated netrc prototypes' }
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"netrc_native.c`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'nine actual prototypes' $gcc ($flags+@('-fsyntax-only',$abi))
    $languageSources=@('test/cases/extlib_netrc.xl','release/extlibs/netrc/test/boundaries.xl',
        'release/extlibs/netrc/test/resource_limits.xl','release/extlibs/netrc/test/allocations.xl',$owned)
    foreach ($source in $languageSources) { $null=Run "$source JIT" $xl @('run',$source) }
    if (-not $SkipAot) {
        $sources=@('release/extlibs/netrc/test/resource_limits_c.c','release/extlibs/netrc/test/stream_errors_c.c',$central)+$languageSources
        foreach ($pair in $examples) { $sources+=@($pair[0],$pair[1]) }
        foreach ($source in $sources) {
            $binary=Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null=Run "$source build" $xl @('build',$source,'-o',$binary)
            $output=Run "$source AOT" $binary @()
            $name=[IO.Path]::GetFileNameWithoutExtension($source)
            if ($goldens.ContainsKey($source) -and $output -cne $goldens[$source]) { throw 'AOT golden differs' }
            if ($expectedChecks.ContainsKey($name) -and $output -cne $expectedChecks[$name]) { throw 'AOT regressions incomplete' }
        }
    }
    # M1 是必过正例，留在最后便于收集不依赖语言字典构造的完整验证。
    $named='release/extlibs/netrc/test/named_type.xl'
    $null=Run 'named type complete JIT' $xl @('run',$named)
    if (-not $SkipAot) {
        $binary=Join-Path $work 'named.exe'
        $null=Run 'named type complete build' $xl @('build',$named,'-o',$binary)
        $null=Run 'named type complete AOT' $binary @()
    }
    $outcome='passed'
} catch {
    $failure=$_.Exception.Message
    throw
} finally {
    Pop-Location
    $changed=@($hashes.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_]})
    $changed+=@(InputPaths|Where-Object{!$hashes.Contains($_)})
    if ($changed.Count) { $outcome='invalid-input-change' }
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())}|ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[netrc] report: $work.json"
    $base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $resolved=[IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'Netrc work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $resolved -Recurse -Force }
    if ($changed.Count) { throw 'Netrc inputs changed during gate' }
}
