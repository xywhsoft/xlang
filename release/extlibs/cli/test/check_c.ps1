param([string]$Root='')
$ErrorActionPreference='Stop'
if(!$Root){$Root=Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))}
$Root=(Resolve-Path -LiteralPath $Root).Path
. (Join-Path $Root 'tools/extlib_smoke_process.ps1')
$gcc=(Get-Command gcc -CommandType Application -ErrorAction Stop|Select-Object -First 1).Source
$xl=Join-Path $Root 'release/xl.exe'
$base=Join-Path $Root 'build/test/cli_guard'
$work=Join-Path $base ('run_'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$utf8=[Text.UTF8Encoding]::new($false)
$checks=[Collections.Generic.List[object]]::new()
$problems=[Collections.Generic.List[string]]::new()
$environment=@{XLANG_PACKAGE_PATH=(Join-Path $Root 'release/extlibs')}
$paths=@(Get-ChildItem (Join-Path $Root 'release/extlibs/cli') -Recurse -File|ForEach-Object FullName)
$paths+=@(Get-ChildItem (Join-Path $Root 'release/include') -Recurse -File|ForEach-Object FullName)
$paths+=@('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
    'tools/extlib_smoke_process.ps1','test/cases/extlib_cli.xl',
    'dev/extlib_blockers/cli_os_args_symbol.xl','dev/extlib_blockers/cli_args_projection.xl',
    'dev/extlib_blockers/cli_args_projection.c')|ForEach-Object{Join-Path $Root $_}
$paths+=$gcc
$inputs=[ordered]@{}
foreach($path in @($paths|Sort-Object -Unique)){$inputs[$path]=(Get-FileHash -LiteralPath $path).Hash}
function Run([string]$Name,[string]$Program,[string[]]$Arguments){
    if($null -eq $Arguments){$Arguments=@()}
    $r=Invoke-ExtlibSmokeProcess $Program $Arguments $Root 120 $environment
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;result=$r})
    if($r.status -ne 'pass'){throw "$Name failed: status=$($r.status), exit=$($r.exitCode)$([Environment]::NewLine)$($r.stderr)$([Environment]::NewLine)$($r.stdout)"}
    Write-Host "[cli] ${Name}: PASS"
    $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
function Observe([scriptblock]$Action){
    try { & $Action } catch { $problems.Add($_.Exception.Message); Write-Host "[cli] FAIL: $($_.Exception.Message)" }
}
function Golden([string]$Actual,[string]$Path){
    $expected=[IO.File]::ReadAllText((Join-Path $Root $Path)).Replace("`r`n","`n").TrimEnd("`n")
    if($Actual -cne $expected){throw "CLI golden mismatch: $Path"}
}
$flags=@('-std=c11','-O2','-DNDEBUG','-Wall','-Wextra','-Werror','-Ilib/xrt')
$outcome='failed';$failure=''
try {
    $null=Run compiler-info $gcc @('--version')
    $runtime=Join-Path $work 'runtime.o'
    $native=Join-Path $work 'native.o'
    $null=Run runtime-compile $gcc ($flags+@('-c','release/extlibs/cli/test/xrt_runtime.c','-o',$runtime))
    $null=Run native-compile $gcc ($flags+@('-Wmissing-prototypes','-c','release/extlibs/cli/cli_native.c','-o',$native))
    foreach($name in @('test_cli','test_defaults')){
        $exe=Join-Path $work "$name.exe"
        $null=Run "$name-compile" $gcc ($flags+@("release/extlibs/cli/test/$name.c",$runtime,$native,'-lm','-o',$exe))
        $null=Run "$name-run" $exe @()
    }
    foreach($name in @('basic','options')){
        $source="release/extlibs/cli/examples/${name}_c.c"
        $golden="release/extlibs/cli/examples/$name.expected.txt"
        $exe=Join-Path $work "$name-c.exe"
        $null=Run "$name-c-compile" $gcc ($flags+@($source,$runtime,$native,'-lm','-o',$exe))
        Golden (Run "$name-c-run" $exe @()) $golden
        Golden (Run "$name-c-direct" $xl @('run',$source)) $golden
        $exe=Join-Path $work "$name-c-aot.exe"
        $null=Run "$name-c-aot-build" $xl @('build',$source,'-o',$exe)
        Golden (Run "$name-c-aot-run" $exe @()) $golden
    }
    $sources=@(
        @{name='central';path='test/cases/extlib_cli.xl'},
        @{name='basic';path='release/extlibs/cli/examples/basic.xl';golden='release/extlibs/cli/examples/basic.expected.txt'},
        @{name='options';path='release/extlibs/cli/examples/options.xl';golden='release/extlibs/cli/examples/options.expected.txt'},
        @{name='os-args';path='dev/extlib_blockers/cli_os_args_symbol.xl'},
        @{name='projection';path='dev/extlib_blockers/cli_args_projection.xl'}
    )
    $generated=[Collections.Generic.List[string]]::new()
    # 各正例仍要求0。缺符号、投影分别保留原始失败，不删除parseOs绕过问题。
    foreach($case in $sources){
        $name=$case.name;$source=$case.path
        $null=Observe { Run "$name-check" $xl @('check',$source) }
        $jit=Observe { Run "$name-jit" $xl @('run',$source) }
        $exe=Join-Path $work "lang-$name.exe"
        $built=Observe { $null=Run "$name-build" $xl @('build',$source,'-o',$exe); $true }
        if($built){
            $aot=Observe { Run "$name-aot" $exe @() }
            if($null -ne $jit -and $null -ne $aot -and $jit -cne $aot){$problems.Add("$name JIT/AOT output differs")}
            if($case.golden -and $null -ne $aot){$null=Observe { Golden $aot $case.golden }}
        }
        if($case.golden -and $null -ne $jit){$null=Observe { Golden $jit $case.golden }}
        $module=Join-Path $work "module-$name.c"
        $null=Run "$name-cgen" $xl @('cgen',$source,'--emit',$module)
        $generated.Add($module)
    }
    $decls=@($generated|ForEach-Object{Get-Content -LiteralPath $_|Where-Object{$_ -match '^extern .*\bxcli_parse\('}}|Sort-Object -Unique)
    if($decls.Count -ne 1){throw "Expected one real CLI ABI declaration, got $($decls.Count)"}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"cli_native.h`"`n"+$decls[0]+"`n",$utf8)
    $null=Run generated-abi $gcc ($flags+@('-Irelease/extlibs/cli/include','-fsyntax-only',$abi))
    if($problems.Count){throw ($problems -join "`n")}
    $outcome='passed'
} catch { $failure=$_.Exception.Message; throw } finally {
    $changed=@($inputs.Keys|Where-Object{!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $inputs[$_]})
    if($changed.Count){$outcome='changed-inputs'}
    $record=[ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;releaseReady=$false;
        scope='Native parser/default fault/lifetime, exact C examples, actual ABI and real language JIT/AOT boundaries';
        inputs=$inputs;changedInputs=$changed;checks=$checks.ToArray()}
    $json=$record|ConvertTo-Json -Depth 10
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),$json,$utf8)
    [IO.File]::WriteAllText("$work.json",$json,$utf8)
    if($outcome -eq 'passed'){
        $resolved=[IO.Path]::GetFullPath((Resolve-Path -LiteralPath $work).Path)
        $prefix=[IO.Path]::GetFullPath($base).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
        if(!$resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'CLI work directory escaped build/test'}
        Remove-Item -LiteralPath $resolved -Recurse -Force
    } else { Write-Host "[cli] retained artifacts: $work" }
}
if($outcome -ne 'passed'){throw "CLI gate outcome: $outcome"}
