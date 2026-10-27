param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [int[]]$Modes=@(0..17),[string[]]$Levels=@('O0','O2'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve module preparation evidence'}
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path $ContractDirectory 'build.json') -Raw | ConvertFrom-Json
    $driver=Join-Path $ContractDirectory 'xlir_code_owner_binding_contract.exe'
    if(!$build.SourceXrt){throw 'Current source runtime required'}
    $files=@(Get-Item -LiteralPath $PSCommandPath,(Get-Command gcc).Source,
        'test/cases/xlir_module_prepare.xl','test/cases/xlir_module_prepare_artifact.c','test/cases/xlir_module_prepare_legacy_artifact.c','test/cases/xlir_module_prepare_tcc_host.c',
        'test/cases/xlir_future_retirement_artifact.c','test/cases/xlir_one_shot_tcc_host.c',
        'test/cases/xlir_async_module_lifetime_host.c','release/xl.dll')
    $inputs=@($build.Inputs)+@($build.Headers)+@($build.Objects)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if(@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count){throw 'Stale module preparation inputs'}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $headers=New-Item -ItemType Directory -Path "$outputPath/__xl_runtime"
    foreach($header in $build.Headers){Copy-Item -LiteralPath $header.Path -Destination (Join-Path $headers ([IO.Path]::GetFileName($header.Path)))}
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    $prefix="$outputPath/xlir_future_retirement"
    foreach($pass in @('generated','repeat')){
        & $driver test/cases/xlir_module_prepare.xl "$prefix.$pass.c" "$prefix.$pass.h" "$prefix.$pass.json" --script *> "$outputPath/emit.$pass.log"
        if($LASTEXITCODE){throw 'Module preparation generation failed'}
    }
    foreach($ext in @('c','h','json')){if((Get-FileHash "$prefix.generated.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash){throw 'Nondeterministic module preparation emission'}}
    $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers',"-I$outputPath",'-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Itest/cases',
        '-DJOINT_SOURCE_XRT','-DXL_ENABLE_EXE_COMPILER=1',(New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $outputPath.Replace('\','/') -RawArgument))
    $excluded=@('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
    $dependencies=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -notin $excluded} | ForEach-Object Path)
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$rows=@()
    foreach($level in $Levels){
        if($level -notin @('O0','O2')){throw 'Unknown compiler level'}
        foreach($version in @(2,3)){
        $laneName=if($version-eq2){"tcc-host-$level"}else{"tcc-owned-host-$level"}
        $versionFlags=@();if($version-eq2){$versionFlags=@('-DMODULE_PREPARE_LEGACY_V2')}
        $artifactSource=if($version-eq2){'test/cases/xlir_module_prepare_legacy_artifact.c'}else{'test/cases/xlir_module_prepare_artifact.c'}
        $versionFlags+=@(New-XlirCStringDefine 'JOINT_TCC_SOURCE_FILE' ([IO.Path]::GetFullPath($artifactSource).Replace('\','/')) -RawArgument)
        $binary="$outputPath/$laneName.exe"
        $buildExit=Invoke-XlirGccToLog ($flags+$versionFlags+@("-$level",'test/cases/xlir_module_prepare_tcc_host.c')+$dependencies+@('release/xl.dll')+$libs+@('-o',$binary)) "$outputPath/$laneName.build.log"
        foreach($mode in $Modes){
            if($mode -lt 0 -or $mode -gt 17){throw 'Unknown preparation mode'}
            $run=$null;$complete=$false;$log="$outputPath/$laneName-mode$mode"
            if(!$buildExit){
$process=Start-Process -FilePath $binary -ArgumentList $mode -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$log.out.log" -RedirectStandardError "$log.err.log"
                $null = $process.Handle
                if(!$process.WaitForExit(30000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
                $process.Dispose()
                $marker=if($version-eq2){"^Module prepare mode=${mode}: 25 real TCC lifetimes, 75 internal finalizers, [0-9]+ unlocked/unfrozen probes; automatic resource drain, aliases and zero live delta$"}
                    else{"^Module preflight V3 mode=${mode}: 25 real TCC lifetimes, 75 internal finalizers, [0-9]+ unlocked/unfrozen probes; graph-owned pool, aliases and zero live delta$"}
                $complete=$run -eq 0 -and [bool](Select-String -LiteralPath "$log.out.log" -Pattern $marker -Quiet)
            }
            $rows+=[pscustomobject]@{Lane="$laneName-mode$mode";Abi=$version;Build=$buildExit;Run=$run;Complete=$complete}
            Write-Output "$laneName-mode$mode build=$buildExit run=$run complete=$complete"
        }
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    [IO.File]::WriteAllText("$outputPath/results.json",([pscustomobject]@{Inputs=$inputs;InputsUnchanged=$unchanged;Deterministic=$true;Modes=$Modes;Levels=$Levels;Lanes=$rows;
        Scope='All original 18 modes and both host optimization levels retained for an explicitly adapted legacy V2 artifact with real task-pool drain. The same complete population is additionally run against a test-only V3 adapter of current V4 output, which leaves graph-owned services open at preflight. The fixture now creates both an actual blocking task pool and a separate ready-entry executor. Six Future aliases, BUSY and diagnostic-independent failure/retry, ambient-error precedence, cross-thread lock/freeze probes, reentrant exclusion, malformed V2/V3 pre-callback refusal, inherited mutation deferral and V1 compatibility. Legacy adaptation is test-only, not a Cgen fallback.'} | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or @($rows | Where-Object {!$_.Complete}).Count){throw 'Module preparation failures collected'}
} finally {Pop-Location}
