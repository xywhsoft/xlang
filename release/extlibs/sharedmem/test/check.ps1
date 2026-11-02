param([switch]$SkipAot)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$gcc=(Get-Command gcc -CommandType Application -ErrorAction Stop|Select-Object -First 1).Source
$python=(Get-Command python -CommandType Application -ErrorAction Stop|Select-Object -First 1).Source
$shell=(Get-Process -Id $PID).Path
$cc1=''; $pkg='release/extlibs/sharedmem'; $xl=Join-Path $root 'release/xl.exe'
$parent=Join-Path $root 'build/test'
$work=Join-Path $parent ('sharedmem_guard_'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
$checks=[Collections.Generic.List[object]]::new()
$issues=[Collections.Generic.List[string]]::new()
$hashes=[ordered]@{}; $outcome='failed'; $failure=''
$flags=@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Ilib/xrt',"-I$pkg")
$links=@('-lWs2_32','-lAdvapi32','-lm')
function Inputs {
    Get-ChildItem (Join-Path $root $pkg),(Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
      'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/run_extlib_c.ps1',
      'test/cases/extlib_sharedmem.xl','release/examples/extlibs/sharedmem.xl')|ForEach-Object {Join-Path $root $_}
    $gcc; $python; $shell
    if($cc1){$cc1}
}
foreach($path in @(Inputs|Sort-Object -Unique)){$hashes[$path]=(Get-FileHash -LiteralPath $path).Hash}
function Run([string]$Name,[string]$Program,[string[]]$Arguments,[int]$Expected=0,[bool]$Collect=$false) {
    if($null -eq $Arguments){$Arguments=@()}
    $r=Invoke-ExtlibSmokeProcess $Program $Arguments $root 120
    $checks.Add([ordered]@{name=$Name;program=$Program;arguments=$Arguments;expected=$Expected;status=$r.status;exitCode=$r.exitCode;elapsedMs=$r.elapsedMs;stdout=$r.stdout;stderr=$r.stderr})
    if($r.exitCode -ne $Expected) {
        if($Collect){$issues.Add("$Name expected $Expected, actual=$($r.exitCode)")}
        else {throw "$Name failed: $($r.status) exit=$($r.exitCode)`n$($r.stderr)`n$($r.stdout)"}
    }
    Write-Host "[sharedmem] $Name : exit=$($r.exitCode) expected=$Expected"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
function Golden([string]$Path,[string]$Text) {
    if($Text -cne [IO.File]::ReadAllText((Join-Path $root $Path)).Replace("`r`n","`n").TrimEnd("`n")){throw "Golden mismatch: $Path"}
}
Push-Location $root
try {
    $null=Run compiler $gcc @('--version')
    $cc1=Run cc1 $gcc @('-print-prog-name=cc1'); $hashes[$cc1]=(Get-FileHash $cc1).Hash
    $runtime=Join-Path $work 'runtime.o'; $native=Join-Path $work 'native.o'
    $null=Run runtime $gcc ($flags+@('-c',"$pkg/test/xrt_runtime.c",'-o',$runtime))
    $null=Run native $gcc ($flags+@('-Wmissing-prototypes','-c',"$pkg/sharedmem_native.c",'-o',$native))
    $header=Join-Path $work 'header.c'
    [IO.File]::WriteAllText($header,"#include `"include/sharedmem_native.h`"`nint main(void) { return xshm_write_text(0, 0, (xstrview){0,0}); }`n",$utf8)
    $null=Run public-header $gcc ($flags+@('-fsyntax-only',$header))
    $core=Join-Path $work 'core.exe'
    $null=Run core-compile $gcc ($flags+@("$pkg/test/test_sharedmem.c",$native,$runtime)+$links+@('-o',$core))
    $coreText=Run core-real-process $core @()
    if($coreText -notmatch 'cross-process: real child'){throw 'GCC core did not run its real child'}
    $null=Run core-JIT-host $xl @('run',"$pkg/test/test_sharedmem.c","$pkg/sharedmem_native.c",'--inc-path','lib/xrt')
    if(!$SkipAot) {
        $binary=Join-Path $work 'core-aot.exe'
        $null=Run core-build $xl @('build',"$pkg/test/test_sharedmem.c","$pkg/sharedmem_native.c",'--inc-path','lib/xrt','-o',$binary)
        $text=Run core-AOT-real-process $binary @()
        if($text -notmatch 'cross-process: real child'){throw 'AOT core did not run its real child'}
    }
    $binary=Join-Path $work 'allocations.exe'
    $null=Run allocation-compile $gcc ($flags+@('-DNDEBUG',"$pkg/test/test_sharedmem_alloc.c",$native,$runtime)+$links+@('-o',$binary))
    $null=Run allocations $binary @()
    foreach($name in @('error_contract_c','layout_contract_c')) {
        $source="$pkg/test/$name.c"; $binary=Join-Path $work "$name.exe"
        $null=Run "$name-compile" $gcc ($flags+@($source,$runtime)+$links+@('-o',$binary))
        $normal=Run "$name-GCC" $binary @()
        $direct=Run "$name-TCC" $xl @('run',$source)
        if($normal -cne $direct){throw "$name GCC/TCC mismatch"}
        if($name -eq 'layout_contract_c') {
            $records=Join-Path $work 'layout.records.tsv'
            [IO.File]::WriteAllText($records,(Run layout-records $binary @('--records'))+"`n",$utf8)
            $null=Run independent-layout $python @('-B',"$pkg/test/check_layout.py",$records)
        }
        if(!$SkipAot) {
            $binary=Join-Path $work "$name-aot.exe"
            $null=Run "$name-build" $xl @('build',$source,'-o',$binary)
            if((Run "$name-AOT" $binary @()) -cne $normal){throw "$name AOT mismatch"}
        }
    }
    foreach($entry in @(@("$pkg/examples/sharedmem_c.c","$pkg/examples/basic.expected.txt"),@("$pkg/examples/snapshots_c.c","$pkg/examples/snapshots.expected.txt"))) {
        $source=$entry[0];$golden=$entry[1];$name=[IO.Path]::GetFileNameWithoutExtension($source)
        $binary=Join-Path $work "$name.exe"
        $null=Run "$name-compile" $gcc ($flags+@($source,$native,$runtime)+$links+@('-o',$binary))
        Golden $golden (Run "$name-GCC" $binary @())
        Golden $golden (Run "$name-TCC" $xl @('run',$source))
        if(!$SkipAot) {
            $binary=Join-Path $work "$name-aot.exe"
            $null=Run "$name-build" $xl @('build',$source,'-o',$binary)
            Golden $golden (Run "$name-AOT" $binary @())
        }
    }
    foreach($source in @('test/cases/extlib_sharedmem.xl',"$pkg/test/boundaries.xl","$pkg/test/allocations.xl","$pkg/examples/snapshots.xl",'release/examples/extlibs/sharedmem.xl',"$pkg/test/global_teardown_repro.xl","$pkg/test/local_teardown_control.xl")) {
        $name=[IO.Path]::GetFileNameWithoutExtension($source)
        $jit=Run "$name-JIT" $xl @('run',$source)
        if($name -eq 'snapshots'){Golden "$pkg/examples/snapshots.expected.txt" $jit}
        if($name -eq 'sharedmem'){Golden "$pkg/examples/basic.expected.txt" $jit}
        if(!$SkipAot) {
            $binary=Join-Path $work "$name-aot.exe"
            $null=Run "$name-build" $xl @('build',$source,'-o',$binary)
            $collect=$name -in @('boundaries','sharedmem','global_teardown_repro')
            $aot=Run "$name-AOT" $binary @() 0 $collect
            if($checks[$checks.Count-1].exitCode -eq 0 -and $aot -cne $jit){throw "$name JIT/AOT mismatch"}
        }
    }
    $decls=@()
    foreach($source in @('test/cases/extlib_sharedmem.xl',"$pkg/test/boundaries.xl")) {
        $module=Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($source)+'.c')
        $null=Run ('cgen-'+[IO.Path]::GetFileNameWithoutExtension($source)) $xl @('cgen',$source,'--emit',$module)
        $decls+=Select-String -LiteralPath $module -Pattern '^extern .*xshm_[a-z_0-9]+\('|ForEach-Object Line
    }
    $decls=@($decls|Sort-Object -Unique)
    $expected=@(Select-String -LiteralPath "$pkg/src/sharedmem.xl" -Pattern 'extern "(xshm_[a-z_0-9]+)"'|ForEach-Object {$_.Matches.Groups[1].Value}|Sort-Object -Unique)
    $actual=@($decls|ForEach-Object {[regex]::Match($_,'xshm_[a-z_0-9]+').Value}|Sort-Object -Unique)
    if($decls.Count -ne 13 -or @(Compare-Object $expected $actual).Count){throw 'Incomplete generated ABI'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/sharedmem_native.h`"`n"+($decls -join "`n"),$utf8)
    $null=Run generated-ABI $gcc ($flags+@('-fsyntax-only',$abi))
    [IO.File]::WriteAllText($abi,([IO.File]::ReadAllText($abi).Replace('int64_t xshm_size','double xshm_size')),$utf8)
    $null=Run wrong-return-control $gcc ($flags+@('-fsyntax-only',$abi)) 1
    $null=Run private-native $xl @('check',"$pkg/test/private_native_negative.xl") 1
    if(($checks[$checks.Count-1].stdout+$checks[$checks.Count-1].stderr) -notmatch 'access'){throw 'Missing private access diagnostic'}
    $null=Run private-constructor $xl @('check',"$pkg/test/private_constructor_negative.xl") 1 $true
    if($issues.Count){throw ('sharedmem release blockers: '+($issues -join '; '))}
    $outcome='passed'
} catch {$failure=$_.Exception.Message;throw} finally {
    $changed=@($hashes.Keys|Where-Object {!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $hashes[$_]})
    $changed+=@(Inputs|Where-Object {!$hashes.Contains($_)})
    if($changed.Count){$outcome='changed-inputs'}
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{outcome=$outcome;failure=$failure;issues=$issues.ToArray();skipAot=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=$checks.ToArray()}|ConvertTo-Json -Depth 10),$utf8)
    Pop-Location
    if(!$work.StartsWith([IO.Path]::GetFullPath($parent)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe sharedmem cleanup path'}
    if($outcome -eq 'passed'){Remove-Item -LiteralPath $work -Recurse -Force}
    else {Write-Warning "sharedmem failure artifacts retained: $work"}
    Write-Host "[sharedmem] report: $work.json"
}
if($outcome -ne 'passed'){throw "sharedmem gate outcome: $outcome"}
