param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,[switch]$Aggregate)
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $out=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $out){throw 'Preserve mixed dependency evidence'}
    $public=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $contracts=Get-Content (Join-Path $ContractDirectory 'build.json') -Raw|ConvertFrom-Json
    $driver=Join-Path $ContractDirectory 'xlir_code_owner_binding_contract.exe'
    if(!$public.SourceXrt){throw 'Require source runtime'}
    $case=if($Aggregate){'xlir_module_aggregate_dependency'}else{'xlir_module_mixed_dependency'}
    $files=@(Get-Item -LiteralPath $PSCommandPath,(Get-Command gcc).Source,'release/xl.dll',
        'test/cases/xlir_module_mixed_dependency.xl','test/cases/xlir_module_mixed_dependency_host.c',
        'test/cases/xlir_module_mixed_dependency_artifact.c','test/cases/xlir_module_executor_host.c',
        'test/cases/xlir_module_executor_artifact.c','test/cases/xlir_one_shot_tcc_host.c','test/cases/xlir_async_module_lifetime_host.c')
    if($Aggregate){$files+=@(Get-Item -LiteralPath "test/cases/$case.xl","test/cases/${case}_host.c","test/cases/${case}_artifact.c",'tools/check_xlir_aggregate_dependencies.ps1')}
    $inputs=@($public.Inputs)+@($public.Headers)+@($public.Objects)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if(@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count){throw 'Stale mixed dependency inputs'}
    New-Item -ItemType Directory -Path $out,"$out/__xl_runtime"|Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$out/xl.dll"
    foreach($header in $public.Headers|Where-Object Path -match '[/\\]__xl_runtime[/\\]'){
        Copy-Item -LiteralPath $header.Path -Destination (Join-Path "$out/__xl_runtime" ([IO.Path]::GetFileName($header.Path)))
    }
    $emission=@();$deterministic=$true
    foreach($pass in @(1,2)){
        & $driver "test/cases/$case.xl" "$out/emit$pass.c" "$out/emit$pass.h" "$out/emit$pass.json" --script *> "$out/emit$pass.log"
        $code=$LASTEXITCODE;$complete=$false
        if(!$code){
            $ir=Get-Content "$out/emit$pass.json" -Raw|ConvertFrom-Json
            $complete=$ir.asyncMachines.Count-eq1-and$ir.moduleExecutorOwnershipAbi-eq1-and$ir.moduleTaskPoolOwnershipAbi-eq1-and$ir.retirementAbi-eq3-and
                $ir.callbackBoundaries.Count-eq5-and@($ir.callbackBoundaries|Where-Object {$_.transportAbi-eq2-and$_.storageAbi-eq4}).Count-eq4
            if($Aggregate){$complete=$complete-and$ir.futureAggregateAbi-eq1}
        }
        $emission+=[pscustomobject]@{Pass=$pass;Exit=$code;Complete=$complete};$deterministic=$deterministic-and$complete
    }
    if($deterministic){foreach($suffix in @('c','h','json')){
        $deterministic=$deterministic-and(Get-FileHash "$out/emit1.$suffix").Hash-eq(Get-FileHash "$out/emit2.$suffix").Hash
    }}
    $lanes=@();$residentBuilds=@()
    if($deterministic){
        Copy-Item -LiteralPath "$out/emit1.c" -Destination "$out/xlir_module_executor.generated.c"
        Copy-Item -LiteralPath "$out/emit1.h" -Destination "$out/xlir_module_executor.generated.h"
        [IO.File]::WriteAllText("$out/xlir_module_executor.binding.h",("#define MODULE_EXECUTOR_START xlir_async_start_"+$ir.asyncMachines[0].targetSymbol+"`n"),[Text.UTF8Encoding]::new($false))
        $excluded=@('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
        $dependencies=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-notin$excluded}|ForEach-Object Path)
        $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
            '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers',
            '-I.','-Irelease','-Ilib/tcc',"-I$out")
        $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
        $waitSources=@('src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_future_wait.c')
        $waitNames=@($waitSources|ForEach-Object {[IO.Path]::GetFileNameWithoutExtension($_)+'.o'})
        $commonDependencies=@($dependencies|Where-Object {[IO.Path]::GetFileName($_)-notin$waitNames})
        $waitObjects=@{}
        foreach($level in @(0,2)){
            $waitObjects[$level]=@()
            foreach($source in $waitSources){
                $object="$out/$([IO.Path]::GetFileNameWithoutExtension($source))-O$level.o"
                & gcc @flags "-O$level" -DXL_STATIC_EMBED -c $source -o $object *> "$object.log"
                $code=$LASTEXITCODE;$residentBuilds+=[pscustomobject]@{Source=$source;Level=$level;Exit=$code;Object=$object}
                if(!$code){$waitObjects[$level]+=$object}
            }
        }
        foreach($kind in @('tcc','dll')){foreach($level in @(0,2)){
            $name="$kind-O$level";$binary="$out/$name.exe";$dllExit=$null
            $extra=@('-DJOINT_SOURCE_XRT',('-DJOINT_TCC_INCLUDE_DIRECTORY="'+$out.Replace('\','/')+'"'))
            if($kind-eq'dll'){$extra=@('-DMODULE_EXECUTOR_DLL','-DXL_BUILD_DLL',
                ('-DJOINT_DLL_PATH="'+$out.Replace('\','/')+"/mixed-dependency-O$level.dll"+'"'),
                ('-DJOINT_DLL_NAME="'+"mixed-dependency-O$level.dll"+'"'),"-Wl,--out-implib,$out/$name.a",'-Wl,--export-all-symbols')}
            $buildExit=1
            if($waitObjects[$level].Count-eq3){
                $linked=@($commonDependencies)+@($waitObjects[$level])
                & gcc @flags "-O$level" @extra "test/cases/${case}_host.c" @linked release/xl.dll @libs -o $binary *> "$out/$name.build.log"
                $buildExit=$LASTEXITCODE
            }
            $run=$null;$complete=$false;$oom=0;$admissionDeferrals=0;$snapshotDeferrals=0
            if(!$buildExit-and$kind-eq'dll'){
                & gcc @flags "-O$level" -shared -DXL6_BUILD_SHARED "test/cases/${case}_artifact.c" "$out/$name.a" '-Wl,--export-all-symbols' -o "$out/mixed-dependency-O$level.dll" *> "$out/$name.dll.log"
                $dllExit=$LASTEXITCODE
            }
            if(!$buildExit-and($kind-ne'dll'-or$dllExit-eq0)){
                $process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$out/$name.out.log" -RedirectStandardError "$out/$name.err.log"
                if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
                $process.Dispose();$log=[IO.File]::ReadAllText("$out/$name.out.log")
                $match=[regex]::Match($log,'Mixed producer modules: 132 real unloads, 60 rings, 20 feeders, 20 acyclic controls, 112 external-root checks, 40 cleanup-await roots, 548 callback finally executions, 32 snapshot budgets, ([1-9][0-9]*) actual OOM; real task pool, four callback frames per await, original async catch/finally, globals, MEM_FREE and exact memory balance')
                $admission=[regex]::Match($log,'Mixed preflight: 132 generated READY callbacks, ([0-9]+) pre-callback admission deferrals; no BUSY or FAILED callback retry')
                if($Aggregate){
                    $match=[regex]::Match($log,'Aggregate dependency modules: 170 real unloads, six literal/array ALL/RACE/ANY modes, 72 rings, 24 feeders, 24 acyclic controls, 128 external-root checks, 48 cleanup-await roots, 704 callback finally executions, 32 snapshot budgets, ([1-9][0-9]*) actual OOM; original async catch/finally, complete ALL results, ANY index, MEM_FREE and exact memory balance')
                    $admission=[regex]::Match($log,'Aggregate wait proof: 80 actual independently admitted module plans, 160 members, 12 feeders refused, 108 diagnostic-preserving proof budgets, 12 resource failures, 12 real producer BUSY deferrals; zero premature cancellation or semantic cleanup')
                    $snapshot=[regex]::Match($log,'Aggregate snapshot admission: ([0-9]+) exact native-pool quiescence deferrals; every successful plan independently readmits the whole graph')
                    $boundary=$log.Contains('Aggregate descriptor boundary: 172 unprepared-queue refusals and 172 wrong-claim refusals; actual queue preparation required')
                    $nativeFacts=$log.Contains('Aggregate native input facts: 172 exact complete ordered views, duplicate and completed slots, ALL/ANY rules and Race-only loser cancellation; 18 success/failure/cancellation completions after module Close')
                    if($snapshot.Success){$snapshotDeferrals=[int]$snapshot.Groups[1].Value}
                }
                if($match.Success){$oom=[int]$match.Groups[1].Value}
                if($admission.Success-and!$Aggregate){$admissionDeferrals=[int]$admission.Groups[1].Value}
                $complete=$run-eq0-and$match.Success-and$admission.Success
                if($Aggregate){$complete=$complete-and$snapshot.Success-and$boundary-and$nativeFacts}
            }
            $lanes+=[pscustomobject]@{Lane=$name;Build=$buildExit;DllBuild=$dllExit;Run=$run;Complete=$complete;ActualOom=$oom;AdmissionDeferrals=$admissionDeferrals;SnapshotDeferrals=$snapshotDeferrals}
            Write-Output "$name build=$buildExit dll=$dllExit run=$run complete=$complete"
        }}
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    $report=[pscustomobject]@{Inputs=$inputs;InputsUnchanged=$unchanged;Emission=$emission;Deterministic=$deterministic;Lanes=$lanes;ResidentBuilds=$residentBuilds;
        PerLane=[pscustomobject]@{Unloads=132;Rings=60;Feeders=20;Acyclic=20;ExternalRoots=112;CleanupRoots=40;CallbackFinally=548;SnapshotBudgets=32};
        Scope='Real generated module owning executor and native task pool; each await passes through four generated Future callback frames. The shared wait solver and both producer providers are compiled at the host O0/O2 level; unchanged dependencies use the pinned source-build objects. Original catch/finally CFG and genuine TCC/DLL release, observer/native code roots, cleanup-await re-rooting, OOM and exact physical allocation balance. Does not claim arbitrary native/multi-input producer or full platform/release acceptance.'}
    if($Aggregate){
        $report.PerLane=[pscustomobject]@{Unloads=170;Rings=72;Feeders=24;Acyclic=24;ExternalRoots=128;CleanupRoots=48;CallbackFinally=704;SnapshotBudgets=32;
            Modes=6;ProofPlans=80;ProofMembers=160;ProofFeeders=12;ProofBudgets=108;ProofResourceFailures=12;PrepareDeferrals=12;UnpreparedRefusals=172;WrongClaimRefusals=172;
            NativeInputViews=172;CompletionsAfterClose=18}
        $report.Scope='Real generated ALL/RACE/ANY literal and array modules, ordered duplicate inputs and completed ALL slots, with genuine callback/continuation/aggregate wait descriptors, complete independently admitted module plans, shared AND/OR proof, original catch/finally and task pool, external roots, OOM diagnostic preservation and real producer BUSY deferral. Four actual TCC/DLL O0/O2 unload lanes with MEM_FREE and exact memory. Native dependencies use the pinned O0 source build; not whole-language/platform/release acceptance.'
    }
    [IO.File]::WriteAllText("$out/results.json",($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged-or!$deterministic-or$lanes.Count-ne4-or@($lanes|Where-Object {!$_.Complete}).Count-or
        $residentBuilds.Count-ne6-or@($residentBuilds|Where-Object Exit -ne 0).Count){throw 'Mixed dependency failures preserved'}
}finally{Pop-Location}
