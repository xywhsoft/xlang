param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior module-executor evidence'}
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $contracts=Get-Content (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw|ConvertFrom-Json
    if(!$build.SourceXrt -or @($build.Objects|Where-Object Path -match 'xrt_single_header_impl.o$').Count-ne1){throw 'Require current source runtime'}
    $files=@(Get-Item -LiteralPath $PSCommandPath,(Get-Command gcc).Source,'release/xl.dll',
        'test/cases/xlir_module_executor.xl','test/cases/xlir_module_executor_contract.c',
        'test/cases/xlir_module_executor_artifact.c','test/cases/xlir_module_executor_host.c',
        'test/cases/xlir_module_executor_mixed.xl','test/cases/xlir_module_executor_mixed_host.c',
        'test/cases/xlir_continuation_owner_test.h','test/cases/xlir_module_ownership.xl',
        'test/cases/xlir_one_shot_tcc_host.c','test/cases/xlir_async_module_lifetime_host.c')
    $inputs=@($build.Inputs)+@($build.Headers)+@($build.Objects)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if(@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count){throw 'Stale module-executor inputs'}
    New-Item -ItemType Directory -Path $outputPath|Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    $runtimeInclude=Join-Path $outputPath '__xl_runtime'
    New-Item -ItemType Directory -Path $runtimeInclude|Out-Null
    foreach($header in $build.Headers|Where-Object Path -match '[/\\]__xl_runtime[/\\]'){
        Copy-Item -LiteralPath $header.Path -Destination (Join-Path $runtimeInclude ([IO.Path]::GetFileName($header.Path)))
    }
    $emission=@()
    foreach($pass in @(1,2)){
        $prefix="$outputPath/emit$pass"
        & $ContractDriver "$prefix.c" "$prefix.h" "$prefix.binding.h" "$prefix.json" "$prefix.txt" *> "$prefix.log"
        $code=$LASTEXITCODE
        $complete=$code-eq0 -and [bool](Select-String -LiteralPath "$prefix.log" -Pattern '^Module executor IR: 29 C/header-gated mutations,' -Quiet)
        $emission+=[pscustomobject]@{Pass=$pass;Exit=$code;Complete=$complete}
    }
    $deterministic=@($emission|Where-Object {!$_.Complete}).Count-eq0
    if($deterministic){foreach($suffix in @('c','h','binding.h','json','txt')){
        $deterministic=$deterministic -and (Get-FileHash "$outputPath/emit1.$suffix").Hash-eq(Get-FileHash "$outputPath/emit2.$suffix").Hash
    }}
    $rows=@()
    if($deterministic){
        Copy-Item -LiteralPath "$outputPath/emit1.c" -Destination "$outputPath/xlir_module_executor.generated.c"
        Copy-Item -LiteralPath "$outputPath/emit1.h" -Destination "$outputPath/xlir_module_executor.generated.h"
        Copy-Item -LiteralPath "$outputPath/emit1.binding.h" -Destination "$outputPath/xlir_module_executor.binding.h"
        $excluded=@('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
        $dependencies=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-notin$excluded}|ForEach-Object Path)
        $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
            '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers',
            '-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Itest/cases',"-I$outputPath")
        $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
        foreach($kind in @('tcc','dll')){foreach($level in @(0,2)){
            $name="$kind-O$level";$binary="$outputPath/$name.exe";$dllExit=$null
            $extra=@('-DJOINT_SOURCE_XRT',(New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $outputPath.Replace('\','/') -RawArgument))
            if($kind-eq'dll'){$extra=@('-DMODULE_EXECUTOR_DLL','-DXL_BUILD_DLL',
                (New-XlirCStringDefine 'JOINT_DLL_PATH' ($outputPath.Replace('\','/')+"/executor-O$level.dll") -RawArgument),
                (New-XlirCStringDefine 'JOINT_DLL_NAME' "executor-O$level.dll" -RawArgument),"-Wl,--out-implib,$outputPath/$name.a",'-Wl,--export-all-symbols')}
            $buildExit=Invoke-XlirGccToLog ($flags+@("-O$level")+$extra+@('test/cases/xlir_module_executor_host.c')+$dependencies+@('release/xl.dll')+$libs+@('-o',$binary)) "$outputPath/$name.build.log"
            $run=$null;$complete=$false;$actualOom=0
            if(!$buildExit -and $kind-eq'dll'){
                $dllExit=Invoke-XlirGccToLog ($flags+@("-O$level",'-shared','-DXL6_BUILD_SHARED','test/cases/xlir_module_executor_artifact.c',"$outputPath/$name.a",'-Wl,--export-all-symbols','-o',"$outputPath/executor-O$level.dll")) "$outputPath/$name.dll.log"
            }
            if(!$buildExit -and ($kind-ne'dll' -or $dllExit-eq0)){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$outputPath/$name.out.log" -RedirectStandardError "$outputPath/$name.err.log"
                $null = $process.Handle
                if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
                $process.Dispose()
                $text=Get-Content "$outputPath/$name.out.log" -Raw
                if($text-match '32 snapshot budgets, ([1-9][0-9]*) actual OOM'){$actualOom=[int]$Matches[1]}
                $complete=$run-eq0 -and $actualOom-gt0 -and $text-match '^([\s\S]*\n)?Generated module executor: 182 real unloads, 60 rings, 20 feeders, 20 acyclic controls, 112 external-root checks, 40 cleanup-await roots, 30 malformed-contract refusals, 5 legacy guarded teardowns, 15 readiness retries, 32 snapshot budgets, [1-9][0-9]* actual OOM; original module executor, catch/finally/globals, MEM_FREE and exact memory balance' -and $text.Contains('Strict V4 admission: 30 malformed contracts rejected before activation; real images retired with MEM_FREE and full allocation balance')
            }
            $rows+=[pscustomobject]@{Lane=$name;Build=$buildExit;DllBuild=$dllExit;Run=$run;Complete=$complete;ActualOom=$actualOom}
            Write-Output "$name build=$buildExit dll=$dllExit run=$run complete=$complete"
        }}
    }
    if($deterministic){
        $mixed="$outputPath/mixed"
        New-Item -ItemType Directory -Path $mixed,"$mixed/__xl_runtime"|Out-Null
        foreach($header in $build.Headers|Where-Object Path -match '[/\\]__xl_runtime[/\\]'){
            Copy-Item -LiteralPath $header.Path -Destination (Join-Path "$mixed/__xl_runtime" ([IO.Path]::GetFileName($header.Path)))
        }
        $sourceDriver=Join-Path (Split-Path -Parent $ContractDriver) 'xlir_code_owner_binding_contract.exe'
        foreach($pass in @(1,2)){
            & $sourceDriver test/cases/xlir_module_executor_mixed.xl "$mixed/emit$pass.c" "$mixed/emit$pass.h" "$mixed/emit$pass.json" --script *> "$mixed/emit$pass.log"
            $code=$LASTEXITCODE;$complete=$false
            if(!$code){
                $ir=Get-Content "$mixed/emit$pass.json" -Raw|ConvertFrom-Json
                $complete=$ir.asyncMachines.Count-eq1 -and $ir.moduleExecutorOwnershipAbi-eq1 -and $ir.moduleTaskPoolOwnershipAbi-eq1 -and $ir.retirementAbi-eq4 -and $ir.moduleTaskPoolAbi-eq'xrt-guarded-quiesce-before-globals'
            }
            $emission+=[pscustomobject]@{Pass="mixed$pass";Exit=$code;Complete=$complete}
            $deterministic=$deterministic -and $complete
        }
        if($deterministic){foreach($suffix in @('c','h','json')){
            $deterministic=$deterministic -and (Get-FileHash "$mixed/emit1.$suffix").Hash-eq(Get-FileHash "$mixed/emit2.$suffix").Hash
        }}
        if($deterministic){
            Copy-Item -LiteralPath "$mixed/emit1.c" -Destination "$mixed/xlir_module_executor.generated.c"
            Copy-Item -LiteralPath "$mixed/emit1.h" -Destination "$mixed/xlir_module_executor.generated.h"
            [IO.File]::WriteAllText("$mixed/xlir_module_executor.binding.h",("#define MODULE_EXECUTOR_START xlir_async_start_"+$ir.asyncMachines[0].targetSymbol+"`n"),[Text.UTF8Encoding]::new($false))
            $mixedFlags=@("-I$mixed")+$flags
            foreach($kind in @('tcc','dll')){foreach($level in @(0,2)){
                $name="mixed-$kind-O$level";$binary="$outputPath/$name.exe";$dllExit=$null
                $extra=@('-DJOINT_SOURCE_XRT',(New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $mixed.Replace('\','/') -RawArgument))
                if($kind-eq'dll'){$extra=@('-DMODULE_EXECUTOR_DLL','-DXL_BUILD_DLL',
                    (New-XlirCStringDefine 'JOINT_DLL_PATH' ($mixed.Replace('\','/')+"/mixed-executor-O$level.dll") -RawArgument),
                    (New-XlirCStringDefine 'JOINT_DLL_NAME' "mixed-executor-O$level.dll" -RawArgument),"-Wl,--out-implib,$outputPath/$name.a",'-Wl,--export-all-symbols')}
                $buildExit=Invoke-XlirGccToLog ($mixedFlags+@("-O$level")+$extra+@('test/cases/xlir_module_executor_mixed_host.c')+$dependencies+@('release/xl.dll')+$libs+@('-o',$binary)) "$outputPath/$name.build.log"
                $run=$null;$complete=$false
                if(!$buildExit -and $kind-eq'dll'){
                    $dllExit=Invoke-XlirGccToLog ($mixedFlags+@("-O$level",'-shared','-DXL6_BUILD_SHARED','test/cases/xlir_module_executor_artifact.c',"$outputPath/$name.a",'-Wl,--export-all-symbols','-o',"$mixed/mixed-executor-O$level.dll")) "$outputPath/$name.dll.log"
                }
                if(!$buildExit -and ($kind-ne'dll' -or $dllExit-eq0)){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$outputPath/$name.out.log" -RedirectStandardError "$outputPath/$name.err.log"
                    $null = $process.Handle
                    if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
                    $process.Dispose()
                    $complete=$run-eq0 -and [bool](Select-String -LiteralPath "$outputPath/$name.out.log" -SimpleMatch -Pattern 'Mixed module services: 50 real unloads, 25 open-owner await-to-task completions, 25 closed-owner fresh-task refusals, READY preflight without pool mutation, normal finally/globals, MEM_FREE and exact memory balance' -Quiet)
                }
                $rows+=[pscustomobject]@{Lane=$name;Build=$buildExit;DllBuild=$dllExit;Run=$run;Complete=$complete}
                Write-Output "$name build=$buildExit dll=$dllExit run=$run complete=$complete"
            }}
        }
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    $report=[pscustomobject]@{Inputs=$inputs;InputsUnchanged=$unchanged;Deterministic=$deterministic;Emission=$emission;Lanes=$rows;
        PerProgram=[pscustomobject]@{RealUnloads=182;Rings=60;Feeders=20;Acyclic=20;ExternalRoots=112;CleanupRoots=40;MalformedContracts=30;LegacyTeardowns=5;ReadinessRetries=15;AllocationBudgets=32};
        MixedPerProgram=[pscustomobject]@{RealUnloads=50;OpenOwnerCompletions=25;ClosedOwnerRefusals=25};
        Scope='Real generated-module-owned executor, explicit IR ownership and retryable-service V4 contract, current source API/runtime, actual TCC and DLL MEM_FREE. Test-only canonical machine entry, not new public Future parameter export support. All original gates remain mandatory. Network and product-wide release acceptance remain independent.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or !$deterministic -or $rows.Count-ne8 -or @($rows|Where-Object {!$_.Complete}).Count){throw 'Module-executor batch collected failures'}
} finally {Pop-Location}
