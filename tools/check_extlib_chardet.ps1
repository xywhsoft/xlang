param([string]$Root='', [string]$RuntimeDirectory='', [string]$WorkDirectory='')
$ErrorActionPreference='Stop'
if (!$Root) { $Root=Split-Path -Parent $PSScriptRoot }
$Root=(Resolve-Path $Root).Path
if (!$RuntimeDirectory) { $RuntimeDirectory=Join-Path $Root 'release' }
$RuntimeDirectory=(Resolve-Path $RuntimeDirectory).Path
$work=if ($WorkDirectory) {[IO.Path]::GetFullPath($WorkDirectory)} else {Join-Path $Root 'build/test/chardet_guard'}
[void][IO.Directory]::CreateDirectory($work)
. (Join-Path $PSScriptRoot 'extlib_smoke_process.ps1')
$xl=Join-Path $RuntimeDirectory 'xl.exe'
$gcc=(Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$pkg='release/extlibs/chardet'
$manifest=Get-Content (Join-Path $Root "$pkg/xlang.package.json") -Raw | ConvertFrom-Json
$sources=@($manifest.artifact.cSources | ForEach-Object {"$pkg/$_"})
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',"-I$pkg/include")
$cases=@('test/cases/extlib_chardet.xl',"$pkg/test/boundaries.xl",'release/examples/extlibs/chardet.xl','release/examples/extlibs/chardet_basic.xl')
$checks=[Collections.Generic.List[object]]::new()
$inputs=[ordered]@{}
$utf8=[Text.UTF8Encoding]::new($false)
function InputPaths {
    $gcc; $PSCommandPath
    Get-ChildItem $RuntimeDirectory -File | Where-Object Name -in @('xl.exe','xl.dll','xlc.dll') | ForEach-Object FullName
    Get-ChildItem (Join-Path $RuntimeDirectory 'include') -Recurse -File | ForEach-Object FullName
    Get-ChildItem (Join-Path $Root $pkg) -Recurse -File | ForEach-Object FullName
    foreach($file in $cases+@('lib/xrt/xrt.h','lib/xruntime/xruntime.h','test/support/extlib_chardet_runtime.c','test/support/extlib_runtime_host.c',
        'release/examples/extlibs/chardet_basic.expected.txt','tools/check_extlib_chardet.sh',
        'tools/run_extlib_c.ps1','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) {Join-Path $Root $file}
}
foreach($path in @(InputPaths | Sort-Object -Unique)) {$inputs[$path]=(Get-FileHash -LiteralPath $path).Hash}
function Check([string]$name,[string]$program,[string[]]$arguments) {
    $r=Invoke-ExtlibSmokeProcess $program $arguments $Root 120 @{XLANG_PACKAGE_PATH=(Join-Path $Root 'release/extlibs')}
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    Write-Host "[chardet] ${name}: $($r.status)"
    if($r.status -ne 'pass') {throw "$name failed: $($r.stderr)`n$($r.stdout)"}
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
$outcome='failed'
$decls=@()
try {
    $runtime=Join-Path $work 'runtime.o'
    $null=Check 'runtime-build' $gcc ($flags+@('-c','test/support/extlib_chardet_runtime.c','-o',$runtime))
    $objects=@()
    foreach($source in $sources) {
        $object=Join-Path $work "native-$($objects.Count).o"
        $null=Check "native-$($objects.Count)" $gcc ($flags+@('-c',$source,'-o',$object))
        $objects+=$object
    }
    $links=$objects+@($runtime,'-lws2_32','-ladvapi32','-lm')
    foreach($test in @('test_chardet','test_threads')) {
        $exe=Join-Path $work "$test.exe"
        $null=Check "$test-build" $gcc ($flags+@("$pkg/test/$test.c")+$links+@('-o',$exe))
        $null=Check $test $exe @()
    }
    foreach($case in $cases) {
        $name=[IO.Path]::GetFileNameWithoutExtension($case)
        $jit=Check "$name-jit" $xl @('run',$case)
        $exe=Join-Path $work "$name.exe"
        $null=Check "$name-build" $xl @('build',$case,'-o',$exe)
        $aot=Check "$name-aot" $exe @()
        if($jit -cne $aot) {throw "$name output differs"}
        if($name -eq 'chardet_basic') {$xlOutput=$jit}
        $module=Join-Path $work "$name.c"
        $null=Check "$name-cgen" $xl @('cgen',$case,'--emit',$module)
        $text=Get-Content $module -Raw
        $decls+=@(Get-Content $module | Where-Object {$_ -match '^extern .*\bxchardet_\w+\('})
        if($name -eq 'boundaries' -and ($text -notmatch 'xrtStrDup\(xchardet_detect_name_n\(' -or
            $text -notmatch 'xrtStrDup\(xchardet_detect_name_str\(')) {throw 'Borrowed string results must be copied'}
    }
    $decls=@($decls | Sort-Object -Unique)
    $binding=Get-Content (Join-Path $Root "$pkg/src/chardet.xl") -Raw
    $expected=@([regex]::Matches($binding,'extern "(xchardet_\w+)"') | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
    $actual=@([regex]::Matches(($decls -join "`n"),'\bxchardet_\w+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if(@(Compare-Object $expected $actual).Count -or $decls.Count -ne 10) {throw 'Incomplete ABI coverage'}
    [IO.File]::WriteAllText((Join-Path $work 'declarations.inc'),($decls -join "`n")+"`n",$utf8)
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"chardet.h`"`n#include `"declarations.inc`"`n",$utf8)
    $null=Check 'generated-prototypes' $gcc ($flags+@('-fsyntax-only',$abi))
    $example="$pkg/examples/basic.c"
    $exe=Join-Path $work 'c-example.exe'
    $null=Check 'c-example-build' $gcc ($flags+@($example)+$links+@('-o',$exe))
    $cOutput=Check 'c-example' $exe @()
    $options=@('--inc-path','lib/xrt','--inc-path',"$pkg/include")
    $cJit=Check 'c-example-jit' $xl (@('run',$example)+$sources+$options)
    $exe=Join-Path $work 'c-tcc.exe'
    $null=Check 'c-example-aot-build' $xl (@('build',$example)+$sources+$options+@('-o',$exe))
    $cAot=Check 'c-example-aot' $exe @()
    $helper=Check 'c-example-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','chardet','-Source',$example,'-RuntimeDirectory',$RuntimeDirectory)
    $null=Check 'c-smoke-runner' (Get-Process -Id $PID).Path @('-NoProfile','-File','tools/run_extlib_c.ps1','-Package','chardet','-Source',"$pkg/test/test_chardet.c",'-RuntimeDirectory',$RuntimeDirectory)
    foreach($dll in @('xl','xlc')) {
        $library=Join-Path $RuntimeDirectory "$dll.dll"
        Copy-Item -LiteralPath $library -Destination (Join-Path $work "$dll.dll") -Force
        $hostExe=Join-Path $work "$dll-host.exe"
        $null=Check "$dll-host-build" $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror',
            '-I',(Join-Path $RuntimeDirectory 'include'),'test/support/extlib_runtime_host.c',$library,'-o',$hostExe)
        $null=Check "$dll-host-reuse" $hostExe @($Root,(Join-Path $Root "$pkg/test/boundaries.xl"))
    }
    $golden=(Get-Content (Join-Path $Root 'release/examples/extlibs/chardet_basic.expected.txt') -Raw).Replace("`r`n","`n").TrimEnd("`n")
    foreach($output in @($xlOutput,$cOutput,$cJit,$cAot,$helper)) {if($output -cne $golden) {throw 'Paired example differs from golden'}}
    $outcome='passed-scoped'
} finally {
    $changed=@($inputs.Keys | Where-Object {!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $inputs[$_]})
    $changed+=@(InputPaths | Where-Object {!$inputs.Contains($_)})
    if($changed.Count) {$outcome='invalid-input-change'}
    $record=[ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;runtimeDirectory=$RuntimeDirectory;
        scope='Windows C/XL JIT/AOT, xl/xlc DLL context reuse, 10 generated physical ABIs, borrowed string copies, zero-allocation, threads and paired examples. No other package or platform admission.';
        inputs=$inputs;changedInputs=$changed;generatedPrototypes=$decls.Count;checks=$checks.ToArray()}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($record | ConvertTo-Json -Depth 8),$utf8)
}
if($outcome -ne 'passed-scoped') {throw "Chardet gate failed: $work/results.json"}
