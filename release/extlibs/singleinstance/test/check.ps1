param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$safeRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $safeRoot ('si_' + [guid]::NewGuid().ToString('N').Substring(0, 12))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/singleinstance')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$profileDir=Join-Path $work 'home'
$environment=@{USERPROFILE=$profileDir;XLANG_SI_TEST_HOME=$profileDir;XLANG_PACKAGE_PATH=(Join-Path $root 'release/extlibs');PATH=((Join-Path $root 'release')+';'+$env:PATH)}
$checks=[Collections.Generic.List[object]]::new()
$hashes=[ordered]@{};$passed=$false;$failure='';$cc1=''
function Inputs {
    Get-ChildItem (Join-Path $root 'release/extlibs/singleinstance'),(Join-Path $root 'release/include') -Recurse -File|ForEach-Object FullName
    @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','test/cases/extlib_singleinstance.xl','release/examples/extlibs/singleinstance.xl')|ForEach-Object {Join-Path $root $_}
    $gcc;(Get-Process -Id $PID).Path
    if($cc1){$cc1}
}
foreach($path in @(Inputs|Sort-Object -Unique)){$hashes[$path]=(Get-FileHash -LiteralPath $path).Hash}

function Run([string]$name, [string]$program, [string[]]$arguments) {
    if($null -eq $arguments){$arguments=@()}
    $result=Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if($result.status -ne 'pass'){throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)"}
    Write-Host "[singleinstance] ${name}: PASS"
    return $result.stdout.Replace("`r`n", "`n").TrimEnd("`n")
}

[IO.Directory]::CreateDirectory($work) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $work 'home')) | Out-Null
Push-Location $root
try {
    $null=Run compiler $gcc @('--version')
    $cc1=Run cc1 $gcc @('-print-prog-name=cc1');$hashes[$cc1]=(Get-FileHash -LiteralPath $cc1).Hash
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/singleinstance/test/runtime.c', '-o', $runtime))
    $native = Join-Path $work 'native.o'
    $null = Run 'native compile' $gcc ($flags + @('-c',
        'release/extlibs/singleinstance/singleinstance_native.c', '-o', $native))
    $test = Join-Path $work 'boundaries.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/singleinstance/test/test_singleinstance.c',
        $native, $runtime) + $links + @('-o', $test))
    if((Run 'C cross-process boundaries' $test @()) -notmatch 'cross-process: real child'){throw 'Missing real GCC child'}
    if(!$SkipAot){
        $binary=Join-Path $work 'core-aot.exe'
        $null=Run 'C core build' $xl @('build','release/extlibs/singleinstance/test/test_singleinstance.c','release/extlibs/singleinstance/singleinstance_native.c','--inc-path','lib/xrt','-o',$binary)
        if((Run 'C core AOT' $binary @()) -notmatch 'cross-process: real child'){throw 'Missing real AOT child'}
    }
    $contract=Join-Path $work 'contract.exe'
    $null=Run 'cleanup compile' $gcc ($flags+@('release/extlibs/singleinstance/test/cleanup_contract_c.c',$runtime)+$links+@('-o',$contract))
    $normal=Run 'cleanup GCC' $contract @()
    if((Run 'cleanup TCC' $xl @('run','release/extlibs/singleinstance/test/cleanup_contract_c.c')) -cne $normal){throw 'Cleanup GCC/TCC mismatch'}
    if(!$SkipAot){
        $binary=Join-Path $work 'cleanup-aot.exe'
        $null=Run 'cleanup build' $xl @('build','release/extlibs/singleinstance/test/cleanup_contract_c.c','-o',$binary)
        if((Run 'cleanup AOT' $binary @()) -cne $normal){throw 'Cleanup GCC/AOT mismatch'}
    }
    $moduleObjects = @()
    foreach ($instance in @('a', 'b')) {
        $object = Join-Path $work "module_${instance}.o"
        $null = Run "independent module $instance compile" $gcc ($flags + @(
            "-Dxsi_acquire=xsi_${instance}_acquire",
            "-Dxsi_release=xsi_${instance}_release",
            "-Dxsi_is_acquired=xsi_${instance}_is_acquired",
            '-c', 'release/extlibs/singleinstance/singleinstance_native.c', '-o', $object))
        $moduleObjects += $object
    }
    $modules = Join-Path $work 'modules.exe'
    $null = Run 'independent modules test compile' $gcc ($flags + @('-DNDEBUG',
        'release/extlibs/singleinstance/test/test_multiple_modules.c') +
        $moduleObjects + @($runtime) + $links + @('-o', $modules))
    $null = Run 'independent modules same-process boundaries' $modules @()
    $example = Join-Path $work 'example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/singleinstance/examples/singleinstance_c.c',
        $native, $runtime) + $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $direct = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/singleinstance/examples/singleinstance_c.c')
    $directExe = Join-Path $work 'example-direct.exe'
    $null = Run 'C example direct xl build' $xl @('build',
        'release/extlibs/singleinstance/examples/singleinstance_c.c', '-o', $directExe)
    $directBuilt = Run 'C example direct xl executable' $directExe @()
    $null = Run 'central XLang test' $xl @('run', 'test/cases/extlib_singleinstance.xl')
    $null = Run 'XLang boundaries' $xl @('run',
        'release/extlibs/singleinstance/test/boundaries.xl')
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/singleinstance.xl')
    if ($cOutput -cne $xlOutput -or $direct -cne $cOutput -or $directBuilt -cne $cOutput) {
        throw "C/XLang outputs differ:`nC:`n$cOutput`nXL:`n$xlOutput`nDirect:`n$direct`nBuilt:`n$directBuilt"
    }
    $expectedPath = Join-Path $root 'release/extlibs/singleinstance/test/singleinstance.expected.txt'
    $expected = [IO.File]::ReadAllText($expectedPath).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $expected) { throw 'singleinstance example differs from fixed output' }
    Write-Host '[singleinstance] C/XLang parity: PASS'
    $recovery=Join-Path $work 'recovery-c.exe'
    $null=Run 'C recovery compile' $gcc ($flags+@('release/extlibs/singleinstance/examples/recovery_c.c',$native,$runtime)+$links+@('-o',$recovery))
    $golden=[IO.File]::ReadAllText((Join-Path $root 'release/extlibs/singleinstance/examples/recovery.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    foreach($text in @((Run 'C recovery' $recovery @()),(Run 'C recovery direct' $xl @('run','release/extlibs/singleinstance/examples/recovery_c.c')),(Run 'XL recovery' $xl @('run','release/extlibs/singleinstance/examples/recovery.xl')))){
        if($text -cne $golden){throw 'Recovery golden mismatch'}
    }
    if (-not $SkipAot) {
        foreach ($source in @('test/cases/extlib_singleinstance.xl',
            'release/extlibs/singleinstance/test/boundaries.xl')) {
            $name = [IO.Path]::GetFileNameWithoutExtension($source)
            $binary = Join-Path $work "$name.exe"
            $null = Run "$name build" $xl @('build', $source, '-o', $binary)
            $null = Run "$name AOT" $binary @()
        }
        $target = Join-Path $work 'singleinstance.exe'
        $null = Run 'XLang example build' $xl @('build',
            'release/examples/extlibs/singleinstance.xl', '-o', $target)
        $aotOutput = Run 'XLang example AOT' $target @()
        if ($aotOutput -cne $expected) { throw 'XLang AOT example differs from fixed output' }
        foreach($source in @('release/extlibs/singleinstance/examples/recovery_c.c','release/extlibs/singleinstance/examples/recovery.xl')){
            $name=[IO.Path]::GetFileNameWithoutExtension($source);$binary=Join-Path $work "$name.exe"
            $null=Run "$name build" $xl @('build',$source,'-o',$binary)
            if((Run "$name AOT" $binary @()) -cne $golden){throw 'Recovery AOT mismatch'}
        }
    }
    $module=Join-Path $work 'module.c'
    $null=Run 'generated source' $xl @('cgen','release/extlibs/singleinstance/test/boundaries.xl','--emit',$module)
    $decls=@(Select-String -LiteralPath $module -Pattern '^extern .*xsi_[a-z_]+\('|ForEach-Object Line|Sort-Object -Unique)
    $expectedNames=@(Select-String -LiteralPath (Join-Path $root 'release/extlibs/singleinstance/src/singleinstance.xl') -Pattern 'extern "(xsi_[a-z_]+)"'|ForEach-Object {$_.Matches.Groups[1].Value}|Sort-Object -Unique)
    $actualNames=@($decls|ForEach-Object {[regex]::Match($_,'xsi_[a-z_]+').Value}|Sort-Object -Unique)
    if($decls.Count -ne 3 -or @(Compare-Object $expectedNames $actualNames).Count){throw 'Incomplete generated ABI'}
    $abi=Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"include/singleinstance_native.h`"`n"+($decls -join "`n"),[Text.UTF8Encoding]::new($false))
    $null=Run 'generated ABI' $gcc ($flags+@('-fsyntax-only',$abi))
    $context=Join-Path $work 'context.exe'
    $null=Run 'contexts compile' $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror','-Irelease/include','-Ilib/xrt','release/extlibs/singleinstance/test/test_contexts.c','-Lrelease','-lxl','-o',$context)
    $null=Run 'real contexts' $context @((Join-Path $root 'release'),(Join-Path $root 'release/extlibs/singleinstance/test/context_hold.xl'),(Join-Path $root 'release/extlibs/singleinstance/test/context_probe.xl'),(Join-Path $root 'release/extlibs/singleinstance/test/context_after.xl'),(Join-Path $work 'ready'),(Join-Path $work 'done'))
    $passed=$true
} catch {$failure=$_.Exception.Message;throw} finally {
    Pop-Location
    $changed=@($hashes.Keys|Where-Object {!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $hashes[$_]})
    $changed+=@(Inputs|Where-Object {!$hashes.Contains($_)})
    if($changed.Count){$passed=$false}
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{passed=$passed;failure=$failure;skipAot=[bool]$SkipAot;checks=$checks.ToArray();inputs=$hashes;changedInputs=$changed}|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
    $resolved = [IO.Path]::GetFullPath($work)
    $prefix = $safeRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not ([IO.Path]::GetFileName($resolved)).StartsWith('si_')) {
        throw "Refusing to remove unexpected test directory: $resolved"
    }
    if($passed){Remove-Item -LiteralPath $resolved -Recurse -Force}
    else{Write-Warning "singleinstance failure artifacts: $resolved"}
    Write-Host "[singleinstance] report: $work.json"
}
if(!$passed){throw 'singleinstance gate failed'}
