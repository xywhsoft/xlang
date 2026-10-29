param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,[string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
. (Join-Path $PSScriptRoot 'xlir_resident_closure.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve Future retirement evidence'}
    $build=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    $driver=Join-Path $ContractDirectory 'xlir_code_owner_binding_contract.exe'
    if(!$build.SourceXrt -or $runtime.Count -ne 1){throw 'Current source runtime required'}
    $files=@(Get-Item -LiteralPath $PSCommandPath,(Join-Path $PSScriptRoot 'xlir_native_process.ps1'),
        $driver,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,
        'test/cases/xlir_future_plan_runtime.c','test/cases/xlir_class_callable_plan_runtime.c',
        'test/cases/xlir_future_retirement.xl','test/cases/xlir_future_retirement_artifact.c','test/cases/xlir_future_retirement_tcc_host.c',
        'test/cases/xlir_one_shot_tcc_host.c','release/xl.dll')
    $inputs=@($build.Inputs)+@($build.Headers)+@($build.Objects)+@($files | ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    if(@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count){throw 'Stale Future inputs'}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $headers=New-Item -ItemType Directory -Path "$outputPath/__xl_runtime"
    Copy-Item -LiteralPath release/include/xruntime_decl.h -Destination "$headers/xruntime_decl.h"
    foreach($header in $build.Headers){Copy-Item -LiteralPath $header.Path -Destination (Join-Path $headers ([IO.Path]::GetFileName($header.Path)))}
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    $prefix="$outputPath/xlir_future_retirement"
    foreach($pass in @('generated','repeat')){
        & $driver test/cases/xlir_future_retirement.xl "$prefix.$pass.c" "$prefix.$pass.h" "$prefix.$pass.json" --script *> "$outputPath/emit.$pass.log"
        if($LASTEXITCODE){throw 'Future retirement generation failed'}
    }
    foreach($ext in @('c','h','json')){if((Get-FileHash "$prefix.generated.$ext").Hash -ne (Get-FileHash "$prefix.repeat.$ext").Hash){throw 'Future retirement emission changed'}}
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$rows=@()
    # Close the actual artifact registry from the fixture's physical roots.
    # Future payloads include generator/array and V2 capture policies even when
    # a selected case does not instantiate each policy. Never guess their link
    # dependencies or compile FOREIGN_ERROR twice with the native composite.
    $closure=Get-XlirResidentClosure -Root $rootPath -Directory "$outputPath/resident-closure" -Sources @('src/runtime/xl6_closure.c','src/runtime/xl6_cell.c',
        'src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_ownership_plan.c',
        'src/runtime/xl6_future_payload.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c',
        'src/runtime/xl6_generator.c') -NativeProvider 'tools/xrt_single_header_impl.c'
    $inputs+=@($closure.Inputs)
    $sources=@('test/cases/xlir_future_plan_runtime.c')+@($closure.Sources)
    foreach($lane in @('gcc-O0','gcc-O2','tcc')){
        $binary="$outputPath/plan-$lane.exe"
        if($lane -eq 'tcc'){
            $arguments=@('-std=c11','-Wall','-Werror','-I.','-Irelease','-Irelease/include')+$sources+
                @('tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll')+$libs+@('-o',$binary)
            $buildExit=Invoke-XlirNativeToLog $Tcc $arguments "$outputPath/plan-$lane.build.log"
        } else {
            $arguments=@('-std=c11','-Wall','-Wextra','-Werror',"-$($lane.Substring(4))",'-I.','-Irelease','-Irelease/include')+$sources+
                @($runtime[0].Path)+$libs+@('-o',$binary)
            $buildExit=Invoke-XlirGccToLog $arguments "$outputPath/plan-$lane.build.log"
        }
        $run=$null;$complete=$false
        if(!$buildExit){
            $run=Invoke-XlirNativeToLog $binary @() "$outputPath/plan-$lane.run.log"
            $complete=$run -eq 0 -and [bool](Select-String -LiteralPath "$outputPath/plan-$lane.run.log" -Pattern '^Future plans: 225 physical graphs, 289 once-only finalizers, 189 producer closes, 50 pre-trace observer refusals, 64 OOM probes with [1-9][0-9]* real failures; .*zero live delta$' -Quiet)
        }
        $rows+=[pscustomobject]@{Lane="plan-$lane";Build=$buildExit;Run=$run;Complete=$complete}
        Write-Output "plan-$lane build=$buildExit run=$run complete=$complete"
    }
    $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-pointer-sign','-Wno-format-truncation','-Wno-missing-field-initializers',"-I$outputPath",'-I.','-Irelease','-Irelease/include','-Ilib/tcc',
        '-DJOINT_SOURCE_XRT','-DXL_ENABLE_EXE_COMPILER=1',(New-XlirCStringDefine 'JOINT_TCC_INCLUDE_DIRECTORY' $outputPath.Replace('\','/') -RawArgument))
    $excluded=@('xl_api.o','xl_api_path.o','xl6_console.o','xlir_public_source_driver.o')
    $dependencies=@($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -notin $excluded} | ForEach-Object Path)
    foreach($level in @('O0','O2')){
        $binary="$outputPath/tcc-host-$level.exe"
        $arguments=$flags+@("-$level",'test/cases/xlir_future_retirement_tcc_host.c')+$dependencies+
            @('release/xl.dll')+$libs+@('-o',$binary)
        $buildExit=Invoke-XlirGccToLog $arguments "$outputPath/tcc-host-$level.build.log"
        foreach($mode in 0..5){
            $run=$null;$complete=$false;$log="$outputPath/tcc-host-$level-mode$mode.run.log"
            if(!$buildExit){
                $run=Invoke-XlirNativeToLog $binary @([string]$mode) $log
                $complete=$run -eq 0 -and [bool](Select-String -LiteralPath $log -SimpleMatch "Future retirement mode=${mode}: 25 real TCC lifetimes, 75 internal finalizers, independent aliases and zero live delta" -Quiet)
            }
            $rows+=[pscustomobject]@{Lane="tcc-host-$level-mode$mode";Build=$buildExit;Run=$run;Complete=$complete}
            Write-Output "tcc-host-$level-mode$mode build=$buildExit run=$run complete=$complete"
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    [IO.File]::WriteAllText("$outputPath/results.json",([pscustomobject]@{Inputs=$inputs;InputsUnchanged=$unchanged;Deterministic=$true;Lanes=$rows;ResidentClosure=$closure;
        Scope='Additive source-XLIR Future/class/closure real TCC retirement: six modes x25 on host O0/O2, plus three physical-plan lanes. Existing gate populations unchanged. Unknown waiter/task/native policies remain refused; no Linux, default backend or release/VFS acceptance.'} | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or @($rows | Where-Object {!$_.Complete}).Count){throw 'Future retirement failures collected'}
} finally {Pop-Location}
