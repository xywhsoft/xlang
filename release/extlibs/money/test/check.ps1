param([switch]$SkipAot)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg='release/extlibs/money'
$workRoot=Join-Path $root 'build/test/money_guard'
$work=Join-Path $workRoot ('run_'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$cc=if($env:CC){$env:CC}else{'gcc'}
$python=if($env:PYTHON){$env:PYTHON}else{'python'}
foreach($name in @('cc','python')){
    $cmd=Get-Command (Get-Variable $name -ValueOnly) -CommandType Application -ErrorAction SilentlyContinue|Select-Object -First 1
    if($cmd){Set-Variable $name $cmd.Source}
}
$xl=Join-Path $root 'release/xl.exe'
$utf8=[Text.UTF8Encoding]::new($false)
$checks=[Collections.Generic.List[object]]::new()
$inputs=[ordered]@{}
$environment=@{XLANG_PACKAGE_PATH=Join-Path $root 'release/extlibs';PYTHONDONTWRITEBYTECODE='1'}
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',"-I$pkg/include")
$links=@('-lws2_32','-ladvapi32','-lm')
$outcome='failed';$failure='';$decls=@()
function Paths {
    Get-ChildItem -LiteralPath (Join-Path $root $pkg) -Recurse -File|ForEach-Object FullName
    foreach($p in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xlc.dll','tools/extlib_smoke_process.ps1',
        'tools/xlir_native_process.ps1','test/cases/extlib_money.xl','test/cases/extlib_money_boundaries.xl',
        'release/examples/extlibs/money.xl')){Join-Path $root $p}
    foreach($p in @($cc,$python)){if(Test-Path -LiteralPath $p -PathType Leaf){$p}}
}
foreach($p in @(Paths|Sort-Object -Unique)){$inputs[$p]=(Get-FileHash -LiteralPath $p).Hash}
function Normalize([string]$text){$text.Replace("`r`n","`n").TrimEnd("`n")}
function Run([string]$name,[string]$program,[string[]]$arguments){
    if($null -eq $arguments){$arguments=@()}
    $r=Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    Write-Host "[money] ${name}: $($r.status)"
    if($r.status -ne 'pass'){throw "${name}: $($r.status) exit=$($r.exitCode)`n$($r.stderr)`n$($r.stdout)"}
    return Normalize $r.stdout
}
function Record([string]$name,[string]$program,[string[]]$arguments){
    $text=Run "$name records" $program $arguments
    $path=Join-Path $work ($name+'.tsv')
    [IO.File]::WriteAllText($path,$text+"`n",$utf8)
    $null=Run "$name integer oracle" $python @("$pkg/test/check_boxed_records.py",$path)
    return $text
}
try {
    $null=Run 'compiler version' $cc @('--version')
    $null=Run 'Python version' $python @('-c','import sys; print(sys.version)')
    $golden=Normalize (Get-Content -LiteralPath (Join-Path $root "$pkg/examples/money.expected.txt") -Raw -Encoding UTF8)
    $settlement=Normalize (Get-Content -LiteralPath (Join-Path $root "$pkg/examples/settlement.expected.txt") -Raw -Encoding UTF8)
    $manifest=Get-Content -LiteralPath (Join-Path $root "$pkg/xlang.package.json") -Raw|ConvertFrom-Json
    $defined=@([regex]::Matches((Get-Content -LiteralPath (Join-Path $root "$pkg/examples/settlement_c.c") -Raw),'(?m)^#define (XRT_MODULE_[A-Z_]+)\s*$')|ForEach-Object{$_.Groups[1].Value}|Sort-Object)
    if($defined.Count -ne 2 -or @(Compare-Object @($manifest.artifact.xrtModules|Sort-Object) $defined).Count){throw 'Minimal modules differ from manifest'}
    $minimal=Join-Path $work 'minimal.exe'
    $null=Run 'manifest-only build' $cc ($flags+@('-DXRT_IMPLEMENTATION',"$pkg/examples/settlement_c.c")+$links+@('-o',$minimal))
    if((Run 'manifest-only example' $minimal @()) -cne $settlement){throw 'Minimal settlement differs'}
    $runtime=Join-Path $work 'runtime.o'
    $null=Run 'runtime build' $cc ($flags+@('-c',"$pkg/test/xrt_runtime.c",'-o',$runtime))
    $native=Join-Path $work 'native.o'
    $null=Run 'native build' $cc ($flags+@('-c',"$pkg/money_native.c",'-o',$native))
    $test=Join-Path $work 'test_money.exe'
    $null=Run 'legacy C build' $cc ($flags+@('-DNDEBUG',"$pkg/test/test_money.c",$native,$runtime)+$links+@('-o',$test))
    $null=Run 'legacy C' $test @()
    $boxed=Join-Path $work 'boxed.exe'
    $source="$pkg/test/boxed_records_c.c"
    $null=Run 'boxed build' $cc ($flags+@('-DNDEBUG',$source,$runtime)+$links+@('-o',$boxed))
    $summary='money boxed: 657 cases, 5 allocation paths, PASS'
    if((Run 'boxed boundary and allocation' $boxed @()) -cne $summary){throw 'Incomplete boxed corpus'}
    $records=Record 'gcc' $boxed @('--records')
    if((Record 'direct' $xl @('run',$source,'--','--records')) -cne $records){throw 'GCC/TCC records differ'}
    $owned="$pkg/test/owned_format.xl"
    $null=Run 'owned JIT' $xl @('run',$owned)
    $ownedC=Join-Path $work 'owned.c'
    $null=Run 'owned cgen' $xl @('cgen',$owned,'--emit',$ownedC)
    $generated=Get-Content -LiteralPath $ownedC -Raw
    if($generated -cnotmatch 'extern char\* xm_format\(' -or $generated -cnotmatch '\bv\d+\.Data\s*=\s*xm_format\s*\('){throw 'Owned return not directly adopted'}
    $module=Join-Path $work 'all.c'
    $null=Run 'all API cgen' $xl @('cgen','test/cases/extlib_money.xl','--emit',$module)
    $decls=@(Get-Content -LiteralPath $module|Where-Object{$_ -match '^extern .*\bxm_\w+\('}|Sort-Object -Unique)
    $expectedNames=@([regex]::Matches((Get-Content -LiteralPath (Join-Path $root "$pkg/src/money.xl") -Raw),'extern "(xm_\w+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxm_\w+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    if($decls.Count -ne 9 -or @(Compare-Object $expectedNames $actual).Count){throw 'Incomplete generated ABI'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"money_native.h`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'nine generated prototypes' $cc ($flags+@('-fsyntax-only',$abi))
    foreach($input in @("$pkg/examples/money.c","$pkg/examples/settlement_c.c")){
        $target=Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($input)+'.exe')
        $null=Run "$input GCC build" $cc ($flags+@('-DNDEBUG',$input,$runtime)+$links+@('-o',$target))
        $expected=if($input -like '*/settlement_c.c'){$settlement}else{$golden}
        if((Run "$input GCC run" $target @()) -cne $expected -or (Run "$input direct C" $xl @('run',$input)) -cne $expected){throw 'C golden differs'}
    }
    $inputsToRun=@('test/cases/extlib_money.xl','test/cases/extlib_money_boundaries.xl',"$pkg/test/error_kinds.xl",
        "$pkg/examples/settlement.xl",'release/examples/extlibs/money.xl')
    foreach($input in $inputsToRun){
        $text=Run "$input JIT" $xl @('run',$input)
        if($input -like '*/settlement.xl' -and $text -cne $settlement){throw 'XLang settlement differs'}
        if($input -eq 'release/examples/extlibs/money.xl' -and $text -cne $golden){throw 'Legacy golden differs'}
    }
    if(!$SkipAot){
        foreach($input in (@($source,$owned,"$pkg/test/aot_core.xl","$pkg/examples/money.c","$pkg/examples/settlement_c.c")+$inputsToRun)){
            $target=Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null=Run "$input build" $xl @('build',$input,'-o',$target)
            $text=Run "$input AOT" $target @()
            if($input -eq $source){
                if($text -cne $summary -or (Record 'aot' $target @('--records')) -cne $records){throw 'AOT corpus differs'}
            }elseif($input -like '*/settlement*' -and $text -cne $settlement){throw 'Settlement AOT differs'}
            elseif(($input -eq 'release/examples/extlibs/money.xl' -or $input -like '*/money.c') -and $text -cne $golden){throw 'Legacy AOT differs'}
            elseif(($input -eq $owned -or $input -like '*/aot_core.xl') -and $text.Length){throw 'Silent AOT emitted output'}
        }
    }
    # 原生数组与字面量的相等性仍是必过正例，不改成逐项比较。
    $allocation="$pkg/test/allocations.xl"
    $null=Run 'complete allocation JIT' $xl @('run',$allocation)
    if(!$SkipAot){$target=Join-Path $work 'allocation.exe';$null=Run 'complete allocation build' $xl @('build',$allocation,'-o',$target);$null=Run 'complete allocation AOT' $target @()}
    $outcome='passed'
}catch{$failure=$_.Exception.Message;throw}finally{
    $changed=@($inputs.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $inputs[$_]})
    $changed+=@(Paths|Where-Object{!$inputs.Contains($_)})
    if($changed.Count){$outcome='invalid-input-change'}
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{outcome=$outcome;failure=$failure;aotSkipped=[bool]$SkipAot;
        inputs=$inputs;changedInputs=$changed;generatedPrototypes=$decls.Count;checks=$checks.ToArray()}|ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[money] report: $work.json"
    if($outcome -eq 'passed'){
        $safe=[IO.Path]::GetFullPath($work);$base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
        if(!$safe.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe money work path'}
        Remove-Item -LiteralPath $safe -Recurse -Force
    }
    if($changed.Count){throw 'Money gate inputs changed'}
}
