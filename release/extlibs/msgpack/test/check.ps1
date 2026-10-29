param([switch]$SkipAot)

$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg=Join-Path $root 'release/extlibs/msgpack'
$workRoot=Join-Path $root 'build/test/msgpack_guard'
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
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/msgpack')
$links=@('-lWs2_32','-lAdvapi32','-lm')
$outcome='failed'
$failure=''
function Normalize([string]$value){$value.Replace("`r`n","`n").TrimEnd("`n")}
function InputPaths {
    Get-ChildItem -LiteralPath $pkg,(Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    foreach($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
        'test/cases/extlib_msgpack.xl','release/examples/extlibs/msgpack.xl')){Join-Path $root $path}
    foreach($path in @($gcc,$python)){if(Test-Path -LiteralPath $path -PathType Leaf){$path}}
}
foreach($path in @(InputPaths|Sort-Object -Unique)){$hashes[$path]=(Get-FileHash -LiteralPath $path).Hash}
function Run([string]$name,[string]$program,[string[]]$arguments,[int]$seconds=120,[int]$exit=0){
    $result=Invoke-ExtlibSmokeProcess $program $arguments $root $seconds $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;expectedExit=$exit;result=$result})
    if($result.exitCode -ne $exit -or $result.status -notin @('pass','fail')){
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)"
    }
    Write-Host "[msgpack] ${name}: PASS"
    return Normalize $result.stdout
}
Push-Location $root
try {
    $null=Run 'compiler version' $gcc @('--version')
    $null=Run 'Python version' $python @('--version')
    $manifest=Get-Content -LiteralPath (Join-Path $pkg 'xlang.package.json') -Raw|ConvertFrom-Json
    $modules=@($manifest.artifact.xrtModules|ForEach-Object{"-D$_"})
    $nativeSource='release/extlibs/msgpack/msgpack_native.c'
    $null=Run 'public header' $gcc ($flags+$modules+@('-x','c','-fsyntax-only','release/extlibs/msgpack/msgpack_native.h'))
    $null=Run 'manifest syntax' $gcc ($flags+$modules+@('-fsyntax-only',$nativeSource))
    $runtime=Join-Path $work 'runtime.o'
    $native=Join-Path $work 'native.o'
    $null=Run 'selected XRT compile' $gcc ($flags+@('-c','release/extlibs/msgpack/test/xrt_runtime.c','-o',$runtime))
    $null=Run 'native compile' $gcc ($flags+@('-c',$nativeSource,'-o',$native))
    $extensionExpected='msgpack extension framing: 50651 checks, 18 OOM paths, 0 failures'
    foreach($name in @('test_msgpack','test_boundaries','test_allocations','extensions_c')){
        $source="release/extlibs/msgpack/test/$name.c"
        $binary=Join-Path $work ($name+'.exe')
        $objects=@($runtime)
        if($name -ne 'extensions_c'){$objects+=$native}
        $null=Run "$name compile" $gcc ($flags+@($source)+$objects+$links+@('-o',$binary))
        $output=Run $name $binary @()
        if($name -eq 'extensions_c' -and $output -cne $extensionExpected){throw 'Incomplete extension matrix'}
    }
    $records=Run 'GCC timestamp records' (Join-Path $work 'extensions_c.exe') @('--records')
    $recordPath=Join-Path $work 'gcc.tsv'
    [IO.File]::WriteAllText($recordPath,$records+"`n",$utf8)
    $null=Run 'GCC independent timestamp oracle' $python @('release/extlibs/msgpack/test/check_timestamps.py',$recordPath)
    $extensionSource='release/extlibs/msgpack/test/extensions_c.c'
    $output=Run 'extension matrix direct C' $xl @('run',$extensionSource)
    if($output -cne $extensionExpected){throw 'TCC extension matrix incomplete'}
    $direct=Run 'TCC timestamp records' $xl @('run',$extensionSource,'--','--records')
    if($direct -cne $records){throw 'GCC/TCC timestamp bytes or values differ'}
    $directPath=Join-Path $work 'tcc.tsv'
    [IO.File]::WriteAllText($directPath,$direct+"`n",$utf8)
    $null=Run 'TCC independent timestamp oracle' $python @('release/extlibs/msgpack/test/check_timestamps.py',$directPath)
    $examples=@(@('msgpack_c.c','basic'),@('timestamp_frames_c.c','timestamp_frames'))
    $goldens=@{}
    foreach($pair in $examples){
        $source='release/extlibs/msgpack/examples/'+$pair[0]
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
    $null=Run 'C example write failure' $basic @((Join-Path $work 'missing/output.bin')) 30 1
    $language='release/extlibs/msgpack/examples/timestamp_frames.xl'
    $goldens[$language]=$goldens['release/extlibs/msgpack/examples/timestamp_frames_c.c']
    $output=Run 'timestamp XLang example' $xl @('run',$language)
    if($output -cne $goldens[$language]){throw 'Bilingual timestamp golden differs'}
    $paths='release/extlibs/msgpack/test/extensions.xl'
    $null=Run 'XLang extension boundaries' $xl @('run',$paths)
    $probe=Join-Path $work 'probe.c'
    $null=Run 'complete API cgen' $xl @('cgen',$language,'--emit',$probe)
    $text=[IO.File]::ReadAllText($probe)
    if($text -cnotmatch 'xl6_xrt_take_cstr\('){throw 'Owned HEX output is no longer adopted'}
    $decls=@($text -split "`r?`n"|Where-Object{$_ -match '^extern .*\bxmsgpack_[a-z_]+\('}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxmsgpack_[a-z_]+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    $binding=[IO.File]::ReadAllText((Join-Path $pkg 'src/msgpack.xl'))
    $expected=@([regex]::Matches($binding,'extern "([A-Za-z0-9_]+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    if($decls.Count -ne 8 -or @(Compare-Object $expected $actual).Count){throw 'Incomplete generated API prototypes'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"msgpack_native.c`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'eight actual prototypes' $gcc ($flags+@('-fsyntax-only',$abi))
    if(-not $SkipAot){
        $sources=@($extensionSource,$paths,$language)
        foreach($pair in $examples){$sources+='release/extlibs/msgpack/examples/'+$pair[0]}
        foreach($source in $sources){
            $binary=Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null=Run "$source build" $xl @('build',$source,'-o',$binary)
            $output=Run "$source AOT" $binary @()
            if($goldens.ContainsKey($source) -and $output -cne $goldens[$source]){throw 'AOT golden differs'}
            if($source -eq $extensionSource){
                if($output -cne $extensionExpected){throw 'AOT extension matrix incomplete'}
                $aotRecords=Run 'AOT timestamp records' $binary @('--records')
                if($aotRecords -cne $records){throw 'AOT timestamp bytes or values differ'}
                $path=Join-Path $work 'aot.tsv'
                [IO.File]::WriteAllText($path,$aotRecords+"`n",$utf8)
                $null=Run 'AOT independent timestamp oracle' $python @('release/extlibs/msgpack/test/check_timestamps.py',$path)
            }
        }
    }
    # 原语言字典正例依旧必过；C 生成的值或纯标量测试不能替代 M1 边界。
    foreach($source in @('test/cases/extlib_msgpack.xl','release/extlibs/msgpack/test/aot_core.xl',
        'release/extlibs/msgpack/test/boundaries.xl','release/extlibs/msgpack/test/allocations.xl',
        'release/extlibs/msgpack/examples/msgpack.xl','release/examples/extlibs/msgpack.xl')){
        $output=Run "$source complete JIT" $xl @('run',$source)
        if($source -eq 'release/extlibs/msgpack/examples/msgpack.xl' -and
            $output -cne $goldens['release/extlibs/msgpack/examples/msgpack_c.c']){throw 'Complete bilingual output differs'}
        if(-not $SkipAot){
            $binary=Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null=Run "$source complete build" $xl @('build',$source,'-o',$binary)
            $null=Run "$source complete AOT" $binary @()
        }
    }
    $outcome='passed'
} catch {$failure=$_.Exception.Message;throw} finally {
    Pop-Location
    $changed=@($hashes.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_]})
    $changed+=@(InputPaths|Where-Object{!$hashes.Contains($_)})
    if($changed.Count){$outcome='invalid-input-change'}
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())}|ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[msgpack] report: $work.json"
    $base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $resolved=[IO.Path]::GetFullPath($work)
    if(!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'MessagePack work path escaped build/test'}
    if($outcome -eq 'passed'){Remove-Item -LiteralPath $resolved -Recurse -Force}
    if($changed.Count){throw 'MessagePack inputs changed during gate'}
}
