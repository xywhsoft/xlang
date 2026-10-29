param([switch]$SkipAot)

$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$pkg=Join-Path $root 'release/extlibs/shellwords'
$workRoot=Join-Path $root 'build/test/shellwords_guard'
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
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/shellwords')
$links=@('-lWs2_32','-lAdvapi32','-lm')
$outcome='failed'
$failure=''
function Normalize([string]$value){$value.Replace("`r`n","`n").TrimEnd("`n")}
function InputPaths {
    Get-ChildItem -LiteralPath $pkg,(Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    foreach($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','test/cases/extlib_shellwords.xl','release/examples/extlibs/shellwords.xl')){Join-Path $root $path}
    foreach($path in @($gcc,$python)){if(Test-Path -LiteralPath $path -PathType Leaf){$path}}
}
foreach($path in @(InputPaths|Sort-Object -Unique)){$hashes[$path]=(Get-FileHash -LiteralPath $path).Hash}
function Run([string]$name,[string]$program,[string[]]$arguments,[int]$seconds=120){
    $result=Invoke-ExtlibSmokeProcess $program $arguments $root $seconds $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if($result.status -ne 'pass'){throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)"}
    Write-Host "[shellwords] ${name}: PASS"
    return Normalize $result.stdout
}
Push-Location $root
try {
    $null=Run 'compiler version' $gcc @('--version')
    $null=Run 'Python version' $python @('--version')
    $null=Run 'public header' $gcc ($flags+@('-x','c','-fsyntax-only','release/extlibs/shellwords/include/xrt_shellwords.h'))
    $manifest=Get-Content -LiteralPath (Join-Path $pkg 'xlang.package.json') -Raw|ConvertFrom-Json
    $modules=@($manifest.artifact.xrtModules|ForEach-Object{"-D$_"})
    $nativeSource='release/extlibs/shellwords/shellwords_native.c'
    $null=Run 'manifest syntax' $gcc ($flags+$modules+@('-fsyntax-only',$nativeSource))
    $runtime=Join-Path $work 'runtime.o'
    $native=Join-Path $work 'native.o'
    $null=Run 'selected XRT compile' $gcc ($flags+@('-c','release/extlibs/shellwords/test/xrt_runtime.c','-o',$runtime))
    $null=Run 'native compile' $gcc ($flags+@('-c',$nativeSource,'-o',$native))
    foreach($name in @('test_shellwords','quote_runs_c')){
        $source="release/extlibs/shellwords/test/$name.c"
        $binary=Join-Path $work ($name+'.exe')
        $objects=@($runtime)
        if($name -eq 'test_shellwords'){$objects+=$native}
        $null=Run "$name compile" $gcc ($flags+@($source)+$objects+$links+@('-o',$binary))
        $output=Run $name $binary @()
        if($name -eq 'quote_runs_c' -and $output -cne 'shellwords quote runs: 126 vectors, 8 OOM paths, PASS'){throw 'Incomplete quote vectors'}
    }
    $argv=Join-Path $work 'windows-argv.exe'
    $null=Run 'Windows argv compile' $gcc ($flags+@('-municode','release/extlibs/shellwords/test/windows_argv.c',$native,$runtime)+$links+@('-o',$argv))
    $null=Run 'Windows actual argv' $argv @()
    $records=Run 'GCC quote records' (Join-Path $work 'quote_runs_c.exe') @('--records')
    $recordPath=Join-Path $work 'gcc.tsv'
    [IO.File]::WriteAllText($recordPath,$records+"`n",$utf8)
    $null=Run 'GCC independent quote oracle' $python @('release/extlibs/shellwords/test/check_quote_records.py',$recordPath)
    $quoteSource='release/extlibs/shellwords/test/quote_runs_c.c'
    $output=Run 'quote vectors direct C' $xl @('run',$quoteSource)
    if($output -cne 'shellwords quote runs: 126 vectors, 8 OOM paths, PASS'){throw 'TCC quote vectors incomplete'}
    $direct=Run 'TCC quote records' $xl @('run',$quoteSource,'--','--records')
    if($direct -cne $records){throw 'GCC/TCC quoted bytes differ'}
    $directPath=Join-Path $work 'tcc.tsv'
    [IO.File]::WriteAllText($directPath,$direct+"`n",$utf8)
    $null=Run 'TCC independent quote oracle' $python @('release/extlibs/shellwords/test/check_quote_records.py',$directPath)
    $examples=@(@('release/extlibs/shellwords/examples/basic.c','release/examples/extlibs/shellwords.xl','basic'),
        @('release/extlibs/shellwords/examples/command_plan_c.c','release/extlibs/shellwords/examples/command_plan.xl','command_plan'))
    $goldens=@{}
    foreach($pair in $examples){
        $expected=Normalize ([IO.File]::ReadAllText((Join-Path $pkg ("examples/$($pair[2]).expected.txt"))))
        $goldens[$pair[0]]=$expected; $goldens[$pair[1]]=$expected
        $binary=Join-Path $work ($pair[2]+'.exe')
        $null=Run "$($pair[2]) C compile" $gcc ($flags+@($pair[0],$runtime)+$links+@('-o',$binary))
        $c=Run "$($pair[2]) GCC" $binary @()
        $direct=Run "$($pair[2]) direct C" $xl @('run',$pair[0])
        $language=Run "$($pair[2]) XLang" $xl @('run',$pair[1])
        if($c -cne $expected -or $direct -cne $expected -or $language -cne $expected){throw 'Bilingual golden output differs'}
    }
    $texts='release/extlibs/shellwords/test/text_paths.xl'
    $null=Run 'text-only boundary/OOM JIT' $xl @('run',$texts)
    $probe=Join-Path $work 'probe.c'
    $null=Run 'complete API cgen' $xl @('cgen','release/extlibs/shellwords/examples/command_plan.xl','--emit',$probe)
    $text=[IO.File]::ReadAllText($probe)
    if($text -cnotmatch 'xl6_xrt_take_cstr\('){throw 'Owned C strings no longer adopted'}
    $decls=@($text -split "`r?`n"|Where-Object{$_ -match '^extern .*\b(xsw_[a-z_]+|xrtShellwordsSplit)\('}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\b(xsw_[a-z_]+|xrtShellwordsSplit)(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    $binding=[IO.File]::ReadAllText((Join-Path $pkg 'src/shellwords.xl'))
    $expected=@([regex]::Matches($binding,'extern "([A-Za-z0-9_]+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    if($decls.Count -ne 4 -or @(Compare-Object $expected $actual).Count){throw 'Incomplete generated API prototypes'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"shellwords_native.c`"`n"+($decls -join "`n"),$utf8)
    $null=Run 'four actual prototypes' $gcc ($flags+@('-fsyntax-only',$abi))
    if(-not $SkipAot){
        $sources=@($quoteSource,$texts)
        foreach($pair in $examples){$sources+=@($pair[0],$pair[1])}
        foreach($source in $sources){
            $binary=Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null=Run "$source build" $xl @('build',$source,'-o',$binary)
            $output=Run "$source AOT" $binary @()
            if($goldens.ContainsKey($source) -and $output -cne $goldens[$source]){throw 'AOT golden differs'}
            if($source -eq $quoteSource){
                if($output -cne 'shellwords quote runs: 126 vectors, 8 OOM paths, PASS'){throw 'AOT quote vectors incomplete'}
                $aotRecords=Run 'AOT quote records' $binary @('--records')
                if($aotRecords -cne $records){throw 'AOT quoted bytes differ'}
                $path=Join-Path $work 'aot.tsv'
                [IO.File]::WriteAllText($path,$aotRecords+"`n",$utf8)
                $null=Run 'AOT independent quote oracle' $python @('release/extlibs/shellwords/test/check_quote_records.py',$path)
            }
        }
    }
    # 原语言数组输入仍是必过正例，不能用 split 的原生结果替代它。
    foreach($source in @('test/cases/extlib_shellwords.xl','release/extlibs/shellwords/test/boundaries.xl','release/extlibs/shellwords/test/allocations.xl')){
        $null=Run "$source complete JIT" $xl @('run',$source)
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
    Write-Host "[shellwords] report: $work.json"
    $base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $resolved=[IO.Path]::GetFullPath($work)
    if(!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'Shellwords work path escaped build/test'}
    if($outcome -eq 'passed'){Remove-Item -LiteralPath $resolved -Recurse -Force}
    if($changed.Count){throw 'Shellwords inputs changed during gate'}
}
