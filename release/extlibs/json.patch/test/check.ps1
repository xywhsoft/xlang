param([switch]$SkipAot)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg=Join-Path $root 'release/extlibs/json.patch'
$workRoot=Join-Path $root 'build/test/json_patch_guard'
$work=Join-Path $workRoot ('run_'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work)|Out-Null
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$compiler=if($env:CC){$env:CC}else{'gcc'}
$command=Get-Command $compiler -CommandType Application -ErrorAction SilentlyContinue|Select-Object -First 1
if($command){$compiler=$command.Source}
$xl=Join-Path $root 'release/xl.exe'
$utf8=[Text.UTF8Encoding]::new($false)
$checks=[Collections.Generic.List[object]]::new();$hashes=[ordered]@{}
$environment=@{PYTHONDONTWRITEBYTECODE='1';XLANG_PACKAGE_PATH=Join-Path $root 'release/extlibs'}
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/json.patch/include')
$links=@('-lWs2_32','-lAdvapi32','-lm')
$outcome='failed';$failure=''
function InputPaths {
    Get-ChildItem -LiteralPath $pkg,(Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    foreach($p in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','test/cases/extlib_json_patch.xl',
        'release/examples/extlibs/json_patch.xl')){Join-Path $root $p}
    if(Test-Path -LiteralPath $compiler -PathType Leaf){$compiler}
}
foreach($p in @(InputPaths|Sort-Object -Unique)){$hashes[$p]=(Get-FileHash -LiteralPath $p).Hash}
function Normalize([string]$s){$s.Replace("`r`n","`n").TrimEnd("`n")}
function Run([string]$name,[string]$program,[string[]]$arguments,[int]$seconds=120){
    if($null -eq $arguments){$arguments=@()}
    $r=Invoke-ExtlibSmokeProcess $program $arguments $root $seconds $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    if($r.status -ne 'pass'){throw "${name}: $($r.status) exit=$($r.exitCode)`n$($r.stderr)`n$($r.stdout)"}
    Write-Host "[json.patch] ${name}: PASS"
    return Normalize $r.stdout
}
Push-Location $root
try {
    $null=Run 'compiler version' $compiler @('--version')
    $manifest=Get-Content -Raw (Join-Path $pkg 'xlang.package.json')|ConvertFrom-Json
    if(@($manifest.artifact.xrtModules).Count -ne 2){throw 'Production modules changed'}
    $modules=@($manifest.artifact.xrtModules|ForEach-Object{"-D$_"})
    $native='release/extlibs/json.patch/json_patch_native.c'
    $null=Run 'public header' $compiler ($flags+@('-x','c','-fsyntax-only','release/extlibs/json.patch/include/json_patch.h'))
    $null=Run 'manifest syntax' $compiler ($flags+$modules+@('-fsyntax-only',$native))
    $minimal=Join-Path $work 'minimal.exe'
    $null=Run 'manifest-only build' $compiler ($flags+$modules+@('-DXRT_IMPLEMENTATION','release/extlibs/json.patch/test/manifest_probe.c')+$links+@('-o',$minimal))
    $null=Run 'manifest-only run' $minimal @()
    $runtime=Join-Path $work 'runtime.o'
    $null=Run 'selected XRT compile' $compiler ($flags+@('-c','release/extlibs/json.patch/test/xrt_runtime.c','-o',$runtime))
    $legacy=Join-Path $work 'boundaries.exe'
    $null=Run 'original C compile' $compiler ($flags+@($native,'release/extlibs/json.patch/test/test_boundaries.c',$runtime)+$links+@('-o',$legacy))
    $null=Run 'original C boundaries' $legacy @()
    $oracle=Join-Path $work 'oracle.exe'
    $null=Run 'optional oracle runner compile' $compiler ($flags+@($native,'release/extlibs/json.patch/test/oracle_cases.c',$runtime)+$links+@('-o',$oracle))
    $source='release/extlibs/json.patch/test/move_ownership_c.c';$move=Join-Path $work 'move.exe'
    $null=Run 'move ownership compile' $compiler ($flags+@($source,$runtime)+$links+@('-o',$move))
    $expected='json.patch move: 32 cases, 16 OOM paths, 0 semantic failures, 0 clone-budget failures'
    if((Run 'move ownership' $move @()) -cne $expected){throw 'Incomplete move matrix'}
    $costs=Run 'move allocation costs' $move @('--costs')
    if(@($costs -split "`n").Count -ne 12){throw 'Incomplete allocation corpus'}
    [IO.File]::WriteAllText((Join-Path $work 'gcc-costs.tsv'),$costs+"`n",$utf8)
    if((Run 'move direct C' $xl @('run',$source)) -cne $expected){throw 'Direct move differs'}
    if((Run 'move direct costs' $xl @('run',$source,'--','--costs')) -cne $costs){throw 'Direct costs differ'}
    $goldens=@{}
    foreach($name in @('basic','workflow','relocate')){
        $input="release/extlibs/json.patch/examples/${name}_c.c";$target=Join-Path $work ($name+'.exe')
        $null=Run "$name C compile" $compiler ($flags+@($input,$runtime)+$links+@('-o',$target))
        $output=Run "$name C" $target @()
        if((Run "$name direct C" $xl @('run',$input)) -cne $output){throw 'C example outputs differ'}
        $goldens[$input]=$output
    }
    $language='release/extlibs/json.patch/examples/relocate.xl'
    $golden=Normalize ([IO.File]::ReadAllText((Join-Path $pkg 'examples/relocate.expected.txt')))
    if($goldens['release/extlibs/json.patch/examples/relocate_c.c'] -cne $golden -or
        (Run 'relocate language golden' $xl @('run',$language)) -cne $golden){throw 'Relocate golden differs'}
    $goldens[$language]=$golden
    $probe=Join-Path $work 'probe.c';$null=Run 'complete cgen' $xl @('cgen',$language,'--emit',$probe)
    $text=[IO.File]::ReadAllText($probe)
    $decls=@($text -split "`r?`n"|Where-Object{$_ -match '^extern .*\bxjsonpatch_[a-z_]+\('}|Sort-Object -Unique)
    $names=@([regex]::Matches([IO.File]::ReadAllText((Join-Path $pkg 'src/json_patch.xl')),'extern "(xjsonpatch_[a-z_]+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxjsonpatch_[a-z_]+(?=\()')|ForEach-Object Value|Sort-Object)
    if($decls.Count -ne 8 -or @(Compare-Object $names $actual).Count){throw 'Incomplete actual prototypes'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"json_patch.h`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'eight actual prototypes' $compiler ($flags+@('-fsyntax-only',$abi))
    if(!$SkipAot){
        foreach($input in @($source,$language,'release/extlibs/json.patch/examples/basic_c.c',
            'release/extlibs/json.patch/examples/workflow_c.c','release/extlibs/json.patch/examples/relocate_c.c')){
            $target=Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null=Run "$input build" $xl @('build',$input,'-o',$target)
            $output=Run "$input AOT" $target @()
            if($input -eq $source){
                if($output -cne $expected -or (Run 'move AOT costs' $target @('--costs')) -cne $costs){throw 'AOT move differs'}
            }elseif($output -cne $goldens[$input]){throw 'AOT golden differs'}
        }
    }
    # 原语言字面量投影继续必过，标准 JSON 解析出的原生值不替代它。
    foreach($input in @('test/cases/extlib_json_patch.xl','release/extlibs/json.patch/test/boundaries.xl',
        'release/extlibs/json.patch/test/allocations.xl','release/extlibs/json.patch/test/aot_core.xl',
        'release/extlibs/json.patch/examples/basic.xl','release/extlibs/json.patch/examples/workflow.xl',
        'release/examples/extlibs/json_patch.xl')){
        $output=Run "$input complete JIT" $xl @('run',$input)
        if($input -like '*/examples/basic.xl' -and $output -cne $goldens['release/extlibs/json.patch/examples/basic_c.c']){throw 'Complete basic differs'}
        if($input -like '*/examples/workflow.xl' -and $output -cne $goldens['release/extlibs/json.patch/examples/workflow_c.c']){throw 'Complete workflow differs'}
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
    Write-Host "[json.patch] report: $work.json"
    $safe=[IO.Path]::GetFullPath($work);$base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    if(!$safe.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe JSON Patch work path'}
    if($outcome -eq 'passed'){Remove-Item -LiteralPath $safe -Recurse -Force}
    if($changed.Count){throw 'JSON Patch inputs changed during gate'}
}
