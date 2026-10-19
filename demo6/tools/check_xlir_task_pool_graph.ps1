param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,[string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=(Get-Location).Path;$outputPath=[IO.Path]::GetFullPath($Directory)
if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior task-pool graph evidence'}
$public=Get-Content -Raw -LiteralPath (Join-Path $PublicDirectory 'build.json')|ConvertFrom-Json
$contracts=Get-Content -Raw -LiteralPath (Join-Path $ContractDirectory 'build.json')|ConvertFrom-Json
$runtime=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-eq'xrt_single_header_impl.o'})
if(!$public.SourceXrt-or$runtime.Count-ne1){throw 'Require current source XRT'}
$driver=Join-Path $ContractDirectory 'xlir_task_pool_graph_contract.exe'
$sources=@('test/cases/xlir_task_pool_graph_runtime.c','src/runtime/xl6_task.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_code_lease.c',
    'src/runtime/xl6_code_finalizer.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c',
    'src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_generator.c',
    'src/runtime/xl6_future_payload.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_frame_storage.c',
    'src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c',
    'src/runtime/xl6_continuation_future.c')
$inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
    @((Get-Item -LiteralPath (@($PSCommandPath,$Tcc,(Get-Command gcc).Source,'C:/Windows/System32/ucrtbase.dll',
        'test/cases/xlir_task_pool_graph_contract.c','test/cases/xlir_module_executor_mixed.xl')+$sources))|
        ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
foreach($row in $inputs){if((Get-FileHash -LiteralPath $row.Path).Hash-ne$row.Sha256){throw "Stale task-pool graph input: $($row.Path)"}}
New-Item -ItemType Directory -Path $outputPath|Out-Null
& $driver *> "$outputPath/ir.log";$irExit=$LASTEXITCODE
$irComplete=$irExit-eq0-and[bool](Select-String -LiteralPath "$outputPath/ir.log" -SimpleMatch 'Task pool graph IR: 4 partitions, 24 C/header-gated mutations' -Quiet)
$strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
    '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation','-I.')
$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$lanes=@()
foreach($compiler in @('gcc-O0','gcc-O2','tcc')){
    $binary="$outputPath/$compiler.exe"
    if($compiler-eq'tcc'){& $Tcc -std=c11 -I. @sources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$compiler.build.log"}
    else{& gcc @strict "-$($compiler.Substring(4))" @sources $runtime[0].Path @libs -o $binary *> "$outputPath/$compiler.build.log"}
    $build=$LASTEXITCODE;$run=$null;$complete=$false
    if($build-eq0){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath -RedirectStandardOutput "$outputPath/$compiler.out.log" -RedirectStandardError "$outputPath/$compiler.err.log"
        $null = $process.Handle
        if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
        $process.Dispose()
        $complete=$run-eq0-and[bool](Select-String -LiteralPath "$outputPath/$compiler.out.log" -SimpleMatch 'Task pool wrapper: 280 balanced lifetimes, 240 exact queued raw/language graphs, 520 real preparations, accepted execution, joined native owner and both Finish orders' -Quiet)
    }
    $lanes+=[pscustomobject]@{Compiler=$compiler;Build=$build;Run=$run;Complete=$complete}
    Write-Output "$compiler build=$build run=$run complete=$complete"
}
$unchanged=!@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count
$report=[pscustomobject]@{Inputs=$inputs;InputsUnchanged=$unchanged;IrExit=$irExit;IrComplete=$irComplete;Lanes=$lanes;
    Scope='Explicit task-pool owning ABI verified independently from execution and retirement fields. Direct real wrapper/native lifecycle tests, exact physical queued Job/frame graphs and accepted execution. GCC O0/O2 compiles every listed resident source at that level against the validated source XRT object; TCC compiles source XRT. Not itself a generated-module unload or mixed producer-cycle proof.'}
[IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
if(!$unchanged-or!$irComplete-or@($lanes|Where-Object {!$_.Complete}).Count){throw 'Task-pool graph batch collected failures'}
