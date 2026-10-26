param([string]$Root='', [string]$WorkDirectory='', [string]$PublicSourceDirectory='', [switch]$SkipAot)
$ErrorActionPreference='Stop'
if(!$Root){$Root=Split-Path -Parent $PSScriptRoot}
$Root=(Resolve-Path $Root).Path
$pkg='release/extlibs/color'
$xl=Join-Path $Root 'release/xl.exe'
$gcc=(Get-Command gcc -CommandType Application|Select-Object -First 1).Source
$python=(Get-Command python -CommandType Application|Select-Object -First 1).Source
$work=if($WorkDirectory){[IO.Path]::GetFullPath((Join-Path $Root $WorkDirectory))}else{Join-Path $Root ('build/test/color_guard/run_'+[guid]::NewGuid().ToString('N'))}
[void][IO.Directory]::CreateDirectory($work)
$utf8=[Text.UTF8Encoding]::new($false)
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$compiler=$null; $artifact=$null
if($PublicSourceDirectory){
    $compilerDir=(Resolve-Path (Join-Path $Root $PublicSourceDirectory)).Path
    $compiler=Get-Content (Join-Path $compilerDir 'build.json') -Raw|ConvertFrom-Json
    $artifact=Get-Content (Join-Path $compilerDir 'artifact_sources.json') -Raw|ConvertFrom-Json
    if($compiler.RuntimeOnly -or (Get-FileHash $compiler.Driver).Hash -ne $compiler.DriverSha256 -or $artifact.DriverSha256 -ne $compiler.DriverSha256){throw 'Invalid color source compiler'}
    foreach($file in @($compiler.Inputs)+@($compiler.Headers)+@($compiler.Objects)){
        if((Get-FileHash $file.Path).Hash -ne $file.Sha256){throw "Stale compiler input: $($file.Path)"}
    }
    foreach($file in $artifact.Files){
        if((Get-FileHash $file.Source).Hash -ne $file.Sha256 -or (Get-FileHash $file.Path).Hash -ne $file.Sha256){throw "Stale artifact input: $($file.Path)"}
    }
}
$manifest=Get-Content (Join-Path $Root "$pkg/xlang.package.json") -Raw|ConvertFrom-Json
$sources=@($manifest.artifact.cSources|ForEach-Object{"$pkg/$_"})
$cases=@('test/cases/extlib_color.xl','release/examples/extlibs/color.xl','release/examples/extlibs/color_basic.xl')+@($manifest.coverage.tests|ForEach-Object{"$pkg/$_"})
$environment=@{XLANG_PACKAGE_PATH=(Join-Path $Root 'release/extlibs')}
$inputs=[ordered]@{}; $checks=[Collections.Generic.List[object]]::new()
function InputPaths {
    $gcc; $python; $PSCommandPath
    Get-ChildItem (Join-Path $Root $pkg) -Recurse -File|ForEach-Object FullName
    foreach($file in $cases+@('lib/xrt/xrt.h','lib/xruntime/xruntime.h','test/support/extlib_color_runtime.c','test/support/extlib_runtime_host.c',
        'release/examples/extlibs/color_basic.expected.txt','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/check_extlib_color.sh','tools/check_extlib_color_oracle.py','tools/run_extlib_c.ps1','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')){Join-Path $Root $file}
    Get-ChildItem (Join-Path $Root 'release/include') -Recurse -File|ForEach-Object FullName
    if($compiler){
        $compiler.Driver; Join-Path $compilerDir 'build.json'; Join-Path $compilerDir 'artifact_sources.json'
        $compiler.Inputs.Path; $compiler.Headers.Path; $compiler.Objects.Path; $artifact.Files.Source; $artifact.Files.Path
    }
}
foreach($path in @(InputPaths|Sort-Object -Unique)){$inputs[$path]=(Get-FileHash $path).Hash}
function Check([string]$Name,[string]$Program,[string[]]$Arguments,[string]$Directory=$Root){
    $r=Invoke-ExtlibSmokeProcess $Program $Arguments $Directory 120 $environment
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;directory=$Directory;result=$r})
    Write-Host "[color] ${Name}: $($r.status)"
    if($r.status -ne 'pass'){throw "$Name failed: $($r.stderr)`n$($r.stdout)"}
    $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
function Language([string]$Name,[string]$Case,[string]$Action,[string]$Prefix){
    if($compiler){
        $mode=@{run='run';build='exe';cgen='emit'}[$Action]
        $output=Check $Name $compiler.Driver @($Case,$mode,$Prefix)
        return $output -replace '(?:\n)?public source (?:run|exe|emit) complete$',''
    }
    $arguments=@($Action,$Case)
    if($Action -eq 'build'){$arguments+=@('-o',"$Prefix.exe")}
    if($Action -eq 'cgen'){$arguments+=@('--emit',"$Prefix.c")}
    Check $Name $xl $arguments
}
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',"-I$pkg/include")
$outcome='failed'; $failure=''; $formalAllocation=$null; $decls=@()
try {
    $null=Check 'gcc-version' $gcc @('--version')
    $runtime=Join-Path $work 'runtime.o'
    $null=Check 'runtime' $gcc ($flags+@('-c','test/support/extlib_color_runtime.c','-o',$runtime))
    $objects=@($runtime)
    foreach($source in $sources){
        $object=Join-Path $work "source-$($objects.Count).o"
        $null=Check "source-$($objects.Count)" $gcc ($flags+@('-c',$source,'-o',$object))
        $objects+=$object
    }
    $links=$objects+@('-lws2_32','-ladvapi32','-lm')
    foreach($name in @('test_color','test_threads','oracle')){
        $exe=Join-Path $work "$name.exe"
        $testFlags=if($name -eq 'oracle'){$flags}else{$flags+@('-DNDEBUG')}
        $null=Check "$name-build" $gcc ($testFlags+@("$pkg/test/$name.c")+$links+@('-o',$exe))
        if($name -ne 'oracle'){$null=Check $name $exe @()}
    }
    $null=Check 'independent-oracle' $python @('tools/check_extlib_color_oracle.py',(Join-Path $work 'oracle.exe'),(Join-Path $work 'oracle.json'))
    foreach($case in $cases){
        $name=[IO.Path]::GetFileNameWithoutExtension($case)
        $prefix=Join-Path $work $name
        $jit=Language "$name-jit" $case 'run' $prefix
        if(-not $SkipAot){
            $null=Language "$name-build" $case 'build' $prefix
            $aot=Check "$name-aot" "$prefix.exe" @()
            if($jit -cne $aot){throw "$name JIT/AOT output differs"}
        }
        $null=Language "$name-cgen" $case 'cgen' $prefix
        $decls+=@(Get-Content "$prefix.c"|Where-Object{$_ -match '^extern .*\bxcolor_\w+\('})
        if($name -eq 'color_basic'){$xlOutput=$jit}
        if($compiler -and $name -ne 'allocations'){
            $formal=Check "$name-formal-jit" $xl @('run',$case)
            if($formal -cne $jit){throw "$name formal/source output differs"}
        }
    }
    # Run the formal CLI separately only when a public-source driver was used above.
    if($compiler){
        $before=$checks.Count
        Check 'formal-allocation' $xl @('run',"$pkg/test/allocations.xl") | Out-Null
        if($checks.Count -ne $before+1){throw 'Formal allocation result was not recorded'}
        $formalAllocation=$checks[$checks.Count-1].result
    } else {
        foreach($check in $checks){
            if($check.name -eq 'allocations-jit'){$formalAllocation=$check.result}
        }
    }
    if($null -eq $formalAllocation -or $formalAllocation.status -ne 'pass'){
        throw 'Formal allocation gate did not pass'
    }
    $decls=@($decls|Sort-Object -Unique)
    $binding=Get-Content (Join-Path $Root "$pkg/src/color.xl") -Raw
    $expected=@([regex]::Matches($binding,'extern "(xcolor_\w+)"')|ForEach-Object{$_.Groups[1].Value}|Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxcolor_\w+(?=\()')|ForEach-Object Value|Sort-Object -Unique)
    if($decls.Count -ne 21 -or @(Compare-Object $expected $actual).Count){throw 'Incomplete color ABI coverage'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"color_native.h`"`n"+($decls -join "`n"),$utf8)
    $null=Check 'generated-21-prototypes' $gcc ($flags+@('-fsyntax-only',$abi))
    $generated=[IO.File]::ReadAllText((Join-Path $work 'color.c'))
    foreach($name in @('hex_text','hex_a_text','normalize_text','from_hsv_text',
        'from_hsl_text','mix_text','from_name_text','to_name_text')){
        if($generated -notmatch "(?m)\.Data\s*=\s*xcolor_$name\s*\("){
            throw "Owned color result was not captured directly: xcolor_$name"
        }
    }
    $checks.Add([ordered]@{name='owned-direct-cgen';result=[ordered]@{status='pass'}})
    Write-Host '[color] owned-direct-cgen: pass'
    $example="$pkg/examples/basic.c"; $exe=Join-Path $work 'example-c.exe'
    $null=Check 'example-c-build' $gcc ($flags+@('-DNDEBUG',$example,$runtime,'-lws2_32','-ladvapi32','-lm','-o',$exe))
    $native=Check 'example-c' $exe @()
    $cjit=Check 'example-c-jit' $xl @('run',$example)
    if(-not $SkipAot){
        $exe=Join-Path $work 'example-tcc.exe'
        $null=Check 'example-c-aot-build' $xl @('build',$example,'-o',$exe)
        $caot=Check 'example-c-aot' $exe @()
    }
    $helper=Check 'example-package-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','color','-Source',$example)
    $golden=[IO.File]::ReadAllText((Join-Path $Root 'release/examples/extlibs/color_basic.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $outputs=@($xlOutput,$native,$cjit,$helper)
    if(-not $SkipAot){$outputs+=@($caot)}
    foreach($output in $outputs){if($output -cne $golden){throw 'Color bilingual output mismatch'}}
    foreach($dll in @('xl','xlc')){
        $library=Join-Path $Root "release/$dll.dll"
        Copy-Item -LiteralPath $library -Destination (Join-Path $work "$dll.dll")
        $hostExe=Join-Path $work "$dll-host.exe"
        $null=Check "$dll-host-build" $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Irelease/include','test/support/extlib_runtime_host.c',$library,'-o',$hostExe)
        $null=Check "$dll-host-reuse" $hostExe @($Root,(Join-Path $Root "$pkg/test/boundaries.xl"))
    }
    $readme=[IO.File]::ReadAllText((Join-Path $Root "$pkg/README.md"))
    $site=[IO.File]::ReadAllText((Join-Path $Root "$pkg/docs/site.html"))
    $blocks=@([regex]::Matches($readme,'(?ms)^```xlang\r?\n(.*?)^```')|ForEach-Object{$_.Groups[1].Value})
    $blocks+=@([regex]::Matches($site,'(?s)<code class="language-xlang">(.*?)</code>')|ForEach-Object{[Net.WebUtility]::HtmlDecode($_.Groups[1].Value)})
    if($blocks.Count -ne 2){throw 'Color documentation block count changed'}
    for($i=0;$i -lt $blocks.Count;++$i){
        $source=Join-Path $work "doc-$i.xl"
        [IO.File]::WriteAllText($source,$blocks[$i],$utf8)
        $output=Language "documentation-$i" $source 'run' (Join-Path $work "doc-$i")
        if($output -cne "#3366CC`n#808080"){throw "Color documentation-$i mismatch"}
    }
    $outcome='passed-scoped'
} catch {$failure=$_.Exception.Message;throw} finally {
    $changed=@($inputs.Keys|Where-Object{!(Test-Path $_) -or (Get-FileHash $_).Hash -ne $inputs[$_]})
    $changed+=@(InputPaths|Where-Object{!$inputs.Contains($_)})
    if($changed.Count){$outcome='changed-inputs'}
    $record=[ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;releaseReady=$false;
        aotSkipped=[bool]$SkipAot;
        scope='Limited color C/XL ABI, ownership, math oracle, fault injection, DLL boundary reuse and bilingual delivery; source driver is not formal product rebuild';
        compiler=if($compiler){$compiler.Driver}else{$xl};inputs=$inputs;changedInputs=$changed;checks=$checks.ToArray();formalAllocationObservation=$formalAllocation}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($record|ConvertTo-Json -Depth 10),$utf8)
}
if($outcome -ne 'passed-scoped'){throw "Color gate outcome: $outcome"}
