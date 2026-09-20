param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,[string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve native Future mapping evidence'}
    $public=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    if(!$public.SourceXrt-or$public.RuntimeOnly){throw 'Current compiler and source runtime required'}
    $testSources=@('test/cases/xlir_native_future_map_contract.c','test/cases/xlir_future_callback_exclusive.c',
        'test/cases/xlir_network_callback_code_owner.c','test/cases/xlir_network_callback_code_module.c',
        'test/cases/xlir_network_callback_code_unload.c','test/cases/xlir_continuation_owner_test.h',
        'test/cases/xlir_synthetic_owner_edit.h')
    $fixtures=@('stdlib_net_fast_path','stdlib_net_send_file','stdlib_tls_server_loopback',
        'xlir_net_resolver_standalone','xlir_net_datagram_boundaries','xlir_net_buffer_command_lifetime')
    $files=Get-Item -LiteralPath (@($PSCommandPath,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source)+
        $testSources+@($fixtures|ForEach-Object {"test/cases/$_.xl"}))
    $inputs=@($public.Inputs)+@($public.Headers)+@($public.Objects)+@($files|ForEach-Object {
        [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($row in $inputs){if((Get-FileHash -LiteralPath $row.Path).Hash-ne$row.Sha256){throw "Stale native mapper input: $($row.Path)"}}
    New-Item -ItemType Directory -Path $outputPath,"$outputPath/original_tests"|Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    Copy-Item -LiteralPath (@($PSCommandPath)+$testSources) -Destination "$outputPath/original_tests"
    $objects=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
    $includes=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-pointer-sign','-Wno-format-truncation',
        '-Wno-missing-field-initializers','-DXL_STATIC_EMBED','-I.','-Irelease','-Ilib/tcc',"-I$includes")
    $libs=@('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    function Invoke-MapperProbe([string]$Binary,[string[]]$Arguments,[string]$Marker,[int]$Timeout=60000){
        $options=@{FilePath=$Binary;WindowStyle='Hidden';PassThru=$true;WorkingDirectory=$rootPath;
            RedirectStandardOutput="$Binary.out.log";RedirectStandardError="$Binary.err.log"}
        if($Arguments.Count){$options.ArgumentList=@($Arguments|ForEach-Object {'"'+$_+'"'})}
        $timer=[Diagnostics.Stopwatch]::StartNew();$process=Start-Process @options;$exitCode=$null
        try{
            while(!$process.WaitForExit(1000)){if($timer.ElapsedMilliseconds-ge$Timeout){$process.Kill();$process.WaitForExit();$exitCode=-999;break}}
            if($null-eq$exitCode){$process.Refresh();$exitCode=$process.ExitCode}
        }finally{$process.Dispose()}
        $text=[IO.File]::ReadAllText("$Binary.out.log")
        [pscustomobject]@{Exit=$exitCode;Complete=($exitCode-eq0-and$text.Contains($Marker));Log="$Binary.out.log";ErrorLog="$Binary.err.log"}
    }
    $rows=@();$generated=@()
    $contract="$outputPath/contract.exe"
    & gcc @flags -O0 test/cases/xlir_native_future_map_contract.c @objects @libs -o $contract *> "$contract.build.log"
    $build=$LASTEXITCODE;$run=$null
    if(!$build){$run=Invoke-MapperProbe $contract @() '25 IR/C/header-gated mutations' 240000}
    $rows+=[pscustomobject]@{Name='IR-contract';Build=$build;Run=$run;Complete=(!$build-and$run.Complete)}
    Write-Output "IR contract build=$build run=$($run.Exit)"
    $residentSources=@('src/runtime/xl6_code_lease.c','src/runtime/xl6_ownership_plan.c','src/runtime/xl6_capture.c',
        'src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c',
        'src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c',
        'src/runtime/xl6_task.c','src/runtime/xl6_generator.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_continuation_queue.c',
        'src/runtime/xl6_continuation_executor.c','src/runtime/xl6_continuation_future.c','src/runtime/xl6_future_payload.c')
    $refreshed=@('src/runtime/xl6_future_callback.c','src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c')
    $refreshedNames=@($refreshed|ForEach-Object {[IO.Path]::GetFileNameWithoutExtension($_)+'.o'})
    $otherObjects=@($objects|Where-Object {[IO.Path]::GetFileName($_)-notin$refreshedNames})
    foreach($compiler in @('gcc-O0','gcc-O2','tcc')){
        $exe="$outputPath/exclusive-$compiler.exe"
        if($compiler-eq'tcc'){
            & $Tcc -std=c11 -I. "-I$includes" test/cases/xlir_future_callback_exclusive.c @residentSources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll -lws2_32 -liphlpapi -lshell32 -lole32 -ladvapi32 -o $exe *> "$exe.build.log"
        }else{
            & gcc @flags "-$($compiler.Substring(4))" test/cases/xlir_future_callback_exclusive.c @refreshed @otherObjects @libs -o $exe *> "$exe.build.log"
        }
        $build=$LASTEXITCODE;$run=$null
        if(!$build){$run=Invoke-MapperProbe $exe @() '1000 cancellation/completion races; zero live delta'}
        $rows+=[pscustomobject]@{Name="exclusive-$compiler";Build=$build;Run=$run;Complete=(!$build-and$run.Complete)}
        Write-Output "exclusive-$compiler build=$build run=$($run.Exit)"
    }
    $cases=@(
        @{Fixture='stdlib_net_fast_path';Kind='bool';Defines=@()},
        @{Fixture='xlir_net_resolver_standalone';Kind='addresses';Defines=@('-DXLIR_NETWORK_RESOLVER')},
        @{Fixture='stdlib_tls_server_loopback';Kind='tls-context';Defines=@('-DXLIR_NETWORK_TLS_CONTEXT')})
    foreach($case in $cases){
        $prefix="$outputPath/$($case.Kind)"
        & $public.Driver "test/cases/$($case.Fixture).xl" emit $prefix *> "$prefix.emit.log"
        $emit=$LASTEXITCODE;$generated+=[pscustomobject]@{Kind=$case.Kind;Fixture=$case.Fixture;Emit=$emit;C="$prefix.c"}
        if($emit){continue}
        foreach($suffix in @('c','h','json')){$inputs+=[pscustomobject]@{Path="$prefix.$suffix";Sha256=(Get-FileHash -LiteralPath "$prefix.$suffix").Hash}}
        $defines=@($case.Defines)+@('-DXLIR_NETWORK_GENERATED_C="'+($prefix+'.c').Replace('\','/')+'"')
        if($case.Kind-ne'tls-context'){
            $exe="$prefix-owner.exe"
            & gcc @flags -O0 @defines test/cases/xlir_network_callback_code_owner.c @objects @libs -o $exe *> "$exe.build.log"
            $build=$LASTEXITCODE;$run=$null
            if(!$build){$run=Invoke-MapperProbe $exe @() '"Complete":true'}
            $rows+=[pscustomobject]@{Name="$($case.Kind)-owner";Build=$build;Run=$run;Complete=(!$build-and$run.Complete)}
        }
        foreach($level in @(0,2)){
            $exe="$prefix-unload-O$level.exe";$dll="$prefix-generated-O$level.dll";$import="$prefix-host-O$level.a"
            & gcc @flags "-O$level" test/cases/xlir_network_callback_code_unload.c @refreshed @otherObjects @libs "-Wl,--out-implib,$import" '-Wl,--export-all-symbols' -o $exe *> "$exe.build.log"
            $build=$LASTEXITCODE;$dllBuild=$null;$run=$null
            if(!$build){
                & gcc @flags "-O$level" @defines -shared -DXL6_BUILD_SHARED test/cases/xlir_network_callback_code_module.c $import '-Wl,--export-all-symbols' -o $dll *> "$dll.build.log"
                $dllBuild=$LASTEXITCODE
                if(!$dllBuild){$run=Invoke-MapperProbe $exe @($dll,$case.Kind) 'MEM_FREE and exact memory balance'}
            }
            $rows+=[pscustomobject]@{Name="$($case.Kind)-unload-O$level";Build=$build;DllBuild=$dllBuild;Run=$run;Complete=(!$build-and$dllBuild-eq0-and$run.Complete)}
            Write-Output "$($case.Kind)-unload-O$level build=$build dll=$dllBuild run=$($run.Exit)"
        }
    }
    $unchanged=!@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count
    $complete=$unchanged-and$rows.Count-eq12-and!@($rows|Where-Object {!$_.Complete}).Count-and
        $generated.Count-eq3-and!@($generated|Where-Object Emit -ne 0).Count
    $report=[pscustomobject]@{Complete=$complete;Inputs=$inputs;InputsUnchanged=$unchanged;Results=$rows;Generated=$generated;
        Scope='Verified native Future mapping ABI and plan dependencies; resident exclusive cancellation across GCC O0/O2 and TCC; unmodified generated TCP/resolver/TLS adapters in actual unloadable DLLs, four outcomes and separately retained class results after Close. Controlled real native values/source Promises; not engine ownership, live protocol teardown, whole-module collection or release/platform acceptance.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    if(!$complete){throw 'Native mapper failures retained for grouped repair'}
}finally{Pop-Location}
