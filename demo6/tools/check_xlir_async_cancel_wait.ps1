param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$CellContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve previous cancellation evidence'}
    $public=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $contracts=Get-Content (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw|ConvertFrom-Json
    $runtime=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-eq'xrt_single_header_impl.o'})
    if(!$public.SourceXrt-or$runtime.Count-ne1){throw 'Current source XRT required'}
    $resident=@('src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_continuation_queue.c',
        'src/runtime/xl6_continuation_executor.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_closure.c',
        'src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_task.c',
        'src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c',
        'src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c')
    $original=@('test/cases/xlir_async_cancel_watch_runtime.c','test/cases/xlir_async_cancel_watch_host.c',
        'test/cases/xlir_async_cancel_watch_sync.c','test/cases/xlir_async_source_transport.c')+$resident
    $interrupt=@('test/cases/xlir_async_cancel_wait_runtime.c','test/cases/xlir_async_cancel_wait_host.c',
        'src/runtime/xl6_continuation_future.c')+$resident
    $files=@(Get-Item -LiteralPath $PSCommandPath,$ContractDriver,$CellContractDriver,$Tcc,
        'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,
        'test/cases/xlir_async_cancel_watch_contract.c','test/cases/xlir_async_cancel_watch.xl',
        'test/cases/xlir_cell_cancel_race_contract.c','test/cases/xlir_cell_cancel_race.xl')+
        @(Get-Item -LiteralPath $original)+@(Get-Item -LiteralPath $interrupt)
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if(@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count){throw 'Stale cancellation inputs'}
    New-Item -ItemType Directory -Path $outputPath|Out-Null
    $emission=@();$deterministic=$true
    foreach($fixture in @('watch','cell')){
        $name=if($fixture-eq'watch'){'xlir_async_cancel_watch'}else{'xlir_cell_cancel_race'}
        $driver=if($fixture-eq'watch'){$ContractDriver}else{$CellContractDriver}
        foreach($repeat in @('','.repeat')){
            $prefix="$outputPath/$name$repeat"
            & $driver "$prefix.c" "$prefix.h" "$prefix.json" *> "$prefix.emit.log"
            $exitCode=$LASTEXITCODE;$log=[IO.File]::ReadAllText("$prefix.emit.log")
            $matches=if($fixture-eq'watch'){
                [regex]::Matches($log,'async cancel watch: canonical watch transport, once-only cancellation, ([1-9][0-9]*) unread owner handoffs, ([1-9][0-9]*) C/header-gated mutations')
            }else{
                [regex]::Matches($log,'cell cancel race: 2 source awaits, 3 normal/exceptional suspension states, 1 managed cell resident, ([1-9][0-9]*) C/header-gated mutations')
            }
            $emission+=[pscustomobject]@{Fixture=$fixture;Pass=$repeat;Exit=$exitCode;Complete=$matches.Count-eq1}
        }
        foreach($suffix in @('c','h','json','h.bind.h')){
            if(!(Test-Path "$outputPath/$name.$suffix")-or!(Test-Path "$outputPath/$name.repeat.$suffix")-or
                (Get-FileHash "$outputPath/$name.$suffix").Hash-ne(Get-FileHash "$outputPath/$name.repeat.$suffix").Hash){$deterministic=$false}
        }
    }
    $includes=@("-I$([IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include')))","-I$outputPath",'-I.')
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $lanes=@()
    foreach($variant in @('original','interrupt')){foreach($configuration in @('gcc-O0','gcc-O2','tcc')){
        $name="$variant-$configuration";$binary="$outputPath/$name.exe"
        $sources=if($variant-eq'original'){$original}else{$interrupt}
        $defines=if($variant-eq'original'){@(
            (New-XlirCStringDefine 'XLIR_CANCEL_WATCH_GENERATED_C' 'xlir_async_cancel_watch.c'),
            (New-XlirCStringDefine 'XLIR_CANCEL_WATCH_GENERATED_BIND' 'xlir_async_cancel_watch.h.bind.h'))}else{@()}
        if($configuration-eq'tcc'){
            & $Tcc -std=c11 @includes @defines @sources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$name.build.log"
        }else{
            & gcc @strict "-$($configuration.Substring(4))" @includes @defines @sources $runtime[0].Path @libs -o $binary *> "$outputPath/$name.build.log"
        }
        $build=$LASTEXITCODE;$run=$null;$complete=$false
        if($build-eq0){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath `
                -RedirectStandardOutput "$outputPath/$name.out.log" -RedirectStandardError "$outputPath/$name.err.log"
            $null = $process.Handle
            $deadline=[DateTime]::UtcNow.AddSeconds(180)
            while(!$process.WaitForExit(1000)-and[DateTime]::UtcNow-lt$deadline){}
            if(!$process.HasExited){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
            $process.Dispose();$log=[IO.File]::ReadAllText("$outputPath/$name.out.log")
            $marker=if($variant-eq'original'){'async cancel watch: 34560 executions, 20520 cancellation requests, 34560 publication observers, [1-9][0-9]* balanced allocations'}else{
                'Async cancel wait: 2106 generated interruptions, 300 three-thread source/cancel/executor races, 2106 allocation-free armed-OOM admissions, 2106 native observer reentries; shared source untouched, once-only cancellation, exact finally/outcome/barrier and real retired-frame pins, balanced memory'}
            $complete=$run-eq0-and[regex]::Matches($log,$marker).Count-eq1
        }
        $lanes+=[pscustomobject]@{Name=$name;Build=$build;Run=$run;Complete=$complete}
        Write-Output "$name build=$build run=$run complete=$complete"
    }}
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        Emission=$emission;Deterministic=$deterministic;Lanes=$lanes;
        OriginalPerProgram=[pscustomobject]@{Executions=34560;CancelRequests=20520;PublicationObservers=34560};
        InterruptPerProgram=[pscustomobject]@{GeneratedInterruptions=2106;ThreeThreadRaces=300;ArmedOomAdmissions=2106;NativeReentries=2106};
        Scope='All original cancel-watch runtime populations under current-source GCC O0/O2/TCC, with original raising-cleanup and producer-isolation combinations. Added explicit owning-host interruption with shared source, once-only finally awaits, real retired-frame holds and concurrent source/cancel/executor. Both cancellation IR fixtures deterministically emitted with all negative cases; historical cell-race runtime/CLI population is not executed by this gate. This API is not dependency-SCC authorization, combined module unloading, Linux or release acceptance.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged-or!$deterministic-or@($emission|Where-Object {$_.Exit-ne0-or!$_.Complete}).Count-or
        @($lanes|Where-Object {$_.Build-ne0-or$_.Run-ne0-or!$_.Complete}).Count){throw 'Cancellation batch collected failures'}
}finally{Pop-Location}
