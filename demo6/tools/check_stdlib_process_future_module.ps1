param([Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [switch]$GlobalNativeHandles)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. "$root/tools/extlib_smoke_process.ps1"
Push-Location $root
try {
    $public=(Resolve-Path -LiteralPath $PublicSourceDirectory).Path.Replace('\','/')
    $work=[IO.Path]::GetFullPath($Directory).Replace('\','/')
    if(Test-Path -LiteralPath $work){throw 'Preserve prior Process Future module evidence'}
    $build=Get-Content "$public/build.json" -Raw|ConvertFrom-Json
    if($build.RuntimeOnly){throw 'Full current-source compiler required'}
    foreach($row in $build.Inputs){if((Get-FileHash $row.Path).Hash-ne$row.Sha256){throw 'Source build changed'}}
    if((Get-FileHash release/xl.dll).Hash-ne$build.RuntimeSha256){throw 'Runtime provider changed'}
    $fixture=if($GlobalNativeHandles){'test/cases/xlir_process_future_module_globals.xl'}else{'test/cases/xlir_process_future_module_lifetime.xl'}
    $paths=@($build.Inputs.Path)+@($build.Headers.Path)+@($build.Objects.Path)+@($fixture,$PSCommandPath,
        'tools/extlib_smoke_process.ps1','test/cases/xlir_process_future_module_lifetime.xl',
        'test/cases/xlir_process_future_module_host.c','test/cases/xlir_process_future_module_artifact.c',
        'test/cases/xlir_process_child.c','test/support/process_oom_observer.c','test/support/process_module_tcc_provider.h',
        'test/cases/xlir_code_owner_binding_contract.c','test/cases/xlir_owner_consumption_contract.c',
        'release/xl.dll',"$public/build.json",(Get-Command gcc).Source)
    $inputs=[ordered]@{}
    foreach($path in @($paths|ForEach-Object {(Resolve-Path -LiteralPath $_).Path}|Sort-Object -Unique)){
        $inputs[$path]=(Get-FileHash $path).Hash
    }
    New-Item -ItemType Directory -Path $work|Out-Null
    $rows=[Collections.Generic.List[object]]::new();$utf8=[Text.UTF8Encoding]::new($false)
    $child="$work/child.exe"; $complete=$false
    function Check($name,$program,[string[]]$arguments){
        $result=Invoke-ExtlibSmokeProcess $program $arguments $root 120 @{XL6_PROCESS_CHILD=$child}
        $rows.Add([ordered]@{Name=$name;Program=$program;Arguments=$arguments;Result=$result})
        [IO.File]::WriteAllText("$work/$name.log",$result.stdout+$result.stderr,$utf8)
        if($result.status-ne'pass'){throw "$name failed: $($result.stderr) $($result.stdout)"}
        return $result.stdout
    }
    try {
        $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
            '-Wno-unused-parameter','-Wno-unused-label','-Wno-pointer-sign','-Wno-format-truncation',
            '-Wno-missing-field-initializers',"-I$public/include",'-I.','-Irelease','-Ilib/tcc')
        $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
        $compiler=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
        $null=Check 'emitter-build' gcc ($flags+@('-DXL_STATIC_EMBED','test/cases/xlir_code_owner_binding_contract.c')+
            $compiler+@('release/xl.dll')+$libs+@('-o',"$work/emitter.exe"))
        Copy-Item release/xl.dll "$work/xl.dll"
        $contract=Check 'native-ir-contract' "$work/emitter.exe" @('--native-handles')
        if($contract-notmatch 'Native resource policy: 3 typed fields, 3 lowered declarations, 29 rejected mutations before C/header output'){throw 'Native typed/lowered mutation witness missing'}
        foreach($pass in @('generated','repeat')){
            $null=Check "$pass-emit" "$work/emitter.exe" @($fixture,
                "$work/$pass.c","$work/$pass.h","$work/$pass.json",'--script')
        }
        foreach($ext in @('c','h','json')){
            if((Get-FileHash "$work/generated.$ext").Hash-ne(Get-FileHash "$work/repeat.$ext").Hash){throw "Nondeterministic $ext"}
        }
        $ir=Get-Content "$work/generated.json" -Raw|ConvertFrom-Json
        $machine=@($ir.asyncMachines|Where-Object {
            $declaration=$ir.functions[$_.function].declaration
            @($ir.nodes|Where-Object {$_.id-eq$declaration-and$_.parent-eq1}).Count-eq1
        })
        $factory=@($ir.functions|Where-Object {$_.kindName-eq'user'-and$_.parameterCount-eq0})
        if($machine.Count-ne1-or$ir.asyncMachines.Count-ne2){throw 'Canonical Process Future entries missing'}
        $binding="#define PROCESS_RUN xlir_async_start_$($machine[0].targetSymbol)`n"
        if($GlobalNativeHandles){
            if($factory.Count-ne0){throw 'Global native fixture must not use a scoped factory'}
            $binding+="#define PROCESS_GLOBAL_NATIVE 1`n"
        }else{
            if($factory.Count-ne1){throw 'Scoped factory missing'}
            $binding+="#define PROCESS_CREATE xlir_fn_$($factory[0].symbol)`n"
        }
        [IO.File]::WriteAllText("$work/binding.h",$binding,$utf8)
        $policies=@($ir.callTargets|Where-Object {$_.nativeHandlePolicyAbi-eq1-and$_.reachability-ne'unreachable'})
        if($policies.Count-ne3-or@($policies.nativeHandlePolicyCName|Sort-Object -Unique).Count-ne3){throw 'Verified native policies missing'}
        $generated=Get-Content "$work/generated.c" -Raw
        foreach($policy in $policies){
            if($generated-notmatch [regex]::Escape("xl6NativeHandleTakeV1(handle, $($policy.nativeHandlePolicyCName)())")-or
                $generated-match "xlir_native_handle_drop_s$($policy.symbol)\("){throw 'Module-local native destructor escaped resident policy'}
        }
        $null=Check 'child-build' gcc @('-std=c11','-Wall','-Wextra','-Werror','-municode',
            'test/cases/xlir_process_child.c','-o',$child)
        $dll="$work/generated.dll".Replace('\','/')
        $null=Check 'host-build' gcc ($flags+@('-O2','-fwhole-program',"-DPROCESS_DLL_PATH=`"$dll`"",
            '-DPROCESS_DLL_NAME="generated.dll"','-c','test/cases/xlir_process_future_module_host.c','-o',"$work/host.o"))
        $null=Check 'observer-build' gcc ($flags+@('-O0','-include','tools/xrt_single_header_impl.c','-c',
            'test/support/process_oom_observer.c','-o',"$work/observer.o"))
        $native=@($build.Sources|Where-Object {$_-like'src/runtime/*'-or$_-eq'src/api/xl_task_group_bridge.c'}|
            ForEach-Object {Join-Path $public ([IO.Path]::GetFileNameWithoutExtension($_)+'.o')})
        $null=Check 'host-link' gcc ($flags+@("$work/host.o","$work/observer.o")+$native+@('release/xl.dll',
            '-Wl,--wrap=xrtProcessSpawn',"-Wl,--out-implib,$work/host.a",'-Wl,--export-all-symbols')+$libs+@('-o',"$work/host.exe"))
        $null=Check 'artifact-build' gcc ($flags+@('-O2','-shared','-DXL6_BUILD_SHARED',
            "-DPROCESS_GENERATED_C=`"$work/generated.c`"","-DPROCESS_GENERATED_H=`"$work/generated.h`"",
            "-DPROCESS_BINDING_H=`"$work/binding.h`"",'test/cases/xlir_process_future_module_artifact.c',
            "$work/host.a",'release/xl.dll','-o',$dll))
        foreach($mode in 0..1){
            $witness=Check "dll-mode-$mode" "$work/host.exe" @("$mode")
            if($witness-notmatch "Process Future provider DLL/FreeLibrary mode=$mode`: 25 real unloads; pending/cancel isolation, independent statuses, escaped class pins, finally twice, native waiter and heap balanced"){
                throw 'Real provider/unload/ownership witness missing'
            }
            if($witness-notmatch 'Resident native handle: 3 pre-trace refusals, invalid contract keeps slot, [1-9][0-9]* OOM prefix failures \+ first no-hit, exact ledger'){throw 'Native admission/OOM witness missing'}
        }
        $tccFlags=@('-DPROCESS_TCC_PROVIDER',"-DPROCESS_INCLUDE_DIRECTORY=`"$public/include`"",
            "-DPROCESS_GENERATED_C_VALUE=`"\`"$work/generated.c\`"`"",
            "-DPROCESS_GENERATED_H_VALUE=`"\`"$work/generated.h\`"`"",
            "-DPROCESS_BINDING_H_VALUE=`"\`"$work/binding.h\`"`"")
        $null=Check 'tcc-host-build' gcc ($flags+@('-O2','-fwhole-program')+$tccFlags+@('-c',
            'test/cases/xlir_process_future_module_host.c','-o',"$work/tcc-host.o"))
        $null=Check 'tcc-host-link' gcc ($flags+@("$work/tcc-host.o","$work/observer.o")+$native+
            @('release/xl.dll','-Wl,--wrap=xrtProcessSpawn','-Wl,--export-all-symbols')+$libs+@('-o',"$work/tcc-host.exe"))
        foreach($mode in 0..1){
            $witness=Check "tcc-mode-$mode" "$work/tcc-host.exe" @("$mode")
            if($witness-notmatch "Process Future provider TCC/MEM_FREE mode=$mode`: 25 real unloads; pending/cancel isolation, independent statuses, escaped class pins, finally twice, native waiter and heap balanced"){
                throw 'Real TCC/page/ownership witness missing'
            }
        }
        $complete=$true
    } finally {
        $changed=@($inputs.Keys|Where-Object {(Get-FileHash $_).Hash-ne$inputs[$_]})
        if($changed.Count){$complete=$false}
        [IO.File]::WriteAllText("$work/results.json",([ordered]@{Complete=$complete;Checks=$rows.Count;
            Scope='Windows actual DLL and TCC Process Future source API retirement; resident native policy admission/rollback and raw status handle survives real unmapping';
            GlobalNativeHandles=[bool]$GlobalNativeHandles;
            ChangedInputs=$changed;Inputs=$inputs;Rows=$rows}|ConvertTo-Json -Depth 12),$utf8)
        if($changed.Count){throw 'Process Future module inputs changed'}
    }
} finally {Pop-Location}
