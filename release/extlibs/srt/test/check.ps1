param([switch]$SkipAot)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg='release/extlibs/srt'
$workRoot=Join-Path $root 'build/test'
$work=Join-Path $workRoot ('srt_guard_'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$cc=if($env:CC){$env:CC}else{'gcc'}
$cmd=Get-Command $cc -CommandType Application -ErrorAction SilentlyContinue|Select-Object -First 1
if($cmd){$cc=$cmd.Source}
$xl=Join-Path $root 'release/xl.exe'
$utf8=[Text.UTF8Encoding]::new($false)
$checks=[Collections.Generic.List[object]]::new()
$inputs=[ordered]@{}
$environment=@{XLANG_PACKAGE_PATH=Join-Path $root 'release/extlibs'}
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',"-I$pkg/include")
$links=@('-lws2_32','-ladvapi32','-lm')
$outcome='failed';$failure='';$decls=@()
function Paths {
    Get-ChildItem -LiteralPath (Join-Path $root $pkg) -Recurse -File|ForEach-Object FullName
    foreach($p in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
        'test/cases/extlib_srt.xl','release/examples/extlibs/srt.xl','release/examples/extlibs/srt_pair.xl',
        'release/examples/extlibs/srt_c.c')){Join-Path $root $p}
    if(Test-Path -LiteralPath $cc -PathType Leaf){$cc}
}
foreach($p in @(Paths|Sort-Object -Unique)){$inputs[$p]=(Get-FileHash -LiteralPath $p).Hash}
function Normalize([string]$text){$text.Replace("`r`n","`n").TrimEnd("`n")}
function Run([string]$name,[string]$program,[string[]]$arguments){
    if($null -eq $arguments){$arguments=@()}
    $r=Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    Write-Host "[srt] ${name}: $($r.status)"
    if($r.status -ne 'pass'){throw "${name}: $($r.status) exit=$($r.exitCode)`n$($r.stderr)`n$($r.stdout)"}
    return Normalize $r.stdout
}
function RunC([string]$name,[string]$source){
    Run $name 'powershell' @('-NoProfile','-ExecutionPolicy','Bypass','-File','tools/run_extlib_c.ps1','-Package','srt','-Source',$source)
}
try {
    $null=Run 'compiler version' $cc @('--version')
    $manifest=Get-Content -LiteralPath (Join-Path $root "$pkg/xlang.package.json") -Raw|ConvertFrom-Json
    $defined=@([regex]::Matches([IO.File]::ReadAllText((Join-Path $root "$pkg/examples/normalize_c.c")),'(?m)^#define (XRT_MODULE_[A-Z_]+)\s*$')|ForEach-Object{$_.Groups[1].Value}|Sort-Object)
    if($defined.Count -ne 3 -or @(Compare-Object @($manifest.artifact.xrtModules|Sort-Object) $defined).Count){throw 'Minimal modules differ from manifest'}
    $golden=Normalize ([IO.File]::ReadAllText((Join-Path $root "$pkg/examples/normalize.expected.txt")))
    $minimal=Join-Path $work 'minimal.exe'
    $null=Run 'manifest-only build' $cc ($flags+@('-DXRT_IMPLEMENTATION',"$pkg/examples/normalize_c.c")+$links+@('-o',$minimal))
    if((Run 'manifest-only example' $minimal @()) -cne $golden){throw 'Minimal example differs'}
    $runtime=Join-Path $work 'runtime.o'
    $null=Run 'selected runtime' $cc ($flags+@('-c',"$pkg/test/xrt_runtime.c",'-o',$runtime))
    $native=Join-Path $work 'native.o'
    $null=Run 'native build' $cc ($flags+@('-c',"$pkg/srt_native.c",'-o',$native))
    $null=Run 'static ABI contract' $cc ($flags+@('-fsyntax-only',"$pkg/test/abi_contract.c"))
    $target=Join-Path $work 'boundaries.exe'
    $null=Run 'legacy C build' $cc ($flags+@('-DNDEBUG',"$pkg/test/test_srt.c",$native,$runtime)+$links+@('-o',$target))
    if((Run 'legacy C' $target @()) -cne 'srt C boundaries: PASS'){throw 'C boundaries incomplete'}
    $null=RunC 'public C boundary runner' "$pkg/test/test_srt.c"
    $budgetSource="$pkg/test/owned_results_c.c"
    $budget=Join-Path $work 'budgets.exe'
    $null=Run 'C budget build' $cc ($flags+@('-DNDEBUG',$budgetSource,$runtime)+$links+@('-o',$budget))
    if((Run 'C result budgets' $budget @()) -cne 'srt C owned results: 22 budgets, PASS'){throw 'C budget coverage incomplete'}
    $costs=Run 'C fixed allocation costs' $budget @('--costs')
    if($costs -cne "0`t1`n1`t1`n2`t1`n3`t3`n4`t2`n5`t8"){throw 'Native allocation costs changed'}
    if((Run 'TCC C allocation costs' $xl @('run',$budgetSource,'--','--costs')) -cne $costs){throw 'TCC C costs differ'}
    $module=Join-Path $work 'owned.c'
    $null=Run 'whole-return cgen' $xl @('cgen',"$pkg/test/owned_probe.xl",'--emit',$module)
    $generated=[IO.File]::ReadAllText($module)
    $bodies=@([regex]::Matches($generated,'(?ms)^static xlir_result_string xlir_fn_\d+\([^\n]*\)\r?\n\{.*?^\}'))
    if($bodies.Count -ne 3){throw 'Owned probe must contain three return wrappers'}
    foreach($symbol in @('build','build_vtt','format_time')){
        if([regex]::Matches($generated,('\b\w+\.Data\s*=\s*xsrt_'+$symbol+'\s*\(')).Count -ne 1){throw "Direct adoption absent: $symbol"}
    }
    foreach($body in $bodies){if($body.Value -match 'xlir_string_copy\(|xl6_xrt_take_cstr\(|xl6CStringCopyV1'){throw 'Owned return copied or transferred twice'}}
    if($generated.Contains('xl6_xrt_take_cstr(')){throw 'Obsolete transfer helper remains'}
    # parseTime 的错误消息仍可能复制输入，不能误称整个文件没有复制。
    $decls=@($generated -split "`n"|Where-Object{$_ -match '^extern .*\bxsrt_\w+\('}|ForEach-Object{$_.TrimEnd("`r")}|Sort-Object -Unique)
    $binding=[IO.File]::ReadAllText((Join-Path $root "$pkg/src/srt.xl"))
    $expectedNames=@([regex]::Matches($binding,'extern "(xsrt_\w+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxsrt_\w+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    if($decls.Count -ne 6 -or @(Compare-Object $expectedNames $actual).Count){throw 'Incomplete generated ABI'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"srt_native.h`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'six actual prototypes' $cc ($flags+@('-fsyntax-only',$abi))
    $legacyC='release/examples/extlibs/srt_c.c'
    $legacy=RunC 'legacy C example runner' $legacyC
    if((Run 'legacy C direct' $xl @('run',$legacyC)) -cne $legacy){throw 'Legacy C direct differs'}
    if((Run 'legacy bilingual JIT' $xl @('run','release/examples/extlibs/srt_pair.xl')) -cne $legacy){throw 'Legacy bilingual differs'}
    $example="$pkg/examples/normalize_c.c"
    $cExample=Join-Path $work 'normalize.exe'
    $null=Run 'normalize C build' $cc ($flags+@('-DNDEBUG',$example,$runtime)+$links+@('-o',$cExample))
    if((Run 'normalize C' $cExample @()) -cne $golden -or (Run 'normalize C direct' $xl @('run',$example)) -cne $golden){throw 'Normalize C golden differs'}
    $owned="$pkg/test/owned_results.xl";$probe="$pkg/test/owned_probe.xl";$language="$pkg/examples/normalize.xl"
    $ownedOutput=Run 'language result budgets' $xl @('run',$owned)
    if($ownedOutput -cne 'srt owned results: 22 budgets, PASS'){throw 'Language budgets differ'}
    $null=Run 'owned probe JIT' $xl @('run',$probe)
    if((Run 'normalize language' $xl @('run',$language)) -cne $golden){throw 'Normalize language differs'}
    $core="$pkg/test/core_aot.xl"
    if(!$SkipAot){
        foreach($input in @($budgetSource,$owned,$probe,$language,$example,$legacyC,'release/examples/extlibs/srt_pair.xl')){
            $target=Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null=Run "$input build" $xl @('build',$input,'-o',$target)
            $text=Run "$input AOT" $target @()
            if($input -eq $budgetSource){if($text -cne 'srt C owned results: 22 budgets, PASS' -or (Run 'AOT native allocation costs' $target @('--costs')) -cne $costs){throw 'C budget AOT differs'}}
            elseif($input -eq $owned -and $text -cne $ownedOutput){throw 'Owned AOT differs'}
            elseif(($input -eq $language -or $input -eq $example) -and $text -cne $golden){throw 'Normalize AOT differs'}
            elseif(($input -eq $legacyC -or $input -eq 'release/examples/extlibs/srt_pair.xl') -and $text -cne $legacy){throw 'Legacy AOT differs'}
            elseif($input -eq $probe -and $text.Length){throw 'Silent AOT emitted output'}
        }
    }
    # 原语言数组构建仍是正例，不用解析配置的成功替代它。
    foreach($input in @('test/cases/extlib_srt.xl',"$pkg/test/boundaries.xl","$pkg/test/allocations.xl",$core,'release/examples/extlibs/srt.xl')){
        $null=Run "$input complete JIT" $xl @('run',$input)
        if(!$SkipAot){$target=Join-Path $work 'complete.exe';$null=Run "$input complete build" $xl @('build',$input,'-o',$target);$null=Run "$input complete AOT" $target @()}
    }
    $outcome='passed'
}catch{$failure=$_.Exception.Message;throw}finally{
    $changed=@($inputs.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $inputs[$_]})
    $changed+=@(Paths|Where-Object{!$inputs.Contains($_)})
    if($changed.Count){$outcome='invalid-input-change'}
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{outcome=$outcome;failure=$failure;aotSkipped=[bool]$SkipAot;
        inputs=$inputs;changedInputs=$changed;generatedPrototypes=$decls.Count;checks=$checks.ToArray()}|ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[srt] report: $work.json"
    if($outcome -eq 'passed'){
        $safe=[IO.Path]::GetFullPath($work);$base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
        if(!$safe.StartsWith($base,[StringComparison]::OrdinalIgnoreCase) -or ![IO.Path]::GetFileName($safe).StartsWith('srt_guard_')){throw 'Unsafe SRT work path'}
        Get-ChildItem -LiteralPath $safe -File|ForEach-Object{Remove-Item -LiteralPath $_.FullName -Force}
        Remove-Item -LiteralPath $safe -Force
    }
    if($changed.Count){throw 'SRT gate inputs changed'}
}
