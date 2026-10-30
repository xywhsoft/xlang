param([switch]$SkipAot)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg=Join-Path $root 'release/extlibs/toml'
$workRoot=Join-Path $root 'build/test/toml_guard'
$work=Join-Path $workRoot ('run_'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work)|Out-Null
$compiler=if($env:CC){$env:CC}else{'gcc'}
$command=Get-Command $compiler -CommandType Application -ErrorAction SilentlyContinue|Select-Object -First 1
$gcc=if($command){$command.Source}else{$compiler}
$interpreter=if($env:PYTHON){$env:PYTHON}else{'python'}
$command=Get-Command $interpreter -CommandType Application -ErrorAction SilentlyContinue|Select-Object -First 1
$python=if($command){$command.Source}else{$interpreter}
$xl=Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$utf8=[Text.UTF8Encoding]::new($false)
$checks=[Collections.Generic.List[object]]::new()
$hashes=[ordered]@{}
$environment=@{XLANG_PACKAGE_PATH=Join-Path $root 'release/extlibs';PYTHONDONTWRITEBYTECODE='1'}
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/toml')
$links=@('-lWs2_32','-lAdvapi32','-lm')
$outcome='failed'; $failure=''
function Normalize([string]$value){$value.Replace("`r`n","`n").TrimEnd("`n")}
function InputPaths {
    Get-ChildItem -LiteralPath $pkg,(Join-Path $root 'release/extlibs/_tomlc17'),(Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    foreach($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','test/cases/extlib_toml.xl',
        'release/examples/extlibs/toml.xl','release/examples/extlibs/toml_c.c')){Join-Path $root $path}
    foreach($path in @($gcc,$python)){if(Test-Path -LiteralPath $path -PathType Leaf){$path}}
}
foreach($path in @(InputPaths|Sort-Object -Unique)){$hashes[$path]=(Get-FileHash -LiteralPath $path).Hash}
function Run([string]$name,[string]$program,[string[]]$arguments,[int]$seconds=120){
    # PowerShell 的空分支会变成 null；它不代表一个空字符串实参。
    if($null -eq $arguments){$arguments=@()}
    $result=Invoke-ExtlibSmokeProcess $program $arguments $root $seconds $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if($result.status -ne 'pass'){throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)"}
    Write-Host "[toml] ${name}: PASS"
    return Normalize $result.stdout
}
function Records([string]$name,[string]$program,[string[]]$arguments){
    $text=Run "$name calendar records" $program $arguments
    $path=Join-Path $work ($name+'.tsv')
    [IO.File]::WriteAllText($path,$text+"`n",$utf8)
    $null=Run "$name independent calendar oracle" $python @('release/extlibs/toml/test/check_calendar_records.py',$path)
    return $text
}
Push-Location $root
try {
    $null=Run 'compiler version' $gcc @('--version')
    $null=Run 'Python tomllib' $python @('-c','import sys,tomllib; print(sys.version)')
    $manifest=Get-Content -LiteralPath (Join-Path $pkg 'xlang.package.json') -Raw|ConvertFrom-Json
    $modules=@($manifest.artifact.xrtModules|ForEach-Object{"-D$_"})
    $defined=@([regex]::Matches([IO.File]::ReadAllText((Join-Path $pkg 'xrt_toml_internal.h')),'(?m)^#define (XRT_MODULE_[A-Z_]+)\s*$')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    if($defined.Count -ne 12 -or @(Compare-Object @($manifest.artifact.xrtModules|Sort-Object) $defined).Count){throw 'TOML module metadata drift'}
    $vendor=[IO.File]::ReadAllText((Join-Path $root 'release/extlibs/_tomlc17/VENDOR.md'))
    if($vendor -notmatch ('Local `src/tomlc17.c` SHA-256 \(canonical LF\): `'+(Get-FileHash 'release/extlibs/_tomlc17/src/tomlc17.c').Hash+'`')){throw 'Vendor checksum differs'}
    $null=Run 'public header' $gcc ($flags+@('-x','c','-fsyntax-only','release/extlibs/toml/include/xrt_toml.h'))
    foreach($source in $manifest.artifact.cSources){$null=Run "$source manifest syntax" $gcc ($flags+$modules+@('-fsyntax-only',('release/extlibs/toml/'+$source)))}
    $null=Run 'legacy ABI contract' $gcc ($flags+@('-fsyntax-only','release/extlibs/toml/test/abi_contract.c'))
    $manifestProbe=Join-Path $work 'manifest.exe'
    $null=Run 'manifest-only XRT and native bridge compile' $gcc ($flags+$modules+@('-DXRT_IMPLEMENTATION','release/extlibs/toml/test/manifest_probe.c')+$links+@('-o',$manifestProbe))
    $null=Run 'manifest-only native bridge' $manifestProbe @()
    $runtime=Join-Path $work 'runtime.o'
    $null=Run 'selected XRT compile' $gcc ($flags+@('-c','release/extlibs/toml/test/xrt_runtime.c','-o',$runtime))
    $expected='toml ownership/calendar: 288 checks, 0 failures'
    foreach($name in @('test_toml','ownership_bounds_c')){
        $source="release/extlibs/toml/test/$name.c"
        $binary=Join-Path $work ($name+'.exe')
        $null=Run "$name compile" $gcc ($flags+@($source,$runtime)+$links+@('-o',$binary))
        $arguments=if($name -eq 'test_toml'){@((Join-Path $work 'nested.toml'))}else{@()}
        $output=Run $name $binary $arguments
        if($name -eq 'ownership_bounds_c' -and $output -cne $expected){throw 'Incomplete ownership matrix'}
    }
    $bounds=Join-Path $work 'ownership_bounds_c.exe'
    foreach($point in 0..3){$output=Run "GCC cold $point" $bounds @('--cold',[string]$point)
        if($output -cne 'toml cold startup: PASS'){throw 'Cold startup did not recover'}}
    $records=Records 'gcc' $bounds @('--records')
    $null=Run 'Python nested table oracle' $python @('release/extlibs/toml/test/check_external.py','--nested',(Join-Path $work 'nested.toml'))
    $source='release/extlibs/toml/test/ownership_bounds_c.c'
    $output=Run 'ownership matrix direct C' $xl @('run',$source)
    if($output -cne $expected){throw 'TCC matrix incomplete'}
    foreach($point in 0..3){$null=Run "TCC cold $point" $xl @('run',$source,'--','--cold',[string]$point)}
    $direct=Records 'tcc' $xl @('run',$source,'--','--records')
    if($direct -cne $records){throw 'GCC/TCC calendar bytes differ'}
    $null=Run 'legacy core direct C' $xl @('run','release/extlibs/toml/test/test_toml.c')
    $examples=@(@('release/examples/extlibs/toml_c.c','basic'),@('release/extlibs/toml/examples/calendar_guard_c.c','calendar_guard'))
    $goldens=@{}
    foreach($pair in $examples){
        $binary=Join-Path $work ($pair[1]+'.exe')
        $null=Run "$($pair[1]) C compile" $gcc ($flags+@($pair[0],$runtime)+$links+@('-o',$binary))
        $c=Run "$($pair[1]) GCC" $binary @()
        $direct=Run "$($pair[1]) direct C" $xl @('run',$pair[0])
        if($c -cne $direct){throw 'C compiler outputs differ'}
        if($pair[1] -eq 'calendar_guard'){
            $golden=Normalize ([IO.File]::ReadAllText((Join-Path $pkg 'examples/calendar_guard.expected.txt')))
            if($c -cne $golden){throw 'Calendar golden differs'}
        }
        $goldens[$pair[0]]=$c
    }
    $language='release/extlibs/toml/examples/calendar_guard.xl'
    $goldens[$language]=$goldens['release/extlibs/toml/examples/calendar_guard_c.c']
    $output=Run 'calendar XLang golden' $xl @('run',$language)
    if($output -cne $goldens[$language]){throw 'Bilingual calendar golden differs'}
    $calendar='release/extlibs/toml/test/calendar.xl'
    $null=Run 'XLang calendar boundaries' $xl @('run',$calendar)
    $probe=Join-Path $work 'probe.c'
    $null=Run 'complete API cgen' $xl @('cgen',$language,'--emit',$probe)
    $text=[IO.File]::ReadAllText($probe)
    if($text -cnotmatch 'xl6_xrt_take_cstr\('){throw 'Owned text is no longer adopted'}
    $decls=@($text -split "`r?`n"|Where-Object{$_ -match '^extern .*\bxtoml_[a-z_]+\('}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxtoml_[a-z_]+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    $binding=[IO.File]::ReadAllText((Join-Path $pkg 'src/toml.xl'))
    $names=@([regex]::Matches($binding,'extern "(xtoml_[A-Za-z0-9_]+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    if($decls.Count -ne 16 -or @(Compare-Object $names $actual).Count){throw 'Incomplete generated TOML prototypes'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"toml_native.c`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'sixteen actual prototypes' $gcc ($flags+@('-fsyntax-only',$abi))
    if(-not $SkipAot){
        $sources=@($source,'release/extlibs/toml/test/test_toml.c',$calendar,$language)
        foreach($pair in $examples){$sources+=$pair[0]}
        foreach($input in $sources){
            $binary=Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null=Run "$input build" $xl @('build',$input,'-o',$binary)
            $output=Run "$input AOT" $binary @()
            if($goldens.ContainsKey($input) -and $output -cne $goldens[$input]){throw 'AOT golden differs'}
            if($input -eq $source){
                if($output -cne $expected){throw 'AOT matrix incomplete'}
                foreach($point in 0..3){$null=Run "AOT cold $point" $binary @('--cold',[string]$point)}
                $aot=Records 'aot' $binary @('--records')
                if($aot -cne $records){throw 'AOT calendar bytes differ'}
            }
        }
    }
    # 原语言创建的字典保持必过；从 parse 得到的配置可写回不能代替 M1。
    foreach($input in @('test/cases/extlib_toml.xl','release/extlibs/toml/test/root_argument_repro.xl',
        'release/extlibs/toml/test/boundaries.xl','release/extlibs/toml/test/allocations.xl',
        'release/extlibs/toml/test/aot_core.xl','release/examples/extlibs/toml.xl')){
        $output=Run "$input complete JIT" $xl @('run',$input)
        if($input -eq 'release/examples/extlibs/toml.xl' -and $output -cne $goldens['release/examples/extlibs/toml_c.c']){throw 'Complete bilingual output differs'}
        if(-not $SkipAot){$binary=Join-Path $work 'complete.exe'; $null=Run "$input complete build" $xl @('build',$input,'-o',$binary); $null=Run "$input complete AOT" $binary @()}
    }
    $cFile=Join-Path $work 'c.toml'; $xlFile=Join-Path $work 'xlang.toml'
    $c=Run 'C file example' (Join-Path $work 'basic.exe') @($cFile)
    $languageFile=Run 'XLang file example' $xl @('run','release/examples/extlibs/toml.xl','--',$xlFile)
    if($c -cne $languageFile -or (Get-FileHash $cFile).Hash -cne (Get-FileHash $xlFile).Hash){throw 'Bilingual saved files differ'}
    $null=Run 'Python file interop' $python @('release/extlibs/toml/test/check_external.py',$cFile,$xlFile)
    $outcome='passed'
} catch {$failure=$_.Exception.Message;throw} finally {
    Pop-Location
    $changed=@($hashes.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_]})
    $changed+=@(InputPaths|Where-Object{!$hashes.Contains($_)})
    if($changed.Count){$outcome='invalid-input-change'}
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())}|ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[toml] report: $work.json"
    $base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $resolved=[IO.Path]::GetFullPath($work)
    if(!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'TOML work path escaped build/test'}
    if($outcome -eq 'passed'){Remove-Item -LiteralPath $resolved -Recurse -Force}
    if($changed.Count){throw 'TOML inputs changed during gate'}
}
