param([string]$Root='', [string]$RuntimeDirectory='', [string]$WorkDirectory='', [switch]$SkipAot)
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path $Root).Path
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'release' }
$RuntimeDirectory = (Resolve-Path $RuntimeDirectory).Path
$workRoot = Join-Path $Root 'build/test'
$ownWork = !$WorkDirectory
$work = if ($WorkDirectory) {[IO.Path]::GetFullPath((Join-Path $Root $WorkDirectory))} else {
    Join-Path $workRoot ('graph_guard_' + [guid]::NewGuid().ToString('N'))
}
[void][IO.Directory]::CreateDirectory($work)
. (Join-Path $Root 'tools/extlib_smoke_process.ps1')
$xl = Join-Path $RuntimeDirectory 'xl.exe'
$gcc=if($env:CC){$env:CC}else{'gcc'}
$python=if($env:PYTHON){$env:PYTHON}else{'python'}
foreach($name in @('gcc','python')){
    $command=Get-Command (Get-Variable $name -ValueOnly) -CommandType Application -ErrorAction SilentlyContinue|Select-Object -First 1
    if($command){Set-Variable $name $command.Source}
}
$pkg = 'release/extlibs/graph'
$manifest = Get-Content (Join-Path $Root "$pkg/xlang.package.json") -Raw | ConvertFrom-Json
$sources = @($manifest.artifact.cSources | ForEach-Object {"$pkg/$_"})
$flags = @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',"-I$pkg/include")
$checks = [Collections.Generic.List[object]]::new()
$pending = [Collections.Generic.List[object]]::new()
$inputs = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
function InputPaths {
    @($xl,(Join-Path $RuntimeDirectory 'xl.dll'),(Join-Path $RuntimeDirectory 'xlc.dll'),$PSCommandPath)
    foreach($tool in @($gcc,$python)){if(Test-Path -LiteralPath $tool -PathType Leaf){$tool}}
    foreach ($relative in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','test/support/extlib_graph_runtime.c',
        'test/cases/extlib_graph.xl','test/support/extlib_runtime_host.c','release/examples/extlibs/graph.xl','release/examples/extlibs/graph_basic.xl',
        'release/examples/extlibs/graph_basic.expected.txt','tools/check_extlib_graph.sh','tools/extlib_smoke_process.ps1',
        'tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
        'test/fixtures/extlib_boundary_pending/string_byte_len_nul.xl',
        'test/fixtures/extlib_boundary_pending/graph_nul_name.xl',
        'test/fixtures/extlib_boundary_pending/string_join_symbol.xl')) { Join-Path $Root $relative }
    Get-ChildItem (Join-Path $Root $pkg) -Recurse -File | ForEach-Object FullName
    Get-ChildItem (Join-Path $RuntimeDirectory 'include') -Recurse -File | ForEach-Object FullName
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $inputs[$path]=(Get-FileHash -LiteralPath $path).Hash }
function Check([string]$name,[string]$program,[string[]]$arguments) {
    if($null -eq $arguments){$arguments=@()}
    $r = Invoke-ExtlibSmokeProcess $program $arguments $Root 120 @{XLANG_PACKAGE_PATH=(Join-Path $Root 'release/extlibs');PYTHONDONTWRITEBYTECODE='1'}
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    Write-Host "[graph] ${name}: $($r.status)"
    if ($r.status -ne 'pass') {throw "$name failed: $($r.stderr)`n$($r.stdout)"}
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
$outcome='failed'
$decls=@()
$hostExes=@{}
try {
    $null=Check 'compiler-version' $gcc @('--version')
    $null=Check 'python-version' $python @('-c','import sys; print(sys.version)')
    $modules=@($manifest.artifact.xrtModules|ForEach-Object{"-D$_"})
    $minimal=Join-Path $work 'minimal.exe'
    $null=Check 'manifest-only-build' $gcc ($flags+$modules+@('-DXRT_IMPLEMENTATION',"$pkg/test/manifest_probe.c",'-lws2_32','-ladvapi32','-lm','-o',$minimal))
    $null=Check 'manifest-only-run' $minimal @()
    $runtime=Join-Path $work 'runtime.o'
    $null=Check 'runtime-build' $gcc ($flags+@('-c','test/support/extlib_graph_runtime.c','-o',$runtime))
    $objects=@()
    foreach($source in $sources) {
        $object=Join-Path $work "native-$($objects.Count).o"
        $null=Check "native-$($objects.Count)" $gcc ($flags+@('-c',$source,'-o',$object))
        $objects += $object
    }
    $links=$objects+@($runtime,'-lws2_32','-ladvapi32','-lm')
    foreach($case in @('test_graph','test_graph_alloc','test_graph_threads')) {
        $exe=Join-Path $work "$case.exe"
        $null=Check "$case-build" $gcc ($flags+@('-DNDEBUG',"$pkg/test/$case.c")+$links+@('-o',$exe))
        $null=Check $case $exe @()
    }
    $weighted=Join-Path $work 'weighted.exe'
    $null=Check 'weighted-build' $gcc ($flags+@('-DNDEBUG',"$pkg/test/weighted_records_c.c",$runtime,'-lws2_32','-ladvapi32','-lm','-o',$weighted))
    if((Check 'weighted' $weighted @()) -cne 'graph weighted records: 3072 cases, PASS'){throw 'Weighted corpus incomplete'}
    $records=Check 'weighted-records' $weighted @('--records')
    $recordPath=Join-Path $work 'weighted.tsv'
    [IO.File]::WriteAllText($recordPath,$records+"`n",$utf8)
    $null=Check 'Bellman-Ford-oracle' $python @("$pkg/test/check_weighted_records.py",$recordPath)
    if((Check 'weighted-direct-C' $xl @('run',"$pkg/test/weighted_records_c.c",'--','--records')) -cne $records){throw 'GCC/TCC weighted records differ'}
    $golden=(Get-Content (Join-Path $Root 'release/examples/extlibs/graph_basic.expected.txt') -Raw).Replace("`r`n","`n").TrimEnd("`n")
    $language="$pkg/examples/from_json.xl"
    if((Check 'from-json-jit' $xl @('run',$language)) -cne $golden){throw 'JSON input golden differs'}
    $module=Join-Path $work 'from-json.c'
    $null=Check 'from-json-cgen' $xl @('cgen',$language,'--emit',$module)
    $decls=@(Get-Content $module | Where-Object {$_ -match '^extern .*\bxg_\w+\('}|Sort-Object -Unique)
    $names=@([regex]::Matches((Get-Content (Join-Path $Root "$pkg/src/graph.xl") -Raw),'extern "(xg_\w+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxg_\w+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    if($decls.Count -ne 9 -or @(Compare-Object $names $actual).Count){throw 'Incomplete initial graph ABI'}
    $abi=Join-Path $work 'initial-abi.c'
    [IO.File]::WriteAllText($abi,"#include `"graph.h`"`n"+($decls -join "`n"),$utf8)
    $null=Check 'initial-nine-prototypes' $gcc ($flags+@('-fsyntax-only',$abi))
    $example="$pkg/examples/basic.c"
    $exe=Join-Path $work 'c-example.exe'
    $null=Check 'c-example-build' $gcc ($flags+@('-DNDEBUG',$example,$runtime,'-lws2_32','-ladvapi32','-lm','-o',$exe))
    $cOutput=Check 'c-example' $exe @()
    $cJit=Check 'c-example-jit' $xl @('run',$example)
    if($cOutput -cne $golden -or $cJit -cne $golden){throw 'C graph golden differs'}
    if(-not $SkipAot){
        foreach($input in @($language,"$pkg/test/weighted_records_c.c",$example)){
            $target=Join-Path $work ([guid]::NewGuid().ToString('N')+'.exe')
            $null=Check "$input initial-build" $xl @('build',$input,'-o',$target)
            $output=Check "$input initial-aot" $target @()
            if($input -like '*/weighted_records_c.c'){
                $aotRecords=Check 'weighted-aot-records' $target @('--records')
                if($output -cne 'graph weighted records: 3072 cases, PASS' -or
                    $aotRecords -cne $records){throw 'AOT weighted corpus differs'}
                $aotPath=Join-Path $work 'weighted-aot.tsv'
                [IO.File]::WriteAllText($aotPath,$aotRecords+"`n",$utf8)
                $null=Check 'AOT-Bellman-Ford-oracle' $python @("$pkg/test/check_weighted_records.py",$aotPath)
            }elseif($output -cne $golden){throw 'Initial AOT golden differs'}
        }
    }
    foreach($dll in @('xl','xlc')){
        $library=Join-Path $RuntimeDirectory "$dll.dll"
        Copy-Item -LiteralPath $library -Destination (Join-Path $work "$dll.dll") -Force
        $hostExe=Join-Path $work "$dll-host.exe"
        $null=Check "$dll-host-build" $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror',
            '-I',(Join-Path $RuntimeDirectory 'include'),'test/support/extlib_runtime_host.c',$library,'-o',$hostExe)
        $hostExes[$dll]=$hostExe
        $output=Check "$dll-json-context-reuse" $hostExe @($Root,(Join-Path $Root $language))
        if($output -cne (($golden+"`n")*3).TrimEnd("`n")){throw 'Sequential DLL golden differs'}
    }
    # 原字面量输入仍为必过，正常 JSON 输入不能替代共享投影验收。
    foreach($case in @('test/cases/extlib_graph.xl',"$pkg/test/boundaries.xl", "$pkg/test/allocations.xl",
        'release/examples/extlibs/graph.xl','release/examples/extlibs/graph_basic.xl')) {
        $name=[IO.Path]::GetFileNameWithoutExtension($case)
        $jit=Check "$name-jit" $xl @('run',$case)
        if(-not $SkipAot) {
            $exe=Join-Path $work "$name.exe"
            $null=Check "$name-build" $xl @('build',$case,'-o',$exe)
            $aot=Check "$name-aot" $exe @()
            if($jit -cne $aot) {throw "$name JIT/AOT output differs"}
        }
        if($name -eq 'graph_basic') {$xlOutput=$jit}
        $module=Join-Path $work "$name.c"
        $null=Check "$name-cgen" $xl @('cgen',$case,'--emit',$module)
        $decls += @(Get-Content $module | Where-Object {$_ -match '^extern .*\bxg_\w+\('})
    }
    $decls=@($decls | Sort-Object -Unique)
    $binding=Get-Content (Join-Path $Root "$pkg/src/graph.xl") -Raw
    $expected=@([regex]::Matches($binding,'extern "(xg_\w+)"') | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxg_\w+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if(@(Compare-Object $expected $actual).Count -or $decls.Count -ne 9) {throw 'Incomplete graph ABI coverage'}
    [IO.File]::WriteAllText((Join-Path $work 'declarations.inc'),($decls -join "`n")+"`n",$utf8)
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"graph.h`"`n#include `"declarations.inc`"`n",$utf8)
    $null=Check 'generated-prototypes' $gcc ($flags+@('-fsyntax-only',$abi))
    $helper=Check 'c-example-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','graph','-Source',$example,'-RuntimeDirectory',$RuntimeDirectory)
    foreach($dll in @('xl','xlc')) {
        $null=Check "$dll-host-reuse" $hostExes[$dll] @($Root,(Join-Path $Root "$pkg/test/boundaries.xl"))
    }
    $golden=(Get-Content (Join-Path $Root 'release/examples/extlibs/graph_basic.expected.txt') -Raw).Replace("`r`n","`n").TrimEnd("`n")
    $outputs=@($xlOutput,$cOutput,$cJit,$helper)
    foreach($output in $outputs) {if($output -cne $golden) {throw 'Paired example output differs from golden'}}
    # Keep shared language failures separate from graph algorithm checks.
    foreach($name in @('string_byte_len_nul','graph_nul_name','string_join_symbol')) {
        $result=Invoke-ExtlibSmokeProcess $xl @('run',"test/fixtures/extlib_boundary_pending/$name.xl") $Root 45 @{XLANG_PACKAGE_PATH=(Join-Path $Root 'release/extlibs')}
        $pending.Add([ordered]@{name=$name;result=$result})
        Write-Host "[shared-boundary] ${name}: $($result.status)"
    }
    $outcome='passed-scoped'
} finally {
    $changed=@($inputs.Keys | Where-Object {!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $inputs[$_]})
    $changed+=@(InputPaths | Where-Object {!$inputs.Contains($_)})
    if($changed.Count) {$outcome='invalid-input-change'}
    $report=[ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;releaseReady=$false;runtimeDirectory=$RuntimeDirectory;
        aotSkipped=[bool]$SkipAot;
        scope='Graph C boundary/oracle/OOM/threads, Windows C and xlang JIT/AOT, xl/xlc DLL context reuse, 9 generated prototypes and paired examples; shared string gaps recorded separately';
        inputs=$inputs;changedInputs=$changed;generatedPrototypes=$decls.Count;checks=$checks.ToArray();sharedBoundaries=$pending.ToArray()}
    try {
        $reportPath=if($ownWork){$work+'.json'}else{Join-Path $work 'results.json'}
        [IO.File]::WriteAllText($reportPath,($report | ConvertTo-Json -Depth 8),$utf8)
        Write-Host "[graph] report: $reportPath"
    } finally {
        if ($ownWork -and $outcome -eq 'passed-scoped' -and (Test-Path -LiteralPath $work -PathType Container)) {
            $resolved = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $work).Path)
            $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\','/')
            if (!$resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
                throw "Graph work directory escaped build/test: $resolved"
            }
            Remove-Item -LiteralPath $resolved -Recurse -Force
        }
    }
}
if($outcome -ne 'passed-scoped') {throw "Graph gate failed; use -WorkDirectory to retain results.json"}
