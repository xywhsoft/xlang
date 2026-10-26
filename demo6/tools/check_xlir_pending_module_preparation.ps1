param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [ValidateRange(0,7)][int[]]$Modes=@(0..7),[ValidateSet('O0','O2')][string[]]$Levels=@('O0','O2'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve pending module preparation evidence'}
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path $ContractDirectory 'build.json') -Raw | ConvertFrom-Json
    $driver=Join-Path $ContractDirectory 'xlir_code_owner_binding_contract.exe'
    if(!$build.SourceXrt){throw 'Current source runtime required'}
    $files=@(Get-Item -LiteralPath $PSCommandPath,(Get-Command gcc).Source,
        'test/cases/xlir_pending_module_preparation.xl','test/cases/xlir_pending_module_preparation_artifact.c',
        'test/cases/xlir_pending_module_preparation_tcc_host.c','test/cases/xlir_one_shot_tcc_host.c',
        'test/cases/xlir_async_module_lifetime_host.c','release/xl.dll')
    $inputs=@($build.Inputs)+@($build.Headers)+@($build.Objects)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if(@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count){throw 'Stale pending module preparation inputs'}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $headers=New-Item -ItemType Directory -Path "$outputPath/__xl_runtime"
    foreach($header in $build.Headers){Copy-Item -LiteralPath $header.Path -Destination (Join-Path $headers ([IO.Path]::GetFileName($header.Path)))}
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    $prefix="$outputPath/xlir_pending_module_preparation";$emission=@()
    foreach($pass in @('','repeat.')){
        & $driver test/cases/xlir_pending_module_preparation.xl "$prefix.${pass}c" "$prefix.${pass}h" "$prefix.${pass}json" --script *> "$outputPath/emit.${pass}log"
        $emission+=[pscustomobject]@{Pass=$(if($pass){'repeat'}else{'generated'});Exit=$LASTEXITCODE}
    }
    $deterministic=@($emission | Where-Object Exit -ne 0).Count -eq 0
    if($deterministic){foreach($ext in @('c','h','json')){
        if((Get-FileHash "$prefix.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash){$deterministic=$false}
    }}
    $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers',"-I$outputPath",'-I.','-Irelease','-Irelease/include','-Ilib/tcc',
        '-DJOINT_SOURCE_XRT','-DXL_ENABLE_EXE_COMPILER=1',(New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $outputPath.Replace('\','/') -RawArgument))
    $excluded=@('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
    $dependencies=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -notin $excluded} | ForEach-Object Path)
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$rows=@()
    foreach($level in $Levels){
        $binary="$outputPath/tcc-host-$level.exe";$buildExit=$null
        if($deterministic){
            $buildExit=Invoke-XlirGccToLog ($flags+@("-$level",'test/cases/xlir_pending_module_preparation_tcc_host.c')+$dependencies+@('release/xl.dll')+$libs+@('-o',$binary)) "$outputPath/tcc-host-$level.build.log"
        }
        foreach($mode in $Modes){
            $run=$null;$complete=$false;$log="$outputPath/tcc-host-$level-mode$mode"
            if($null -ne $buildExit -and $buildExit -eq 0){
$process=Start-Process -FilePath $binary -ArgumentList $mode -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$log.out.log" -RedirectStandardError "$log.err.log"
                $null = $process.Handle
                if(!$process.WaitForExit(30000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
                $process.Dispose()
                $deferred=if($mode){25}else{0}
                $marker="^Pending module mode=${mode}: 25 real TCC lifetimes, 50 ordered finally callbacks before 25 class finalizers, 75 unlocked/unfrozen code-residency probes, $deferred real-reference deferrals, MEM_FREE and zero live delta; no guard leases or fake roots$"
                $complete=$run -eq 0 -and [bool](Select-String -LiteralPath "$log.out.log" -Pattern $marker -Quiet)
                $graphNotifications=if($mode -lt 4){50}else{0};$graphFinalizers=if($mode -in @(0,1,3)){25}else{0}
                $phaseMarker="^Pending module phases mode=${mode}: $graphNotifications certified preparation notifications, $graphFinalizers graph finalizers$"
                $complete=$complete -and [bool](Select-String -LiteralPath "$log.out.log" -Pattern $phaseMarker -Quiet)
            }
            $rows+=[pscustomobject]@{Lane="tcc-host-$level-mode$mode";Build=$buildExit;Run=$run;Complete=$complete}
            Write-Output "tcc-host-$level-mode$mode build=$buildExit run=$run complete=$complete"
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    [IO.File]::WriteAllText("$outputPath/results.json",([pscustomobject]@{Inputs=$inputs;InputsUnchanged=$unchanged;Deterministic=$deterministic;Emission=$emission;Modes=$Modes;Levels=$Levels;Lanes=$rows;
        PerProgram=@{Lifetimes=25;Notifications=50;Finalizers=25;BoundaryProbes=75};HostPhaseProof=$true;
        Scope='Mixed generated/native module with an actual module-owned Promise slot, composed immutable Trace/Finish/Prepare contracts, actual source host and TCC relocation/deletion. Rootless preparation, external result and producer roots, callback resurrection, four terminal states and post-unmap result access. Not arbitrary native Job, full async graph or closed producer SCC acceptance. Original gates remain mandatory.'} | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or !$deterministic -or @($rows | Where-Object {!$_.Complete}).Count){throw 'Pending module preparation failures collected'}
} finally {Pop-Location}
