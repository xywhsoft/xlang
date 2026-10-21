param([string]$Root='', [string]$WorkDirectory='', [string]$PublicSourceDirectory='', [switch]$SkipAot)
$ErrorActionPreference='Stop'
if (!$Root) { $Root=Split-Path -Parent $PSScriptRoot }
$Root=(Resolve-Path -LiteralPath $Root).Path
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$gcc=(Get-Command gcc -CommandType Application -ErrorAction Stop|Select-Object -First 1).Source
$xl=Join-Path $Root 'release/xl.exe'
$python=(Get-Command python -CommandType Application -ErrorAction Stop|Select-Object -First 1).Source
$compiler=$null
$artifact=$null
if($PublicSourceDirectory){
    $compilerDir=(Resolve-Path -LiteralPath (Join-Path $Root $PublicSourceDirectory)).Path
    $compiler=Get-Content (Join-Path $compilerDir 'build.json') -Raw|ConvertFrom-Json
    if($compiler.RuntimeOnly -or (Get-FileHash -LiteralPath $compiler.Driver).Hash -ne $compiler.DriverSha256){throw 'Invalid CSV source compiler'}
    foreach($input in @($compiler.Inputs)+@($compiler.Headers)+@($compiler.Objects)){
        if((Get-FileHash -LiteralPath $input.Path).Hash -ne $input.Sha256){throw "Stale compiler input: $($input.Path)"}
    }
    # 使用隔离源码编译器验证修复，不替换正式 CLI/DLL，也不混用另一份 AOT 运行库。
    $artifactPath=Join-Path $compilerDir 'artifact_sources.json'
    if(!(Test-Path -LiteralPath $artifactPath)){
        Push-Location $Root
        try { & tools/stage_xlir_artifact_runtime_sources.ps1 -Directory $compilerDir }
        finally { Pop-Location }
    }
    $artifact=Get-Content $artifactPath -Raw|ConvertFrom-Json
    if($artifact.DriverSha256 -ne $compiler.DriverSha256){throw 'CSV artifact sources belong to another compiler'}
    foreach($file in $artifact.Files){
        if((Get-FileHash -LiteralPath $file.Source).Hash -ne $file.Sha256 -or
           (Get-FileHash -LiteralPath $file.Path).Hash -ne $file.Sha256){throw "Stale artifact source: $($file.Path)"}
    }
}
$work=if($WorkDirectory){[IO.Path]::GetFullPath((Join-Path $Root $WorkDirectory))}else{Join-Path $Root 'build/test/csv_guard'}
[void][IO.Directory]::CreateDirectory($work)
$utf8=[Text.UTF8Encoding]::new($false)
$checks=[Collections.Generic.List[object]]::new()
$environment=@{XLANG_PACKAGE_PATH=(Join-Path $Root 'release/extlibs')}
$manifest=Get-Content (Join-Path $Root 'release/extlibs/csv/xlang.package.json') -Raw|ConvertFrom-Json
$sources=@($manifest.artifact.cSources|ForEach-Object{"release/extlibs/csv/$_"})
function InputPaths {
    Get-ChildItem (Join-Path $Root 'release/extlibs/csv') -Recurse -File|ForEach-Object FullName
    foreach($p in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','test/support/extlib_csv_runtime.c',
        'tools/check_extlib_csv.ps1','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
        'tools/run_extlib_c.ps1','tools/check_extlib_csv.sh','tools/check_extlib_csv_interop.py',
        'test/cases/extlib_csv.xl','release/examples/extlibs/csv.xl',
        'release/examples/extlibs/csv_basic.xl','release/examples/extlibs/csv_files.xl',
        'release/examples/extlibs/csv_basic.expected.txt','release/examples/extlibs/csv_files.expected.txt',
        'release/xl.exe','release/xl.dll','release/xlc.dll','release/include/xrt_decl.h',
        'release/include/xruntime_decl.h','release/include/xl_extlib_abi.h')){Join-Path $Root $p}
    $gcc
    $python
    if($compiler){
        $compiler.Driver
        Join-Path $compilerDir 'build.json'
        $compiler.Inputs.Path
        $compiler.Headers.Path
        $compiler.Objects.Path
        $artifactPath
        $artifact.Files.Source
        $artifact.Files.Path
    }
}
$inputs=[ordered]@{}
foreach($p in @(InputPaths|Sort-Object -Unique)){$inputs[$p]=(Get-FileHash -LiteralPath $p).Hash}
function Check([string]$Name,[string]$Program,[string[]]$Arguments,[int]$Seconds=120,[string]$WorkingDirectory=$Root) {
    $r=Invoke-ExtlibSmokeProcess $Program $Arguments $WorkingDirectory $Seconds $environment
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;result=$r})
    if($r.status -ne 'pass'){throw "$Name failed: $($r.stderr)`n$($r.stdout)"}
    Write-Host "[csv] ${Name}: PASS $($r.stdout.Trim())"
    $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
function Language([string]$Name,[string]$Case,[string]$Action,[string]$Prefix,[string]$WorkingDirectory=$Root){
    if($compiler){
        $mode=@{run='run';build='exe';cgen='emit'}[$Action]
        $output=Check $Name $compiler.Driver @($Case,$mode,$Prefix) 120 $WorkingDirectory
        # 驱动的完成提示不属于脚本标准输出，JIT/AOT 仍比较脚本的真实输出。
        return $output -replace '(?:\n)?public source (?:run|exe|emit) complete$',''
    }
    $arguments=@($Action,$Case)
    if($Action -eq 'build'){$arguments+=@('-o',"$Prefix.exe")}
    if($Action -eq 'cgen'){$arguments+=@('--emit',"$Prefix.c")}
    Check $Name $xl $arguments 120 $WorkingDirectory
}
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt')
$outcome='failed';$failure=''
try {
    $null=Check 'compiler-version' $gcc @('--version')
    $access=Invoke-ExtlibSmokeProcess $xl @('check','release/extlibs/csv/test/private_native.xl') $Root 120 $environment
    $accessText="$($access.stderr)`n$($access.stdout)"
    if($access.status -ne 'fail' -or !$accessText.Contains('fieldNative') -or !$accessText.Contains('access')){
        throw "CSV Native member access check failed: $accessText"
    }
    $checks.Add([ordered]@{name='private-native-access';program=$xl;
        arguments=@('check','release/extlibs/csv/test/private_native.xl');
        result=[ordered]@{status='pass';expected='access-rejected';observed=$access}})
    Write-Host '[csv] private-native-access: PASS (rejected as expected)'
    $runtime=Join-Path $work 'runtime.o'
    $null=Check 'runtime' $gcc ($flags+@('-DXRT_MODULE_THREAD','-DXRT_MODULE_ATOMIC','-c','test/support/extlib_csv_runtime.c','-o',$runtime))
    $objects=@($runtime);$index=0
    foreach($source in $sources){
        $object=Join-Path $work "source-$index.o"
        $null=Check "source-$index" $gcc ($flags+@('-c',$source,'-o',$object))
        $objects+=$object;++$index
    }
    foreach($name in @('test_csv','test_csv_boundary','test_csv_stream','test_csv_alloc','test_csv_files','test_csv_contracts','test_csv_threads')) {
        $exe=Join-Path $work "$name.exe"
        $null=Check "$name-build" $gcc ($flags+@("release/extlibs/csv/test/$name.c")+$objects+@('-lws2_32','-ladvapi32','-lm','-o',$exe))
        $null=Check $name $exe @()
    }
    $interop=Join-Path $work 'interop.exe'
    $null=Check 'interop-build' $gcc ($flags+@('release/extlibs/csv/test/csv_interop.c')+$objects+@('-lws2_32','-ladvapi32','-lm','-o',$interop))
    $null=Check 'python-csv-interop' $python @('tools/check_extlib_csv_interop.py',$interop,(Join-Path $work 'interop-data')) 180
    $decls=@()
    foreach($case in @('test/cases/extlib_csv.xl','release/examples/extlibs/csv.xl')+@($manifest.coverage.tests|Where-Object { $_ }|ForEach-Object{"release/extlibs/csv/$_"})) {
        $name=[IO.Path]::GetFileNameWithoutExtension($case)
        $prefix=Join-Path $work $name
        $jit=Language "$name-jit" $case 'run' $prefix
        if(!$SkipAot){
            $null=Language "$name-build" $case 'build' $prefix
            $aot=Check "$name-aot" "$prefix.exe" @()
            if($jit -cne $aot){throw "$name JIT/AOT output differs"}
        }
        $cgen=Join-Path $work "$name.c"
        $null=Language "$name-cgen" $case 'cgen' $prefix
        $decls+=@(Get-Content $cgen|Where-Object{$_ -match '^extern .*\bxcsv_[a-z_]+\('})
    }
    $decls=@($decls|Sort-Object -Unique)
    $binding=Get-Content (Join-Path $Root 'release/extlibs/csv/src/csv.xl') -Raw
    $expected=@([regex]::Matches($binding,'extern "(xcsv_[a-z_]+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxcsv_[a-z_]+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    if(@(Compare-Object $expected $actual).Count -ne 0 -or $expected.Count -ne $decls.Count){throw 'CSV generated ABI coverage is incomplete'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include <stdint.h>`n#include `"csv_native.h`"`n"+($decls -join "`n"),$utf8)
    $null=Check "generated-$($expected.Count)-prototypes" $gcc ($flags+@('-Irelease/extlibs/csv/include','-fsyntax-only',$abi))
    foreach($name in @('basic','files')) {
        $prefix=Join-Path $work "example-$name"
        $expected=[IO.File]::ReadAllText((Join-Path $Root "release/examples/extlibs/csv_$name.expected.txt")).Replace("`r`n","`n").TrimEnd("`n")
        $case="release/examples/extlibs/csv_$name.xl"
        $jit=Language "example-$name-jit" $case 'run' $prefix
        $aot=$null
        if(!$SkipAot){
            $null=Language "example-$name-build" $case 'build' $prefix
            $aot=Check "example-$name-aot" "$prefix.exe" @()
        }
        $c="release/extlibs/csv/examples/$name.c"
        $null=Check "example-$name-c-build" $gcc ($flags+@($c,$runtime,'-lws2_32','-ladvapi32','-lm','-o',"$prefix-c.exe"))
        $native=Check "example-$name-c" "$prefix-c.exe" @()
        $cjit=Check "example-$name-c-jit" $xl @('run',$c)
        $caot=$null
        if(!$SkipAot){
            $null=Check "example-$name-c-aot-build" $xl @('build',$c,'-o',"$prefix-tcc.exe")
            $caot=Check "example-$name-c-aot" "$prefix-tcc.exe" @()
        }
        $helper=Check "example-$name-package-runner" (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','csv','-Source',$c)
        $outputsToCheck=@($jit,$native,$cjit,$helper)
        if(!$SkipAot){$outputsToCheck+=@($aot,$caot)}
        foreach($output in $outputsToCheck) {
            if($output -cne $expected){throw "CSV $name bilingual golden output mismatch"}
        }
    }
    # 直接执行维护文档中的代码块；文件范例隔离工作目录，不能覆盖用户的 report.csv。
    $readmePath=[IO.Path]::Combine($Root,'release','extlibs','csv','README.md')
    $readmeText=[IO.File]::ReadAllText($readmePath)
    if(!$readmeText.Contains('```xlang')){throw 'CSV README has no xlang example'}
    $site=[IO.File]::ReadAllText([IO.Path]::Combine($Root,'release','extlibs','csv','docs','site.html'))
    $readmeBlocks=@([regex]::Matches($readmeText,'(?ms)^```xlang\r?\n(.*?)^```')|ForEach-Object{$_.Groups[1].Value})
    $siteBlocks=@([regex]::Matches($site,'(?s)<code class="language-xlang">(.*?)</code>')|ForEach-Object{[Net.WebUtility]::HtmlDecode($_.Groups[1].Value)})
    $blocks=@($readmeBlocks)+@($siteBlocks)
    $outputs=@('Alice|上海','甲:2','Alice|上海','3')
    if($blocks.Count -ne $outputs.Count){throw "CSV documentation code block count changed: README=$($readmeBlocks.Count) site=$($siteBlocks.Count), $($outputs.Count) expected"}
    for($index=0;$index -lt $blocks.Count;++$index){
        $docwork=[IO.Path]::Combine($work,"doc-$index")
        [void][IO.Directory]::CreateDirectory($docwork)
        $source=[IO.Path]::Combine($docwork,'main.xl')
        [IO.File]::WriteAllText($source,$blocks[$index],$utf8)
        # 源码驱动依赖仓库工作目录；生成产物再在独立目录运行，文档原文无需路径改写。
        $prefix=[IO.Path]::Combine($docwork,'main')
        if($SkipAot){
            $output=Language "documentation-$index-jit" $source 'run' $prefix $docwork
        }else{
            $null=Language "documentation-$index-build" $source 'build' $prefix
            $output=Check "documentation-$index" "$prefix.exe" @() 120 $docwork
        }
        if($output -cne $outputs[$index]){throw "CSV documentation-$index output mismatch"}
    }
    $outcome='passed'
} catch { $failure=$_.Exception.Message; throw } finally {
    $changed=@($inputs.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $inputs[$_]})
    $changed+=@(InputPaths|Where-Object{!$inputs.Contains($_)})
    if($changed.Count){$outcome='changed-inputs'}
    $record=[ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        scope=if($SkipAot){'Independent Windows C, allocation/IO/file contracts, XL JIT, bilingual output and generated native prototypes; AOT skipped'}else{'Independent Windows C, allocation/IO/file contracts, XL JIT/AOT and generated native prototypes; not complete package admission'};
        releaseReady=$false;compiler=if($compiler){$compiler.Driver}else{$xl};
        inputs=$inputs;changedInputs=$changed;checks=$checks.ToArray()}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($record|ConvertTo-Json -Depth 10),$utf8)
}
if($outcome -ne 'passed'){throw "CSV gate outcome: $outcome"}
