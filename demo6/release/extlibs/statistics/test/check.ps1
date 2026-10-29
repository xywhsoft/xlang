param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = Join-Path $root 'build/test/statistics_guard'
$work = Join-Path $workRoot ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$compiler = if ($env:CC) { $env:CC } else { 'gcc' }
$gccCommand = Get-Command $compiler -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$gcc = if ($gccCommand) { $gccCommand.Source } else { $compiler }
$interpreter = if ($env:PYTHON) { $env:PYTHON } else { 'python' }
$pythonCommand = Get-Command $interpreter -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$python = if ($pythonCommand) { $pythonCommand.Source } else { $interpreter }
$xl = Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$utf8 = [Text.UTF8Encoding]::new($false)
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$environment = @{XLANG_PACKAGE_PATH=Join-Path $root 'release/extlibs'; PYTHONDONTWRITEBYTECODE='1'}
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/statistics')
$links = @('-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
$outcome = 'failed'
$failure = ''
function Normalize([string]$value) { $value.Replace("`r`n","`n").TrimEnd("`n") }
function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/statistics') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'test/cases/extlib_statistics.xl','release/examples/extlibs/statistics.xl',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $path }
    if ($gccCommand) { $gcc }
    if ($pythonCommand) { $python }
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path]=(Get-FileHash -LiteralPath $path).Hash }
function Run([string]$name,[string]$program,[string[]]$arguments) {
    $result=Invoke-ExtlibSmokeProcess $program $arguments $root 180 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[statistics] ${name}: PASS"
    return Normalize $result.stdout
}

Push-Location $root
try {
    $null=Run 'compiler version' $gcc @('--version')
    $null=Run 'Python version' $python @('--version')
    $null=Run 'public header' $gcc ($flags+@('-x','c','-fsyntax-only','release/extlibs/statistics/statistics_native.h'))
    $manifest=Get-Content -LiteralPath release/extlibs/statistics/xlang.package.json -Raw|ConvertFrom-Json
    $modules=@($manifest.artifact.xrtModules|ForEach-Object{"-D$_"})
    $null=Run 'manifest syntax' $gcc ($flags+$modules+@('-fsyntax-only','release/extlibs/statistics/statistics_native.c'))
    $null=Run 'C ABI contract' $gcc ($flags+@('-fsyntax-only','release/extlibs/statistics/test/abi_contract.c'))
    $runtime=Join-Path $work 'runtime.o'
    $native=Join-Path $work 'native.o'
    $null=Run 'selected XRT compile' $gcc ($flags+@('-c','release/extlibs/statistics/test/xrt_runtime.c','-o',$runtime))
    $null=Run 'native compile' $gcc ($flags+@('-c','release/extlibs/statistics/statistics_native.c','-o',$native))
    foreach ($name in @('test_statistics','size_guard','translation_c')) {
        $binary=Join-Path $work ($name+'.exe')
        $source="release/extlibs/statistics/test/$name.c"
        $objects=@($runtime)
        if ($name -ne 'size_guard') { $objects+=$native }
        $null=Run "$name compile" $gcc ($flags+@('-DNDEBUG',$source)+$objects+$links+@('-o',$binary))
        $output=Run $name $binary @()
        if ($name -eq 'translation_c' -and $output -cne 'statistics translation: 160 checks, 0 failures') { throw 'Incomplete translation checks' }
    }
    $records=Run 'Fraction input/output records' (Join-Path $work 'translation_c.exe') @('--records')
    $path=Join-Path $work 'gcc.tsv'
    [IO.File]::WriteAllText($path,$records+"`n",$utf8)
    $null=Run 'Fraction/Decimal oracle' $python @('release/extlibs/statistics/test/check_translation.py',$path)
    $output=Run 'translation direct C' $xl @('run','release/extlibs/statistics/test/translation_c.c')
    if ($output -cne 'statistics translation: 160 checks, 0 failures') { throw 'TCC translation differs' }
    $examples=@(@('statistics_c','basic'),@('trend_c','trend'),@('distribution_c','distribution'),@('measurement_offsets_c','measurement_offsets'))
    $goldens=@{}
    foreach ($pair in $examples) {
        $expected=Normalize ([IO.File]::ReadAllText((Join-Path $PSScriptRoot "../examples/$($pair[1]).expected.txt")))
        $goldens[$pair[0]]=$expected
        $source="release/extlibs/statistics/examples/$($pair[0]).c"
        $binary=Join-Path $work ($pair[0]+'.exe')
        $objects=@($runtime)
        if ($pair[0] -eq 'measurement_offsets_c') { $objects+=$native }
        $null=Run "$($pair[0]) compile" $gcc ($flags+@('-DNDEBUG',$source)+$objects+$links+@('-o',$binary))
        $c=Run "$($pair[0]) GCC" $binary @()
        $direct=Run "$($pair[0]) direct" $xl @('run',$source)
        if ($c -cne $expected -or $direct -cne $expected) { throw 'C example golden differs' }
    }
    $probe=Join-Path $work 'probe.c'
    $null=Run 'complete language API cgen' $xl @('cgen','release/extlibs/statistics/examples/measurement_offsets.xl','--emit',$probe)
    $text=[IO.File]::ReadAllText($probe)
    $decls=@($text -split "`r?`n"|Where-Object{$_ -match '^extern .*\bxstat_[a-z_]+\('}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxstat_[a-z_]+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    $binding=[IO.File]::ReadAllText((Join-Path $PSScriptRoot '../src/statistics.xl'))
    $expected=@([regex]::Matches($binding,'extern "(xstat_[a-z_]+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    if ($decls.Count -ne 23 -or $expected.Count -ne 23 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete generated API prototypes' }
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"statistics_native.h`"`n"+($decls -join "`n"),$utf8)
    $null=Run '23 actual prototypes' $gcc ($flags+@('-fsyntax-only',$abi))
    if (-not $SkipAot) {
        $sources=@('release/extlibs/statistics/test/translation_c.c','release/extlibs/statistics/test/size_guard.c')
        $sources+=@($examples|ForEach-Object{"release/extlibs/statistics/examples/$($_[0]).c"})
        foreach ($source in $sources) {
            $binary=Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null=Run "$source build" $xl @('build',$source,'-o',$binary)
            $output=Run "$source AOT" $binary @()
            $name=[IO.Path]::GetFileNameWithoutExtension($source)
            if ($goldens.ContainsKey($name) -and $output -cne $goldens[$name]) { throw 'C AOT example golden differs' }
            if ($name -eq 'translation_c') {
                if ($output -cne 'statistics translation: 160 checks, 0 failures') { throw 'AOT translation differs' }
                $records=Run 'TCC AOT Fraction records' $binary @('--records')
                $path=Join-Path $work 'tcc.tsv'
                [IO.File]::WriteAllText($path,$records+"`n",$utf8)
                $null=Run 'TCC AOT Fraction oracle' $python @('release/extlibs/statistics/test/check_translation.py',$path)
            }
        }
    }
    # 真正的语言数组仍是必过正例；M1不得以C工厂或忽略退出码绕过。
    $full=@('release/extlibs/statistics/examples/measurement_offsets.xl','test/cases/extlib_statistics.xl',
        'release/extlibs/statistics/test/boundaries.xl','release/extlibs/statistics/test/allocations.xl',
        'release/extlibs/statistics/examples/basic.xl','release/extlibs/statistics/examples/trend.xl',
        'release/extlibs/statistics/examples/distribution.xl','release/examples/extlibs/statistics.xl')
    foreach ($source in $full) {
        $output=Run "$source complete JIT" $xl @('run',$source)
        foreach ($pair in $examples) { if ($source -eq "release/extlibs/statistics/examples/$($pair[1]).xl" -and $output -cne $goldens[$pair[0]]) { throw 'Bilingual example differs' } }
        if (-not $SkipAot) {
            $binary=Join-Path $work (([guid]::NewGuid().ToString('N'))+'.exe')
            $null=Run "$source complete build" $xl @('build',$source,'-o',$binary)
            $output=Run "$source complete AOT" $binary @()
            foreach ($pair in $examples) { if ($source -eq "release/extlibs/statistics/examples/$($pair[1]).xl" -and $output -cne $goldens[$pair[0]]) { throw 'Bilingual AOT example differs' } }
        }
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
    Write-Host "[statistics] report: $work.json"
    $base=[IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $resolved=[IO.Path]::GetFullPath($work)
    if (!$resolved.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) { throw 'Statistics work path escaped build/test' }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $resolved -Recurse -Force }
    if ($changed.Count) { throw 'Statistics inputs changed during gate' }
}
