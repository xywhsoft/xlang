param([switch]$SkipAot)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg=Join-Path $root 'release/extlibs/fix'
$workRoot=Join-Path $root 'build/test/fix_guard'
$work=Join-Path $workRoot ('run_'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work)|Out-Null
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$compiler=if($env:CC){$env:CC}else{'gcc'}
$python=if($env:PYTHON){$env:PYTHON}else{'python'}
foreach($name in @('compiler','python')){
    $value=Get-Variable $name -ValueOnly
    $command=Get-Command $value -CommandType Application -ErrorAction SilentlyContinue|Select-Object -First 1
    if($command){Set-Variable $name $command.Source}
}
$xl=Join-Path $root 'release/xl.exe'
$utf8=[Text.UTF8Encoding]::new($false)
$checks=[Collections.Generic.List[object]]::new()
$hashes=[ordered]@{}
$environment=@{PYTHONDONTWRITEBYTECODE='1';XLANG_PACKAGE_PATH=Join-Path $root 'release/extlibs'}
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/fix')
$links=@('-lWs2_32','-lAdvapi32','-lm')
$outcome='failed';$failure=''
function InputPaths {
    Get-ChildItem -LiteralPath $pkg,(Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    foreach($p in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','test/cases/extlib_fix.xl','release/examples/extlibs/fix.xl')){Join-Path $root $p}
    foreach($p in @($compiler,$python)){if(Test-Path -LiteralPath $p -PathType Leaf){$p}}
}
foreach($p in @(InputPaths|Sort-Object -Unique)){$hashes[$p]=(Get-FileHash -LiteralPath $p).Hash}
function Normalize([string]$s){$s.Replace("`r`n","`n").TrimEnd("`n")}
function Run([string]$name,[string]$program,[string[]]$arguments,[int]$seconds=120){
    if($null -eq $arguments){$arguments=@()}
    $r=Invoke-ExtlibSmokeProcess $program $arguments $root $seconds $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    if($r.status -ne 'pass'){throw "${name}: $($r.status) exit=$($r.exitCode)`n$($r.stderr)`n$($r.stdout)"}
    Write-Host "[fix] ${name}: PASS"
    return Normalize $r.stdout
}
function Records([string]$name,[string]$program,[string[]]$arguments){
    $text=Run "$name tag records" $program $arguments
    $path=Join-Path $work ($name+'.tsv')
    [IO.File]::WriteAllText($path,$text+"`n",$utf8)
    $null=Run "$name independent grammar" $python @('release/extlibs/fix/test/check_tag_records.py',$path)
    return $text
}
Push-Location $root
try {
    $null=Run 'compiler version' $compiler @('--version')
    $null=Run 'Python version' $python @('-c','import sys; assert sys.version_info >= (3,10); print(sys.version)')
    $manifest=Get-Content -Raw (Join-Path $pkg 'xlang.package.json')|ConvertFrom-Json
    $defined=@([regex]::Matches([IO.File]::ReadAllText((Join-Path $pkg 'examples/fix_c.c')),'(?m)^#define (XRT_MODULE_[A-Z_]+)\s*$')|ForEach-Object{$_.Groups[1].Value}|Sort-Object)
    if($defined.Count -ne 3 -or @(Compare-Object @($manifest.artifact.xrtModules|Sort-Object) $defined).Count){throw 'Minimal runtime modules differ from manifest'}
    $modules=@($manifest.artifact.xrtModules|ForEach-Object{"-D$_"})
    $null=Run 'public header' $compiler ($flags+@('-x','c','-fsyntax-only','release/extlibs/fix/fix_native.h'))
    $null=Run 'manifest syntax' $compiler ($flags+$modules+@('-fsyntax-only','release/extlibs/fix/fix_native.c'))
    $minimal=Join-Path $work 'minimal.exe'
    $null=Run 'three-module independent build' $compiler ($flags+@('-DXRT_IMPLEMENTATION','release/extlibs/fix/examples/fix_c.c')+$links+@('-o',$minimal))
    $golden=Run 'three-module independent example' $minimal @()
    $null=Run 'independent frame' $python @('release/extlibs/fix/test/check_frame.py',$golden)
    $runtime=Join-Path $work 'runtime.o'
    $null=Run 'selected XRT compile' $compiler ($flags+@('-c','release/extlibs/fix/test/xrt_runtime.c','-o',$runtime))
    $legacy=Join-Path $work 'legacy.exe'
    $null=Run 'legacy C compile' $compiler ($flags+@('release/extlibs/fix/test/test_fix.c','release/extlibs/fix/fix_native.c',$runtime)+$links+@('-o',$legacy))
    $null=Run 'legacy C' $legacy @()
    $source='release/extlibs/fix/test/tag_grammar_c.c';$binary=Join-Path $work 'grammar.exe'
    $null=Run 'tag grammar compile' $compiler ($flags+@($source,$runtime)+$links+@('-o',$binary))
    $expected='fix tag grammar: 374 cases, 4 OOM paths, 0 failures'
    if((Run 'tag grammar' $binary @()) -cne $expected){throw 'Incomplete tag matrix'}
    $records=Records 'gcc' $binary @('--records')
    if((Run 'tag grammar direct C' $xl @('run',$source)) -cne $expected){throw 'Direct grammar differs'}
    if((Records 'tcc' $xl @('run',$source,'--','--records')) -cne $records){throw 'Direct records differ'}
    $direct=Run 'legacy direct C example' $xl @('run','release/extlibs/fix/examples/fix_c.c')
    if($direct -cne $golden){throw 'Legacy C example differs'}
    $cExample='release/extlibs/fix/examples/tag_gate_c.c'
    $example=Join-Path $work 'tag_example.exe'
    $null=Run 'tag C example compile' $compiler ($flags+@($cExample,$runtime)+$links+@('-o',$example))
    $tagGolden=Normalize ([IO.File]::ReadAllText((Join-Path $pkg 'examples/tag_gate.expected.txt')))
    if((Run 'tag C example' $example @()) -cne $tagGolden -or
        (Run 'tag direct C example' $xl @('run',$cExample)) -cne $tagGolden){throw 'C tag golden differs'}
    $language='release/extlibs/fix/examples/tag_gate.xl'
    if((Run 'tag language example' $xl @('run',$language)) -cne $tagGolden){throw 'Language tag golden differs'}
    $boundaries='release/extlibs/fix/test/tag_grammar.xl'
    $null=Run 'tag language boundaries' $xl @('run',$boundaries)
    $probe=Join-Path $work 'probe.c'
    $null=Run 'complete API cgen' $xl @('cgen',$language,'--emit',$probe)
    $text=[IO.File]::ReadAllText($probe)
    if($text -cnotmatch 'xl6_xrt_take_cstr\('){throw 'Build text is no longer adopted'}
    $decls=@($text -split "`r?`n"|Where-Object{$_ -match '^extern .*\bxfix_[a-z_]+\('}|Sort-Object -Unique)
    $names=@([regex]::Matches([IO.File]::ReadAllText((Join-Path $pkg 'src/fix.xl')),'extern "(xfix_[a-z_]+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxfix_[a-z_]+(?=\()')|ForEach-Object Value|Sort-Object)
    if($decls.Count -ne 8 -or @(Compare-Object $names $actual).Count){throw 'Incomplete FIX prototypes'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"fix_native.c`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'eight actual prototypes' $compiler ($flags+@('-fsyntax-only',$abi))
    if(!$SkipAot){
        foreach($input in @($source,$boundaries,$language,$cExample,'release/extlibs/fix/examples/fix_c.c')){
            $target=Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null=Run "$input build" $xl @('build',$input,'-o',$target)
            $output=Run "$input AOT" $target @()
            if($input -eq $source){
                if($output -cne $expected -or (Records 'aot' $target @('--records')) -cne $records){throw 'AOT grammar differs'}
            }elseif($input -eq $language -or $input -eq $cExample){if($output -cne $tagGolden){throw 'AOT tag golden differs'}}
            elseif($input -like '*/fix_c.c' -and $output -cne $golden){throw 'Legacy AOT golden differs'}
        }
    }
    # 保留原语言创建数组的正例；解析收到的报文不等于 build 的数组投影已闭环。
    foreach($input in @('test/cases/extlib_fix.xl','release/extlibs/fix/test/boundaries.xl',
        'release/extlibs/fix/test/limits.xl','release/extlibs/fix/test/allocations.xl','release/extlibs/fix/test/aot_core.xl','release/examples/extlibs/fix.xl')){
        $output=Run "$input complete JIT" $xl @('run',$input)
        if($input -eq 'release/examples/extlibs/fix.xl' -and $output -cne $golden){throw 'Complete bilingual example differs'}
        if(!$SkipAot){$target=Join-Path $work 'complete.exe';$null=Run "$input complete build" $xl @('build',$input,'-o',$target);$null=Run "$input complete AOT" $target @()}
    }
    $outcome='passed'
}catch{$failure=$_.Exception.Message;throw}finally{
    Pop-Location
    $changed=@($hashes.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_]})
    $changed+=@(InputPaths|Where-Object{!$hashes.Contains($_)})
    if($changed.Count){$outcome='invalid-input-change'}
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{outcome=$outcome;failure=$failure;aotSkipped=[bool]$SkipAot;
        inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())}|ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[fix] report: $work.json"
    $safe=[IO.Path]::GetFullPath($work);$base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(!$safe.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe FIX work path'}
    if($outcome -eq 'passed'){Remove-Item -LiteralPath $safe -Recurse -Force}
    if($changed.Count){throw 'FIX inputs changed during gate'}
}
