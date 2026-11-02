param([switch]$SkipAot)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$gcc=(Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl=Join-Path $root 'release/xl.exe'
$shell=(Get-Process -Id $PID).Path
$cc1=''
$testRoot=Join-Path $root 'build/test'
$work=Join-Path $testRoot ('daemon_guard_'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
$checks=[Collections.Generic.List[object]]::new()
$hashes=[ordered]@{}
$outcome='failed'
$failure=''
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt','-Irelease/extlibs/daemon/include')
$links=@('-lWs2_32','-lAdvapi32','-lm')
$pkg='release/extlibs/daemon'
function InputPaths {
    Get-ChildItem (Join-Path $root $pkg),(Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h','lib/xruntime/xruntime.h',
      'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
      'test/cases/extlib_daemon.xl','release/examples/extlibs/daemon.xl','release/examples/extlibs/daemon_c.c') | ForEach-Object {Join-Path $root $_}
    $gcc; $shell
    if($cc1){$cc1}
}
foreach($path in @(InputPaths | Sort-Object -Unique)){$hashes[$path]=(Get-FileHash -LiteralPath $path).Hash}
function Run([string]$Name,[string]$Program,[string[]]$Arguments,[int]$Expected=0) {
    if($null -eq $Arguments){$Arguments=@()}
    $r=Invoke-ExtlibSmokeProcess $Program $Arguments $root 120
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;expected=$Expected;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if($r.exitCode -ne $Expected){throw "$Name failed: status=$($r.status) exit=$($r.exitCode)`n$($r.stderr)`n$($r.stdout)"}
    Write-Host "[daemon] $Name : exit=$($r.exitCode)"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
function Golden([string]$Path,[string]$Text) {
    $expected=[IO.File]::ReadAllText((Join-Path $root $Path)).Replace("`r`n","`n").TrimEnd("`n")
    if(($Text -replace 'pid=\d+','pid=<pid>') -cne $expected){throw "Golden output mismatch: $Path"}
}
Push-Location $root
try {
    $null=Run compiler $gcc @('--version')
    $cc1=Run cc1 $gcc @('-print-prog-name=cc1')
    $hashes[$cc1]=(Get-FileHash -LiteralPath $cc1).Hash
    $manifest=Get-Content "$pkg/xlang.package.json" -Raw -Encoding UTF8|ConvertFrom-Json
    $moduleFlags=@($manifest.artifact.xrtModules|ForEach-Object {"-D$_"})
    $null=Run manifest-modules $gcc ($flags+$moduleFlags+@('-fsyntax-only',"$pkg/daemon_native.c"))
    $runtime=Join-Path $work 'runtime.o'
    $native=Join-Path $work 'native.o'
    $null=Run runtime $gcc ($flags+@('-c',"$pkg/test/xrt_runtime.c",'-o',$runtime))
    $null=Run native $gcc ($flags+@('-Wmissing-prototypes','-c',"$pkg/daemon_native.c",'-o',$native))
    $binary=Join-Path $work 'core.exe'
    $null=Run C-core-compile $gcc ($flags+@("$pkg/test/test_daemon.c",$native,$runtime)+$links+@('-o',$binary))
    $null=Run C-core $binary @()
    $null=Run C-core-package-runner $shell @('-NoProfile','-ExecutionPolicy','Bypass','-File','tools/run_extlib_c.ps1','-Package','daemon','-Source',"$pkg/test/test_daemon.c")
    foreach($case in @('cleanup_contract_c','pid_liveness_c','pid_identity_c')) {
        $source="$pkg/test/$case.c"
        $binary=Join-Path $work "$case.exe"
        $null=Run "$case-compile" $gcc ($flags+@($source,$runtime)+$links+@('-o',$binary))
        $normal=Run "$case-GCC" $binary @()
        $direct=Run "$case-TCC" $xl @('run',$source)
        if($direct -cne $normal){throw "$case GCC/TCC output differs"}
        if(!$SkipAot) {
            $binary=Join-Path $work "$case-aot.exe"
            $null=Run "$case-build" $xl @('build',$source,'-o',$binary)
            $aot=Run "$case-AOT" $binary @()
            if($aot -cne $normal){throw "$case GCC/AOT output differs"}
        }
    }
    foreach($source in @('test/cases/extlib_daemon.xl',"$pkg/test/ffi_boundaries.xl")) {
        $name=[IO.Path]::GetFileNameWithoutExtension($source)
        $jit=Run "$name-JIT" $xl @('run',$source)
        if(!$SkipAot) {
            $binary=Join-Path $work "$name.exe"
            $null=Run "$name-build" $xl @('build',$source,'-o',$binary)
            $aot=Run "$name-AOT" $binary @()
            if($jit -cne $aot){throw "$name JIT/AOT output differs"}
        }
    }
    $module=Join-Path $work 'module.c'
    $null=Run cgen $xl @('cgen','test/cases/extlib_daemon.xl','--emit',$module)
    $decls=@(Select-String -LiteralPath $module -Pattern '^extern .*xdaemon_[a-z_]+\('|ForEach-Object Line|Sort-Object -Unique)
    $expected=@(Select-String -LiteralPath "$pkg/include/daemon_native.h" -Pattern 'xdaemon_[a-z_]+\('|ForEach-Object {[regex]::Match($_.Line,'xdaemon_[a-z_]+').Value}|Sort-Object -Unique)
    $actual=@($decls|ForEach-Object {[regex]::Match($_,'xdaemon_[a-z_]+').Value}|Sort-Object -Unique)
    if($decls.Count -ne 11 -or @(Compare-Object $expected $actual).Count){throw 'Incomplete public ABI extraction'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"daemon_native.h`"`n"+($decls -join "`n"),$utf8)
    $null=Run generated-ABI $gcc ($flags+@('-fsyntax-only',$abi))
    [IO.File]::WriteAllText($abi,([IO.File]::ReadAllText($abi).Replace('int64_t xdaemon_is_alive','double xdaemon_is_alive')),$utf8)
    $null=Run ABI-wrong-return-control $gcc ($flags+@('-fsyntax-only',$abi)) 1
    foreach($negative in @('private_native_negative','private_status_negative','private_getter_negative','unexported_constructor_negative')) {
        $null=Run $negative $xl @('check',"$pkg/test/$negative.xl") 1
        if(($checks[$checks.Count-1].stdout+$checks[$checks.Count-1].stderr) -notmatch '\[(sem|import)\]'){throw 'Missing access diagnostic'}
    }
    foreach($entry in @(@('release/examples/extlibs/daemon_c.c',"$pkg/test/daemon.expected.txt"),@("$pkg/examples/restart_c.c","$pkg/examples/restart.expected.txt"))) {
        $source=$entry[0]; $golden=$entry[1]; $name=[IO.Path]::GetFileNameWithoutExtension($source)
        $binary=Join-Path $work "$name-gcc.exe"
        $null=Run "$name-compile" $gcc ($flags+@($source,$native,$runtime)+$links+@('-o',$binary))
        Golden $golden (Run "$name-GCC" $binary @())
        Golden $golden (Run "$name-TCC" $xl @('run',$source))
        if(!$SkipAot) {
            $binary=Join-Path $work "$name-aot.exe"
            $null=Run "$name-build" $xl @('build',$source,'-o',$binary)
            Golden $golden (Run "$name-AOT" $binary @())
        }
    }
    foreach($entry in @(@('release/examples/extlibs/daemon.xl',"$pkg/test/daemon.expected.txt"),@("$pkg/examples/restart.xl","$pkg/examples/restart.expected.txt"))) {
        $source=$entry[0]; $golden=$entry[1]; $name=[IO.Path]::GetFileNameWithoutExtension($source)
        Golden $golden (Run "$name-JIT" $xl @('run',$source))
        if(!$SkipAot) {
            $binary=Join-Path $work "$name-aot.exe"
            $null=Run "$name-build" $xl @('build',$source,'-o',$binary)
            Golden $golden (Run "$name-AOT" $binary @())
        }
    }
    $null=Run top-level-JIT $xl @('run',"$pkg/test/top_level.xl")
    if(!$SkipAot) {
        $binary=Join-Path $work 'top-level.exe'
        $null=Run top-level-build $xl @('build',"$pkg/test/top_level.xl",'-o',$binary)
        $null=Run top-level-AOT $binary @()
    }
    $outcome='passed'
} catch { $failure=$_.Exception.Message; throw } finally {
    $changed=@($hashes.Keys|Where-Object {!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $hashes[$_]})
    $changed+=@(InputPaths|Where-Object {!$hashes.Contains($_)})
    if($changed.Count){$outcome='changed-inputs'}
    $report=[ordered]@{outcome=$outcome;failure=$failure;skipAot=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=$checks.ToArray()}
    [IO.File]::WriteAllText(($work+'.json'),($report|ConvertTo-Json -Depth 10),$utf8)
    Pop-Location
    if(!$work.StartsWith([IO.Path]::GetFullPath($testRoot)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe daemon work path'}
    if($outcome -eq 'passed'){Remove-Item -LiteralPath $work -Recurse -Force}
    else {Write-Warning "daemon failure artifacts retained: $work"}
    Write-Host "[daemon] report: $work.json"
}
if($outcome -ne 'passed'){throw "daemon gate: $outcome"}
