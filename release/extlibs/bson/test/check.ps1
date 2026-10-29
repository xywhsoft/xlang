param([switch]$SkipAot)

$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg=Join-Path $root 'release/extlibs/bson'
$workRoot=Join-Path $root 'build/test/bson_guard'
$work=Join-Path $workRoot ('run_'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work)|Out-Null
$compiler=if($env:CC){$env:CC}else{'gcc'}
$command=Get-Command $compiler -CommandType Application -ErrorAction SilentlyContinue|Select-Object -First 1
$gcc=if($command){$command.Source}else{$compiler}
$xl=Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$utf8=[Text.UTF8Encoding]::new($false)
$checks=[Collections.Generic.List[object]]::new()
$hashes=[ordered]@{}
$environment=@{XLANG_PACKAGE_PATH=Join-Path $root 'release/extlibs'}
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/bson')
$links=@('-lWs2_32','-lAdvapi32','-lm')
$outcome='failed'
$failure=''
function Normalize([string]$value){$value.Replace("`r`n","`n").TrimEnd("`n")}
function InputPaths {
    Get-ChildItem -LiteralPath $pkg,(Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    foreach($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
        'test/cases/extlib_bson.xl','release/examples/extlibs/bson.xl')){Join-Path $root $path}
    if(Test-Path -LiteralPath $gcc -PathType Leaf){$gcc}
}
foreach($path in @(InputPaths|Sort-Object -Unique)){$hashes[$path]=(Get-FileHash -LiteralPath $path).Hash}
function Run([string]$name,[string]$program,[string[]]$arguments,[int]$seconds=120,[int]$exit=0){
    $result=Invoke-ExtlibSmokeProcess $program $arguments $root $seconds $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;expectedExit=$exit;result=$result})
    if($result.exitCode -ne $exit -or $result.status -notin @('pass','fail')){
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)"
    }
    Write-Host "[bson] ${name}: PASS"
    return Normalize $result.stdout
}
Push-Location $root
try {
    $null=Run 'compiler version' $gcc @('--version')
    $null=Run 'C integer ABI' $gcc ($flags+@('-fsyntax-only','release/extlibs/bson/test/abi_contract.c'))
    $null=Run 'public header' $gcc ($flags+@('-x','c','-fsyntax-only','release/extlibs/bson/bson_native.h'))
    $manifest=Get-Content -LiteralPath (Join-Path $pkg 'xlang.package.json') -Raw|ConvertFrom-Json
    $modules=@($manifest.artifact.xrtModules|ForEach-Object{"-D$_"})
    $nativeSource='release/extlibs/bson/bson_native.c'
    $null=Run 'manifest syntax' $gcc ($flags+$modules+@('-fsyntax-only',$nativeSource))
    $runtime=Join-Path $work 'runtime.o'
    $native=Join-Path $work 'native.o'
    $null=Run 'selected XRT compile' $gcc ($flags+@('-c','release/extlibs/bson/test/xrt_runtime.c','-o',$runtime))
    $null=Run 'native compile' $gcc ($flags+@('-c',$nativeSource,'-o',$native))
    $numericExpected='bson numeric reads: 1080 vectors, 72 OOM paths, 0 failures'
    foreach($name in @('test_bson_smoke','test_boundaries','numeric_reads_c')){
        $source="release/extlibs/bson/test/$name.c"
        $binary=Join-Path $work ($name+'.exe')
        $objects=@($runtime)
        if($name -ne 'numeric_reads_c'){$objects+=$native}
        $null=Run "$name compile" $gcc ($flags+@($source)+$objects+$links+@('-o',$binary))
        $arguments=if($name -eq 'test_boundaries'){@($work)}else{@()}
        $output=Run $name $binary $arguments
        if($name -eq 'numeric_reads_c' -and $output -cne $numericExpected){throw 'Incomplete numeric vectors'}
    }
    $numeric='release/extlibs/bson/test/numeric_reads_c.c'
    $output=Run 'numeric vectors direct C' $xl @('run',$numeric)
    if($output -cne $numericExpected){throw 'TCC numeric vectors incomplete'}
    $examples=@(@('bson_c.c','basic'),@('numeric_fields_c.c','numeric_fields'))
    $goldens=@{}
    foreach($pair in $examples){
        $source='release/extlibs/bson/examples/'+$pair[0]
        $expected=Normalize ([IO.File]::ReadAllText((Join-Path $pkg ("examples/$($pair[1]).expected.txt"))))
        $goldens[$source]=$expected
        $binary=Join-Path $work ($pair[1]+'.exe')
        $null=Run "$($pair[1]) C compile" $gcc ($flags+@($source,$runtime)+$links+@('-o',$binary))
        $c=Run "$($pair[1]) GCC" $binary @()
        $direct=Run "$($pair[1]) direct C" $xl @('run',$source)
        if($c -cne $expected -or $direct -cne $expected){throw 'C golden output differs'}
    }
    $basic=Join-Path $work 'basic.exe'
    $null=Run 'C example argument failure' $basic @('extra','extra') 30 1
    $null=Run 'C example write failure' $basic @((Join-Path $work 'missing/output.bson')) 30 1
    $cFile=Join-Path $work 'c.bson'
    $output=Run 'C save/load example' $basic @($cFile)
    if($output -cne ($goldens['release/extlibs/bson/examples/bson_c.c']+"`nfile=ok") -or !(Test-Path -LiteralPath $cFile)){
        throw 'C save/load output or file missing'
    }
    $language='release/extlibs/bson/examples/numeric_fields.xl'
    $goldens[$language]=$goldens['release/extlibs/bson/examples/numeric_fields_c.c']
    $output=Run 'numeric XLang example' $xl @('run',$language)
    if($output -cne $goldens[$language]){throw 'Bilingual numeric golden differs'}
    $paths='release/extlibs/bson/test/numeric_reads.xl'
    $null=Run 'XLang numeric boundaries' $xl @('run',$paths)
    $probe=Join-Path $work 'probe.c'
    $null=Run 'complete API cgen' $xl @('cgen',$language,'--emit',$probe)
    $text=[IO.File]::ReadAllText($probe)
    if($text -cnotmatch 'xl6_xrt_take_cstr\('){throw 'Owned hex/ObjectId strings no longer adopted'}
    $decls=@($text -split "`r?`n"|Where-Object{$_ -match '^extern .*\bxbson_[a-z_]+\('}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxbson_[a-z_]+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    $binding=[IO.File]::ReadAllText((Join-Path $pkg 'src/bson.xl'))
    $expected=@([regex]::Matches($binding,'extern "(xbson_[A-Za-z0-9_]+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    if($decls.Count -ne 15 -or @(Compare-Object $expected $actual).Count){throw 'Incomplete generated BSON prototypes'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"bson_native.c`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'fifteen actual prototypes' $gcc ($flags+@('-fsyntax-only',$abi))
    if(-not $SkipAot){
        $sources=@($numeric,$paths,$language)
        foreach($pair in $examples){$sources+='release/extlibs/bson/examples/'+$pair[0]}
        foreach($source in $sources){
            $binary=Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null=Run "$source build" $xl @('build',$source,'-o',$binary)
            $output=Run "$source AOT" $binary @()
            if($goldens.ContainsKey($source) -and $output -cne $goldens[$source]){throw 'AOT golden differs'}
            if($source -eq $numeric -and $output -cne $numericExpected){throw 'AOT numeric vectors incomplete'}
        }
    }
    # 原始语言字典始终必过；外部 bytes 解码成功不代替语言字典编码能力。
    foreach($source in @('test/cases/extlib_bson.xl','release/extlibs/bson/test/aot_core.xl',
        'release/extlibs/bson/test/boundaries.xl','release/extlibs/bson/test/bson_test.xl',
        'release/extlibs/bson/test/allocations.xl','release/extlibs/bson/examples/bson.xl','release/examples/extlibs/bson.xl')){
        $output=Run "$source complete JIT" $xl @('run',$source)
        if($source -eq 'release/extlibs/bson/examples/bson.xl' -and $output -cne $goldens['release/extlibs/bson/examples/bson_c.c']){
            throw 'Complete bilingual output differs'
        }
        if(-not $SkipAot){
            $binary=Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null=Run "$source complete build" $xl @('build',$source,'-o',$binary)
            $null=Run "$source complete AOT" $binary @()
        }
    }
    $xlFile=Join-Path $work 'xlang.bson'
    $output=Run 'XLang save/load example' $xl @('run','release/extlibs/bson/examples/bson.xl','--',$xlFile)
    if($output -cne ($goldens['release/extlibs/bson/examples/bson_c.c']+"`nfile=ok") -or
        (Get-FileHash -LiteralPath $cFile).Hash -cne (Get-FileHash -LiteralPath $xlFile).Hash){throw 'Saved bilingual files differ'}
    $outcome='passed'
} catch {$failure=$_.Exception.Message;throw} finally {
    Pop-Location
    $changed=@($hashes.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_]})
    $changed+=@(InputPaths|Where-Object{!$hashes.Contains($_)})
    if($changed.Count){$outcome='invalid-input-change'}
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())}|ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[bson] report: $work.json"
    $base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $resolved=[IO.Path]::GetFullPath($work)
    if(!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'BSON work path escaped build/test'}
    if($outcome -eq 'passed'){Remove-Item -LiteralPath $resolved -Recurse -Force}
    if($changed.Count){throw 'BSON inputs changed during gate'}
}
