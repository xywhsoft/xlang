param([switch]$SkipAot)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg='release/extlibs/geohash'
$workRoot=Join-Path $root 'build/test'
$work=Join-Path $workRoot ('geohash_guard_'+[guid]::NewGuid().ToString('N'))
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
    Get-ChildItem -LiteralPath (Join-Path $root $pkg),(Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    foreach($p in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
        'test/cases/extlib_geohash.xl','release/examples/extlibs/geohash.xl')){Join-Path $root $p}
    foreach($p in @($cc,$python)){if(Test-Path -LiteralPath $p -PathType Leaf){$p}}
}
foreach($p in @(Paths|Sort-Object -Unique)){$inputs[$p]=(Get-FileHash -LiteralPath $p).Hash}
function Normalize([string]$text){$text.Replace("`r`n","`n").TrimEnd("`n")}
function Run([string]$name,[string]$program,[string[]]$arguments){
    if($null -eq $arguments){$arguments=@()}
    $r=Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    Write-Host "[geohash] ${name}: $($r.status)"
    if($r.status -ne 'pass'){throw "${name}: $($r.status) exit=$($r.exitCode)`n$($r.stderr)`n$($r.stdout)"}
    return Normalize $r.stdout
}
function RunC([string]$name,[string]$source){
    Run $name 'powershell' @('-NoProfile','-ExecutionPolicy','Bypass','-File','tools/run_extlib_c.ps1','-Package','geohash','-Source',$source)
}
function Record([string]$name,[string]$program,[string[]]$arguments){
    $text=Run "$name records" $program $arguments
    $path=Join-Path $work ($name+'.tsv')
    [IO.File]::WriteAllText($path,$text+"`n",$utf8)
    $null=Run "$name rational oracle" $python @("$pkg/test/check_grid_records.py",$path)
    return $text
}
try {
    $null=Run 'compiler version' $cc @('--version')
    $null=Run 'Python version' $python @('-c','import sys; print(sys.version)')
    $manifest=Get-Content -LiteralPath (Join-Path $root "$pkg/xlang.package.json") -Raw|ConvertFrom-Json
    $defined=@([regex]::Matches([IO.File]::ReadAllText((Join-Path $root "$pkg/examples/dateline_c.c")),'(?m)^#define (XRT_MODULE_[A-Z_]+)\s*$')|ForEach-Object{$_.Groups[1].Value}|Sort-Object)
    if($defined.Count -ne 3 -or @(Compare-Object @($manifest.artifact.xrtModules|Sort-Object) $defined).Count){throw 'Minimal modules differ from manifest'}
    $golden=Normalize ([IO.File]::ReadAllText((Join-Path $root "$pkg/examples/geohash.expected.txt")))
    $dateline=Normalize ([IO.File]::ReadAllText((Join-Path $root "$pkg/examples/dateline.expected.txt")))
    $minimal=Join-Path $work 'minimal.exe'
    $null=Run 'manifest-only build' $cc ($flags+@('-DXRT_IMPLEMENTATION',"$pkg/examples/dateline_c.c")+$links+@('-o',$minimal))
    if((Run 'manifest-only example' $minimal @()) -cne $dateline){throw 'Minimal dateline differs'}
    $runtime=Join-Path $work 'runtime.o'
    $null=Run 'selected runtime' $cc ($flags+@('-c',"$pkg/test/xrt_runtime.c",'-o',$runtime))
    $native=Join-Path $work 'native.o'
    $null=Run 'native build' $cc ($flags+@('-c',"$pkg/geohash_native.c",'-o',$native))
    foreach($name in @('test_geohash','test_threads')){
        $target=Join-Path $work ($name+'.exe')
        $null=Run "$name build" $cc ($flags+@('-DNDEBUG',"$pkg/test/$name.c",$native,$runtime)+$links+@('-o',$target))
        $text=Run $name $target @()
        if($name -eq 'test_threads' -and $text -cne 'geohash threads: PASS'){throw 'Thread checks incomplete'}
    }
    $null=RunC 'public C boundary runner' "$pkg/test/test_geohash.c"
    if((RunC 'public C thread runner' "$pkg/test/test_threads.c") -cne 'geohash threads: PASS'){throw 'Public thread runner differs'}
    $source="$pkg/test/grid_records_c.c"
    $grid=Join-Path $work 'grid.exe'
    $null=Run 'grid build' $cc ($flags+@('-DNDEBUG',$source,$runtime)+$links+@('-o',$grid))
    $summary='geohash grid: 2696 records, PASS'
    if((Run 'grid memory checks' $grid @()) -cne $summary){throw 'Incomplete grid corpus'}
    $records=Record 'gcc' $grid @('--records')
    if((Record 'direct' $xl @('run',$source,'--','--records')) -cne $records){throw 'GCC/TCC records differ'}
    $ownedC=Join-Path $work 'owned.c'
    $null=Run 'whole-return ownership cgen' $xl @('cgen',"$pkg/test/owned_probe.xl",'--emit',$ownedC)
    $generated=[IO.File]::ReadAllText($ownedC)
    foreach($entry in @{encode=1;bbox=1;neighbor=4}.GetEnumerator()){
        if([regex]::Matches($generated,('\b\w+\.Data\s*=\s*xgeohash_'+$entry.Key+'\s*\(')).Count -ne $entry.Value){throw 'Incomplete owned return probe'}
    }
    if($generated.Contains('xl6_xrt_take_cstr(') -or $generated.Contains('xl6CStringCopyV1') -or $generated -match '=\s*xlir_string_copy\s*\('){throw 'Owned text copied again'}
    $module=Join-Path $work 'all.c'
    $null=Run 'all API cgen' $xl @('cgen','test/cases/extlib_geohash.xl','--emit',$module)
    $decls=@(Get-Content -LiteralPath $module|Where-Object{$_ -match '^extern .*\bxgeohash_\w+\('}|Sort-Object -Unique)
    $binding=[IO.File]::ReadAllText((Join-Path $root "$pkg/src/geohash.xl"))
    $expectedNames=@([regex]::Matches($binding,'extern "(xgeohash_\w+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxgeohash_\w+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    if($decls.Count -ne 6 -or @(Compare-Object $expectedNames $actual).Count){throw 'Incomplete generated ABI'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"geohash_native.h`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'six generated prototypes' $cc ($flags+@('-fsyntax-only',$abi))
    foreach($input in @("$pkg/examples/geohash_c.c","$pkg/examples/dateline_c.c")){
        $target=Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($input)+'.exe')
        $extra=if($input -like '*/geohash_c.c'){@($native)}else{@()}
        $null=Run "$input GCC build" $cc ($flags+@('-DNDEBUG',$input,$runtime)+$extra+$links+@('-o',$target))
        $expected=if($input -like '*/dateline_c.c'){$dateline}else{$golden}
        if((Run "$input GCC run" $target @()) -cne $expected -or (Run "$input direct C" $xl @('run',$input)) -cne $expected){throw 'C golden differs'}
    }
    if((RunC 'public C example runner' "$pkg/examples/geohash_c.c") -cne $golden){throw 'Public C example differs'}
    $language=@('test/cases/extlib_geohash.xl',"$pkg/test/boundaries.xl","$pkg/test/allocations.xl",
        "$pkg/test/owned_results.xl","$pkg/test/owned_probe.xl",'release/examples/extlibs/geohash.xl',"$pkg/examples/dateline.xl")
    $outputs=@{}
    foreach($input in $language){$outputs[$input]=Run "$input JIT" $xl @('run',$input)}
    if($outputs['release/examples/extlibs/geohash.xl'] -cne $golden -or $outputs["$pkg/examples/dateline.xl"] -cne $dateline -or
        $outputs["$pkg/test/boundaries.xl"] -cne 'geohash boundaries: PASS' -or $outputs['test/cases/extlib_geohash.xl'].Length){throw 'Language golden differs'}
    if(!$SkipAot){
        foreach($input in (@($source,"$pkg/examples/geohash_c.c","$pkg/examples/dateline_c.c")+$language)){
            $target=Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null=Run "$input build" $xl @('build',$input,'-o',$target)
            $text=Run "$input AOT" $target @()
            if($input -eq $source){
                if($text -cne $summary -or (Record 'aot' $target @('--records')) -cne $records){throw 'AOT grid differs'}
            }elseif($input -like '*/geohash_c.c' -and $text -cne $golden){throw 'C AOT differs'}
            elseif($input -like '*/dateline_c.c' -and $text -cne $dateline){throw 'Dateline C AOT differs'}
            elseif($outputs.ContainsKey($input) -and $text -cne $outputs[$input]){throw 'JIT/AOT output differs'}
        }
    }
    $outcome='passed'
}catch{$failure=$_.Exception.Message;throw}finally{
    $changed=@($inputs.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $inputs[$_]})
    $changed+=@(Paths|Where-Object{!$inputs.Contains($_)})
    if($changed.Count){$outcome='invalid-input-change'}
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{outcome=$outcome;failure=$failure;aotSkipped=[bool]$SkipAot;
        inputs=$inputs;changedInputs=$changed;generatedPrototypes=$decls.Count;checks=$checks.ToArray()}|ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[geohash] report: $work.json"
    if($outcome -eq 'passed'){
        $safe=[IO.Path]::GetFullPath($work);$base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
        if(!$safe.StartsWith($base,[StringComparison]::OrdinalIgnoreCase) -or ![IO.Path]::GetFileName($safe).StartsWith('geohash_guard_')){throw 'Unsafe geohash work path'}
        Get-ChildItem -LiteralPath $safe -File|ForEach-Object{Remove-Item -LiteralPath $_.FullName -Force}
        Remove-Item -LiteralPath $safe -Force
    }
    if($changed.Count){throw 'Geohash gate inputs changed'}
}
